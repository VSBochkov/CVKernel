#include "rfiremaskingmodel.h"      // Подключение файла rfiremaskingmodel.h для определения методов
//#include <opencv2/imgproc.hpp>      // Подключение файла imgproc.hpp для использования
#include <omp.h>                    // Подключение файла omp.h для использования директив OpenMP

// Определение конструктора класса DataRFireMM
// Входящие параметры
//   rows - высота кадра
//   cols - ширина кадра
DataRFireMM::DataRFireMM(int rows, int cols)
{
    mask = cv::Mat::zeros(rows, cols, cv::DataType<uchar>::type); // Создание матрицы маски
    pixel_cnt = 0;  // Сброс значения количество пикселей сигнала
}

// Определение конструктора класса RFireHistory
RFireHistory::RFireHistory()
{
    ema_rgb_extracted = 1.; // Осуществление выявление сигнала по цветовой модели RGB
}

// Определение метода класса clear_history класса RFireHistory
void RFireHistory::clear_history()
{
    ema_rgb_extracted = 1.; // Осуществление выявление сигнала по цветовой модели RGB
}

// Определение конструктора класса FireValidationParams
// Входящий параметр json_obj - описание параметров модели выявления цветового сигнала пламени в формате JSON
RFireParams::RFireParams(QJsonObject& json_obj) : CVKernel::CVNodeParams(json_obj)  // Вызов конструктора базового класса
{
    QJsonObject::iterator iter;  // Объявление итератора по JSON объекту
    rgb_area_threshold = (iter = json_obj.find("rgb_area_threshold")) == json_obj.end() ? 0.02 : iter.value().toDouble();   // Инициализация численного значения параметра rgb_area_threshold
    rgb_freq_threshold = (iter = json_obj.find("rgb_freq_threshold")) == json_obj.end() ? 0.2 : iter.value().toDouble();    // Инициализация численного значения параметра rgb_freq_threshold
}

