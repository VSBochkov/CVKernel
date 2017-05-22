#include "fireweight.h"         // Подключение файла fireweight.h для определения методов
#include "rfiremaskingmodel.h"  // Подключение файла firebbox.h для использования класса DataRFireMM
#include <omp.h>                // Подключение файла omp.h для использования директив OpenMP

// Определение конструктора класса FireWeightDistribData
// Входящие параметры:
//   base - базовая матрица весов
//   flame_src - матрица сигнала нахождения источника пламени
//   flame_pixel_cnt - количество пикселей сигнала пламени
FireWeightDistribData::FireWeightDistribData(cv::Mat base, cv::Mat flame_src, ulong flame_pixel_cnt)
{
    weights   = base;               // Инициализируем матрицу весов
    flame     = flame_src;          // Инициализируем матрицу очагов пожара
    pixel_cnt = flame_pixel_cnt;    // Инициализируем количество пикселей пламени
}

// Определение конструктора класса FireWeightDistribParams
// Входящий параметр json_obj - описание параметров модели выявления статического сигнала пламени в формате JSON
FireWeightDistribParams::FireWeightDistribParams(QJsonObject &json_obj) : CVNodeParams(json_obj)       // Вызов конструктора базового класса
{
    QJsonObject::iterator iter;  // Объявление итератора по JSON объекту
    weight_thr = (iter = json_obj.find("weight_thr")) == json_obj.end() ? 0.3 : iter.value().toDouble();   // Инициализация численного значения параметра weight_thr
    time_thr   = (iter = json_obj.find("time_thr")) == json_obj.end()   ? 1.5 : iter.value().toDouble();   // Инициализация численного значения параметра time_thr
}

// Определение метода clear_history класса FireWeightDistribHistory
void FireWeightDistribHistory::clear_history()
{
    if (!base.empty())      // Если базовая матрица весов инициализирована
        base.release();     // То удаляем ее
    if (!timings.empty())   // Если базовая матрица времени жизни сигнала инициализирована
        timings.release();  // То удаляем ее
}

