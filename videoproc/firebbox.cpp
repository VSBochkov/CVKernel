#include "firebbox.h"           // Подключение файла firebbox.h для определения методов
#include "firevalidation.h"     // Подключение файла firevalidation.h для использования класса FireValidationData
#include "rfiremaskingmodel.h"  // Подключение файла rfiremaskingmodel для использования класса DataRFireMM
#include <opencv2/imgproc.hpp>  // Подключение файла imgproc.hpp для использования методов обработки изображения библиотеки OpenCV
#include <opencv2/core.hpp>     // Подключение файла core.hpp для использования типов данных библиотеки OpenCV

// Определение конструктора класса FireBBoxData
// Входящий параметр bboxes - список прямоугольных областей объектов
FireBBoxData::FireBBoxData(std::vector<object> &bboxes)
{
    fire_bboxes = bboxes;   // присваивание прямоугольных областей огня
    std::sort(fire_bboxes.begin(), fire_bboxes.end(),       // сортировка
        [](const object &a, const object &b) -> bool {      // прямоугольников
            return a.rect.area() < b.rect.area();           // по убыванию площади
        }
    );
}

// Определение конструктора класса FireBBoxParams
// Входящий параметр json_obj - описание параметров модели сегментации динамического сигнала пламени в формате JSON
FireBBoxParams::FireBBoxParams(QJsonObject& json_obj) : CVKernel::CVNodeParams(json_obj)    // Вызов конструктора базового класса
{
    QJsonObject::iterator iter; // Объявление итератора по JSON объекту
    min_area_percent = (iter = json_obj.find("min_area_percent")) == json_obj.end() ? 10. : iter.value().toDouble();  // Инициализация численного значения параметра min_area_percent
    intersect_thresh = (iter = json_obj.find("intersect_thresh")) == json_obj.end() ? 0.4 : iter.value().toDouble();  // Инициализация численного значения параметра intersect_thresh
    dtime_thresh     = (iter = json_obj.find("dtime_thresh"))     == json_obj.end() ? 0.1 : iter.value().toDouble();  // Инициализация численного значения параметра dtime_thresh
}