// Определение метода compute класса RFireMaskingModel
// Входящий параметр process_data - структура данных видеоаналитики
// Возвращаемое значение - указатель на результат процессорного узла инициализированный классом DataRFireMM
QSharedPointer<CVKernel::CVNodeData> RFireMaskingModel::compute(QSharedPointer<CVKernel::CVProcessData> process_data) {
    cv::Mat frame = CVKernel::video_data[process_data->video_name].frame;   // Берем кадр из структуры
    QSharedPointer<RFireParams> params = process_data->params[metaObject()->className()].staticCast<RFireParams>(); // Берем параметры узла из структуры
    QSharedPointer<RFireHistory> history = process_data->history[metaObject()->className()].staticCast<RFireHistory>(); // Берем историю узла из структуры
    QSharedPointer<DataRFireMM> result(new DataRFireMM(frame.rows, frame.cols)); // Создаем указатель на результат процессорного узла
    uchar* frame_matr = frame.data;         // Берем адрес кадра в памяти для прямого доступа к элементам
    uchar* res_matr   = result->mask.data;  // Берем адрес цветового сигнала пламени в памяти для прямого доступа к элементам

    ulong rgb_pixel_cnt = 0;        // Сбрасываем счетчик количества пикселей извлеченных по модели RGB
    if (!params->draw_overlay) {    // Если не отрисовываем оверлей
    #pragma omp parallel for        // То распараллеливаем цикл ниже
        for (int i = 0; i < frame.rows; ++i) {      // Организуем цикл по строкам матрицы кадра
            for (int j = 0; j < frame.cols; ++j) {  // Организуем цикл по столбцам матрицы кадра
                uchar b = frame_matr[(i * frame.cols + j) * 3], g = frame_matr[(i * frame.cols + j) * 3 + 1], r = frame_matr[(i * frame.cols + j) * 3 + 2]; // Берем значения синего зеленого и красного цвета в пикселе
                if (r > g && g > b && r > 190 && g > 100 && b < 140)    // Если цвета удовлетворяют условию модели RGB
                {
                    rgb_pixel_cnt++;                    // То увеличиваем счетчик извлеченных пикселей сигнала по цветовой схеме RGB
                    result->pixel_cnt++;                // Увеличиваем счетчик цветового сигнала пламени
                    res_matr[i * frame.cols + j] = 1;   // Инициализируем пиксель сигнала значением 1
                }
                // Иначе если цвета не удовлетворяют условию модели RGB
                // и в течении секунды RGB модель извлекает недостаточное количество пикселей пламени
                // но цвета удовлетворяют условию модели Grayscale
                else if (history->ema_rgb_extracted < params->rgb_freq_threshold and (float)(r * 0.299 + g * 0.587 + b * 0.114) > 150.)
                {
                    result->pixel_cnt++;                // Увеличиваем счетчик цветового сигнала пламени
                    res_matr[i * frame.cols + j] = 1;   // Инициализируем пиксель сигнала значением 1
                }
            }
        }
    } else {    // Если отрисовываем оверлей
        cv::Mat overlay = CVKernel::video_data[process_data->video_name].overlay;   // То берем матрицу overlay
        uchar* over_matr  = overlay.data;                                           // Берем адрес матрицы для прямого доступа к элементам
    #pragma omp parallel for    // Распараллеливаем цикл ниже
        for (int i = 0; i < frame.rows; ++i) {      // Организуем цикл по строкам матрицы кадра
            for (int j = 0; j < frame.cols; ++j) {  // Организуем цикл по столбцам матрицы кадра
                uchar b = frame_matr[(i * frame.cols + j) * 3], g = frame_matr[(i * frame.cols + j) * 3 + 1], r = frame_matr[(i * frame.cols + j) * 3 + 2]; // Берем значения синего зеленого и красного цвета в пикселе
                if (r > g && g > b && r > 190 && g > 100 && b < 140)    // Если цвета удовлетворяют условию модели RGB
                {
                    rgb_pixel_cnt++;                    // То увеличиваем счетчик извлеченных пикселей сигнала по цветовой схеме RGB
                    result->pixel_cnt++;                // Увеличиваем счетчик цветового сигнала пламени
                    res_matr[i * frame.cols + j] = 1;   // Инициализируем пиксель сигнала значением 1
                    over_matr[(i * frame.cols + j) * 3 + 1] = 0;    // Выделяем пискель
                    over_matr[(i * frame.cols + j) * 3 + 2] = 0xff; // на оверлее
                }
                // Иначе если цвета не удовлетворяют условию модели RGB
                // и в течении секунды RGB модель извлекает недостаточное количество пикселей пламени
                // но цвета удовлетворяют условию модели Grayscale
                else if (history->ema_rgb_extracted < params->rgb_freq_threshold and (float)(r * 0.299 + g * 0.587 + b * 0.114) > 150.)
                {
                    result->pixel_cnt++;                // Увеличиваем счетчик цветового сигнала пламени
                    res_matr[i * frame.cols + j] = 1;   // Инициализируем пиксель сигнала значением 1
                    over_matr[(i * frame.cols + j) * 3 + 1] = 0;    // Выделяем пискель
                    over_matr[(i * frame.cols + j) * 3 + 2] = 0xff; // на оверлее
                }
            }
        }
    }

    int rgb_enough_criteria = (double)rgb_pixel_cnt / (frame.rows * frame.cols) > params->rgb_area_threshold ? 1 : 0;   // Если количество пикселей извлеченного пламени по модели RGB больше порога то инициализируем критерий значением 1, иначе 0
    history->ema_rgb_extracted = (process_data->fps * history->ema_rgb_extracted + rgb_enough_criteria) / (process_data->fps + 1);  // Обновляем значение среднего скользящего критерия использования модели Grayscale для извлечения сигнала пламени по цвету
    return result;    // Возвращаем указатель на результат процессорного узла
}