// Определение метода compute класса FireWeightDistrib
// Входящий параметр process_data - структура данных видеоаналитики
// Возвращаемое значение - указатель на результат процессорного узла инициализированный классам FireWeightDistribData
QSharedPointer<CVKernel::CVNodeData> FireWeightDistrib::compute(QSharedPointer<CVKernel::CVProcessData> process_data)
{
    std::vector<object> fire_bboxes = dynamic_cast<FireBBoxData *>(process_data->data["FireBBox"].data())->fire_bboxes; // Берем список прямоугольных областей динамического сигнала
    cv::Mat  fire_mask = dynamic_cast<DataRFireMM *>(process_data->data["RFireMaskingModel"].data())->mask;             // Берем сигнал извлечения пламени по цветовой модели
    cv::Mat  flame = cv::Mat::zeros(fire_mask.rows, fire_mask.cols, CV_8UC1);    // Создаем матрицу статического сигнала пламени и инициализируем ее нулем
    QSharedPointer<FireWeightDistribParams> params = process_data->params[metaObject()->className()].staticCast<FireWeightDistribParams>();     // Берем параметры процессорного узла
    QSharedPointer<FireWeightDistribHistory> history = process_data->history[metaObject()->className()].staticCast<FireWeightDistribHistory>(); // Берем историю процессорного узла

    if (history->base.empty()) {    // Если базовая матрица весов не инициализирована
        history->base = cv::Mat::zeros(fire_mask.rows, fire_mask.cols, CV_32FC1);   // То инициализируем ее
    }
    if (history->timings.empty()) { // Если базовая матрица времени жизни сигнала не инициализирована
        history->timings = cv::Mat::zeros(fire_mask.rows, fire_mask.cols, CV_8U);   // То инициализируем ее
    }

    uchar* fire_mask_matr = fire_mask.data; // Берем адрес матрицы сигнала пламени по цвету для прямого доступа к элементам
    uchar* flame_matr = flame.data;         // Берем адрес матрицы статического сигнала пламени для прямого доступа к элементам
    double frames_per_second = process_data->fps > 15. ? process_data->fps : 15.;    // Берем значение количества кадров в секунду
    float window = (float) (frames_per_second) / (frames_per_second + 1.);           // Вычисляем окно усреднения
    uchar* timings_matr = history->timings.data;  // Берем адрес матрицы времени жизни статического сигнала для прямого доступа к элементам
    float* base_matr = (float*) history->base.data; // Берем адрес базовой матрицы весов для прямого доступа к элементам
    ulong flame_pixel_cnt = 0;  // Инициализируем количество пикселей статического сигнала пламени нулем

    if (!params->draw_overlay) {    // Если не отображаем данные в оверлей
    #pragma omp parallel for        // Распараллеливаем цикл ниже
        for (int i = 0; i < fire_mask.rows; ++i) {    // Организуем цикл по строкам матрицы кадра
            for (int j = 0; j < fire_mask.cols; ++j) {  // Организуем цикл по столбцам матрицы кадра
                int id = i * fire_mask.cols + j;    // Отображаем внутренний и внешний индексы на индекс для прямого доступа
                base_matr[id] = window * base_matr[id] + (get_bbox(i, j, fire_bboxes) ? fire_mask_matr[id] / (frames_per_second + 1.) : 0.); // Вычисляем скользящее среднее базового сигнала
                if (base_matr[id] > params->weight_thr) {   // Если значение базового сигнала пикселя выше порога
                    timings_matr[id] = std::min(timings_matr[id] + 1, 0xff);    // То увеличиваем значение времени жизни сигнала
                    if (timings_matr[id] > frames_per_second * params->time_thr) {  // Если время жизни сигнала выше порога
                        flame_matr[id] = 1; // То инициализируем сигнал статического пламени в пикселе значением 1
                        flame_pixel_cnt++;  // Увеличиваем количество пикселей сигнала пламени
                    }
                } else  // Иначе
                    timings_matr[id] = 0;  // Сбрасываем сигнал времени жизни пламени в пикселе
            }
        }
    } else {    // Если отображаем данные в оверлей
        cv::Mat  overlay   = CVKernel::video_data[process_data->video_name].overlay;    // Берем кадр оверлея
        uchar* overlay_matr = overlay.data; // Берем адрес матрицы оверлея для прямого доступа к элементам
    #pragma omp parallel for    // Распараллеливаем цикл ниже
        for (int i = 0; i < fire_mask.rows; ++i) {  // Организуем цикл по строкам матрицы кадра
            for (int j = 0; j < fire_mask.cols; ++j) {  // Организуем цикл по столбцам матрицы кадра
                int id = i * fire_mask.cols + j;    // Отображаем внутренний и внешний индексы на индекс для прямого доступа
                base_matr[id] = window * base_matr[id] + (get_bbox(i, j, fire_bboxes) ? fire_mask_matr[id] / (frames_per_second + 1.) : 0.); // Вычисляем скользящее среднее базового сигнала
                if (base_matr[id] > params->weight_thr) {   // Если значение базового сигнала пикселя выше порога
                    timings_matr[id] = std::min(timings_matr[id] + 1, 0xff);     // То увеличиваем значение времени жизни сигнала
                    if (timings_matr[id] > frames_per_second * params->time_thr) {    // Если время жизни сигнала выше порога
                        overlay_matr[id * 3]     = 0xff;// Отрисовываем пиксель
                        overlay_matr[id * 3 + 1] = 0;   // Статического сигнала пламени синим цветом
                        flame_matr[id] = 1;             // Инициализируем сигнал статического пламени в пикселе значением 1
                        flame_pixel_cnt++;              // Увеличиваем количество пикселей сигнала пламени
                    } else { // Иначе
                        overlay_matr[id * 3]     = 0;   // Отрисовываем пиксель с малым временем жизни
                        overlay_matr[id * 3 + 1] = 0xff;// Зеленым цветом
                    }
                    overlay_matr[id * 3 + 2] = 0;   // Сбрасываем красную компоненту пикселя оверлея
                } else  // Иначе
                    timings_matr[id] = 0;  // Сбрасываем сигнал времени жизни пламени в пикселе
            }
        }
    }

    QSharedPointer<FireWeightDistribData> result(new FireWeightDistribData(history->base, flame, flame_pixel_cnt)); // Создаем указатель на результат процессорного узла
    return result;      // Возвращаем указатель на результат процессорного узла
}