// Определение метода object_tracking класса FireBBox
// Входящие параметры:
//    proc_mask - целевой сигнал подлежащий сегментации
//    overlay - кадр с наложением результатов анализа
//    pixel_cnt - количество пикселей целевого сигнала
//    bbox_color - цвет отображения прямоугольных областей
//    process_data - структура данных видеоаналитики
// Возвращаемое значение - список объектов
std::vector<object> FireBBox::object_tracking(cv::Mat proc_mask, cv::Mat overlay, ulong pixel_cnt,
                                            cv::Scalar bbox_color, CVKernel::CVProcessData &process_data)
{
    QSharedPointer<FireBBoxParams> params = process_data.params[metaObject()->className()].staticCast<FireBBoxParams>();      // Берем параметры алгоритма сегментации
    QSharedPointer<FireBBoxHistory> history = process_data.history[metaObject()->className()].staticCast<FireBBoxHistory>();  // Берем историю алгоритма сегментации
    std::vector<std::vector<cv::Point>> contours;                                            // Объявляем массив контуров
    cv::findContours(proc_mask.clone(), contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE); // Ищем контуры в целевом сигнале
    std::vector<cv::Moments> mu(contours.size());                                            // Объявляем массив моментов контуров
    for (uint i = 0; i < contours.size(); i++)                                               // Организуем цикл по всем контурам
        mu[i] = moments(contours[i], false);                                                 // Заполняем массив моментов

    auto dist2 = [&](double x1, double y1, double x2, double y2) {  // Определяем функцию квадрата расстояния между точками в двумерном Евклидовом пространстве
        return (x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1);       // Возврат расстояния между точками
    };

    /** Алгоритм гравитации **/
    for (uint i = 0; i < contours.size(); ++i) {        // Организуем цикл по контурам
        double x1 = (double) mu[i].m10 / mu[i].m00;     // Нахождение центра масс
        double y1 = (double) mu[i].m01 / mu[i].m00;     // контура под индексом i
        for (uint j = 0; j < contours.size(); ++j) {    // Организуем внутренний цикл по контурам
            if (i == j) continue;   // Если номер итерации первого цикла совпадает с номером второго то пропустить итерацию
            double x2 = (double) mu[j].m10 / mu[j].m00; // Вычисление центра масс
            double y2 = (double) mu[j].m01 / mu[j].m00; // контура под индексом j
            double grav = (double) (cv::contourArea(contours[i]) * cv::contourArea(contours[j])) / dist2(x1, y1, x2, y2); // Вычисление значения гравитации
            if (grav > history->grav_thresh) {  // Если значение гравитации выше порога
                cv::line(proc_mask, cv::Point((int) x1, (int) y1), cv::Point((int) x2, (int) y2), 0xff, 2); // То объединяем контуры на маске целевого сигнала линией
            }
        }
    }

    contours.clear();   // Очищаем массив контуров
    cv::findContours(proc_mask, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE); // Вычисляем массив контуров после алгоритма гравитации

    /** Фильтруем контуры по площади обрамляющих прямоугольников **/
    double area_thresh = params->min_area_percent / 100. * pixel_cnt;   // Устанавливаем порог площади контуров
    std::vector<cv::Rect> bboxes;                                       // Объявляем массив прямоугольных областей
    for (uint i = 0; i < contours.size(); ++i) {    // Организуем цикл по контурам
        auto bbox = cv::boundingRect(contours[i]);  // Нахождение прямоугольной области описывающий контур
        if (bbox.area() > area_thresh)              // Если площадь прямоугольника больше порога
            bboxes.push_back(bbox);                 // Помещаем прямоугольник в массив прямоугольных областей
    }

    /** Объединение значительно пересекающихся прямоугольников **/
    std::vector<cv::Rect> merged_bboxes;            // Объявление массива объединенных прямоугольных областей
    for (uint i = 0; i < bboxes.size(); ++i) {      // Организация цикла по прямоугольным областям
        cv::Rect merge_rect;                        // Объявление объединенной прямоугольной области
        for (uint j = 0; j < bboxes.size(); ++j) {  // Организация внутреннего цикла по прямоугольным областям
            if (i == j) continue;                   // Если индексы внутреннего и внешнего циклов совпадают то пропустить итерацию
            cv::Rect intersect = intersection(bboxes[i], bboxes[j]);    // Нахождение пересечения двух прямоугольников
            if (std::max((double) intersect.area() / bboxes[i].area(), (double) intersect.area() / bboxes[j].area()) > params->intersect_thresh) // Находим максимальное значение между делениями площадей пересечения и площадью каждой из областей и если максимум больше порога по доле пересечения
                merge_rect = rect_union(bboxes[i], bboxes[j]);  // То инициализируем слитый прямоугольник объединением рассматриваемых прямоугольных областей
        }
        merged_bboxes.push_back(merge_rect.area() > 0 ? merge_rect : bboxes[i]);    // Вставляем в конец вектора слитых прямоугольных областей объединенный прямоугольник если он инициализирован или исходный
    }

    /** Обновление прямоугольных областей у объектов (Слежение за объектами) **/
    bboxes = merged_bboxes;                         // Присваиваем массиву прямоугольных областей вычисленный массив объекдиненных прямоугольных областей
    if (!history->base_objects.empty()) {           // Если список базовых объектов не пуст
        for (auto& bbox : bboxes) {                 // Организуем цикл по массиву прямоугольных областей
            double best_rel_koeff = 0.;             // Сбрасываем максимальное значение соответствия
            object* target_obj = nullptr;           // Инициализируем указатель на целевую область нулем
            for (auto& base_bbox : history->base_objects) {  // Организуем цикл по массиву базовых объектов
                if (intersection(bbox, base_bbox.rect).area() > params->intersect_thresh) { // Если площадь пересечения прямоугольников текущего кадра и базового больше порога
                    double relation_koeff = bbox.area() > base_bbox.rect.area() ? ((double)base_bbox.rect.area() / (double)bbox.area()) : ((double)bbox.area() / (double)base_bbox.rect.area()); // То определяем коэффициент отношения делением площади минимального на площадь максимального из двух рассматриваемых прямоугольников
                    if (relation_koeff > best_rel_koeff) {  // Если текущий коэффициент отношения площадей больше максимального
                        best_rel_koeff = relation_koeff;    // То инициализируем максимальный коэффициент значением текущего
                        target_obj = &base_bbox;            // Инициализируем целевую прямоугольную область текущей
                    }
                }
            }
            if (best_rel_koeff > 0. and target_obj != nullptr) {  // Если найдена целевая прямоугольная область
                target_obj->last_fnum = process_data.frame_num;   // То инициализируем последний номер кадра целевого объекта номером текущего кадра
                target_obj->rect = bbox;                          // Инициализируем прямоугольную область целевого объекта текущим прямоугольником
            } else                                                // Иначе
                history->base_objects.push_back(object(bbox, process_data.frame_num));   // Создаем новый объект и помещаем его в список базовых
        }
    } else {    // Если список базовых объектов пуст
        history->base_objects.reserve(bboxes.size());   // Резервируем в списке базовых объектов память под количество найденных прямоугольных областей
        for (cv::Rect& bbox : bboxes)                   // Организуем цикл по прямоугольным областям
            history->base_objects.push_back(object(bbox, process_data.frame_num));    // Создаем новый объект и помещаем его в список базовых
    }

    /** Удаление устаревших объектов **/
    std::vector<object> result_objects;                  // Объявление результирующего списка объектов
    for (auto base_bbox_it = history->base_objects.begin(); base_bbox_it != history->base_objects.end();)   // Организуем цикл по базовым объектам
    {
        if ((process_data.frame_num - base_bbox_it->last_fnum) <= params->dtime_thresh * std::max(process_data.fps, 25.))   // Если с момента последнего обновления номера кадра для объекта прошло времени меньше порога
        {
            if (params->draw_overlay)   // То если отрисовываем оверлей
            {
                cv::rectangle(overlay, base_bbox_it->rect, bbox_color, 1); // Прорисовываем прямоугольную область в кадре
            }

            result_objects.push_back(*base_bbox_it); // Помещаем объект в список result_objects
            base_bbox_it++; // Переходим к следующему базовому объекту
        }
        else        // Иначе если прямоугольник не обновлялся по времени больше порога
        {
            history->base_objects.erase(base_bbox_it);  // То удаляем его из списка базовых
        }
    }

    if (result_objects.empty())   // Если список объектов пуст
        return result_objects;    // То возвращаем его

    /** Пересчет порога гравитации **/
    double aver_bbox_square = 0.;   // Определяем среднее значение площади прямоугольников
    for (auto& base_bbox : result_objects)  // Организуем цикл по всем объектам
        aver_bbox_square += base_bbox.rect.area();  // Суммируем площадь прямоугольников у объектов
    aver_bbox_square /= result_objects.size();  // Делим сумму на количество объектов и получаем среднее значение

    if (aver_bbox_square < params->min_area_percent / 10000. * (proc_mask.rows * proc_mask.cols) && history->grav_thresh > 1) // Если среднее меньше нижнего порога
        history->grav_thresh -= 0.5;    // Уменьшаем порог
    else if (aver_bbox_square > (proc_mask.rows * proc_mask.cols) / 50.)    // Иначе если среднее больше верхнего порога
        history->grav_thresh += 0.5;    // Увеличиваем порог

    return result_objects;    // Возвращаем список объектов
}

// Определение метода compute класса FireBBox
// Входящий параметр process_data - структура данных видеоаналитики
// Возвращаемое значение - указатель на результат процессорного узла инициализированный классам FireBBoxData
QSharedPointer<CVKernel::CVNodeData> FireBBox::compute(QSharedPointer<CVKernel::CVProcessData> process_data)
{
    cv::Mat fire_valid = dynamic_cast<FireValidationData *>(process_data->data["FireValidation"].data())->mask;     // Берем маску динамического сигнала из структуры
    ulong rgb_pixel_cnt = dynamic_cast<DataRFireMM *>(process_data->data["RFireMaskingModel"].data())->pixel_cnt;   // Берем количество пискселей сигнала пламени выявленного по цвету из структуры
    cv::Mat overlay = CVKernel::video_data[process_data->video_name].overlay;                                       // Берем матрицу оверлея
    std::vector<object> bboxes = object_tracking(fire_valid.clone(), overlay, rgb_pixel_cnt, cv::Scalar(0, 0xff, 0), *process_data);    // Запускаем метод сегментации и слежения над динамическим сигналом
    QSharedPointer<FireBBoxData> result(new FireBBoxData(bboxes));  // Создаем указатель на результат процессорного узла
    return result;  // Возвращаем указатель на результат процессорного узла
}
