#include "firevalidation.h"     // Подключение файла firevalidation.h для определения методов
#include "rfiremaskingmodel.h"  // Подключение файля rfiremaskingmodel.h для использования класса DataRFireMM
#include <omp.h>                // Подключение файла omp.h для использования директив OpenMP

// Определение конструктора класса FireValidationData
// Входящие параметры:
//   rows - высота кадра
//   cols - ширина кадра
FireValidationData::FireValidationData(int rows, int cols)
{
    mask = cv::Mat(rows, cols, cv::DataType<uchar>::type); // Создание матрицы маски
}

// Определение конструктора класса FireValidationParams
// Входящий параметр json_obj - описание параметров модели выявления динамического сигнала пламени в формате JSON
FireValidationParams::FireValidationParams(QJsonObject& json_obj) : CVKernel::CVNodeParams(json_obj)
{
    QJsonObject::iterator iter;  // Объявление итератора по JSON объекту
    alpha1 = (iter = json_obj.find("alpha1")) == json_obj.end() ? 0.25 : iter.value().toDouble();   // Инициализация численного значения параметра alpha1
    alpha2 = (iter = json_obj.find("alpha2")) == json_obj.end() ? 0.75 : iter.value().toDouble();   // Инициализация численного значения параметра alpha2
    dma_thresh = (iter = json_obj.find("dma_thresh")) == json_obj.end() ? 12. : iter.value().toDouble(); // Инициализация численного значения параметра dma_thresh
}

// Определение метода compute класса FireValidation
// Входящий параметр process_data - структура данных видеоаналитики
// Возвращаемое значение - указатель на результат процессорного узла инициализированный классом FireValidationData
QSharedPointer<CVKernel::CVNodeData> FireValidation::compute(QSharedPointer<CVKernel::CVProcessData> process_data)
{
    cv::Mat frame = CVKernel::video_data[process_data->video_name].frame;                                   // Берем кадр
    cv::Mat rgbSignal = dynamic_cast<DataRFireMM *>(process_data->data["RFireMaskingModel"].data())->mask;  // Берем матрицу сигнала пламени выявленного по цвету
    QSharedPointer<FireValidationData> result(new FireValidationData(frame.rows, frame.cols));              // Создаем указатель на результат процессорного узла
    QSharedPointer<FireValidationHistory> history = process_data->history[metaObject()->className()].staticCast<FireValidationHistory>();   // Берем историю процессорного узла
    QSharedPointer<FireValidationParams> params = process_data->params[metaObject()->className()].staticCast<FireValidationParams>();   // Берем параметры процессорного узла

    if (history->deffered_init) {       // Если флаг отложенной инициализации установлен в True
        history->ema = frame.clone();   // Инициализируем матрицу скользящего среднего текущим кадром
        history->ema.convertTo(history->ema, CV_32FC3); // Преобразуем компоненты элементов матрицы скользящего среднего в тип float
        history->dma = cv::Mat::zeros(frame.rows, frame.cols, CV_32FC1);   // Создаем матрицу скользящего среднего второго порядка и инициализируем ее нулем
        history->deffered_init = false; // Выставляем значение флага отложенной инициализации в False
        return result;      // Возвращаем указатель на результат процессорного узла
    }

    uchar* frame_matr       = frame.data;                   // Берем адрес кадра в памяти для прямого доступа к элементам
    uchar* rgbSignal_matr   = rgbSignal.data;               // Берем адрес цветового сигнала в памяти для прямого доступа к элементам
    float* ema_matr         = (float*) history->ema.data;   // Берем адрес матрицы ema в памяти для прямого доступа к элементам
    float* dma_matr         = (float*) history->dma.data;   // Берем адрес матрицы dma в памяти для прямого доступа к элементам
    uchar* res_mask         = result->mask.data;            // Берем адрес результирующего сигнала в памяти для прямого доступа к элементам

    if (!params->draw_overlay) {    // Если не отображаем данные вычислений
    #pragma omp parallel for        // Распараллеливаем цикл ниже
        for (int i = 0; i < frame.rows; ++i) {  // Организуем цикл по строкам матрицы кадра
            for (int j = 0; j < frame.cols; ++j) { // Организуем цикл по столбцам матрицы кадра
                int id = i * frame.cols + j;    // Отображаем внутренний и внешний индексы на индекс для прямого доступа
                if (rgbSignal_matr[id]) {       // Если в пикселе кадра был обнаружен сигнал пламени по цвету
                    for (int k = 0; k < 3; ++k) {   // Организуем цикл по компонентам B,G,R пикселя
                       ema_matr[id * 3 + k] = ema_matr[id * 3 + k] * (1. - params->alpha1) + (float) frame_matr[id * 3 + k] * params->alpha1; // Вычисляем скользящее среднее по компонентам цвета
                    }
                    dma_matr[id] = (1. - params->alpha2) * dma_matr[id] + sqrt(             // Вычисляем cкользящее среднее
                        pow((float) frame_matr[id * 3]      - ema_matr[id * 3],     2.) +   // второго порядка
                        pow((float) frame_matr[id * 3 + 1]  - ema_matr[id * 3 + 1], 2.) +   // от расстояния между значениями
                        pow((float) frame_matr[id * 3 + 2]  - ema_matr[id * 3 + 2], 2.)     // цвета пикселей кадра
                    ) * params->alpha2;                                                     // и скользящего среднего 1 порядка
                } else {    // Если в пикселе кадра не был обнаружен сигнал пламени по цвету
                    dma_matr[id] = (1. - params->alpha2) * dma_matr[id] + params->alpha2 * 10;  // То ослабляем значение среднего 2 порядка
                }
                res_mask[id] = dma_matr[id] >= params->dma_thresh ? 1 : 0;      // Если среднее 2 порядка в пикселе больше порога то элемент динамической маски инициализируется значением 1 иначе 0
            }
        }
    } else {        // Если отображаем данные вычислений
        cv::Mat overlay = CVKernel::video_data[process_data->video_name].overlay;   // То берем матрицу overlay
        uchar* overlay_matr     = overlay.data;                                     // Берем адрес матрицы для прямого доступа к элементам
    #pragma omp parallel for
        for (int i = 0; i < frame.rows; ++i) {  // Организуем цикл по строкам матрицы кадра
            for (int j = 0; j < frame.cols; ++j) { // Организуем цикл по столбцам матрицы кадра
                int id = i * frame.cols + j; // Отображаем внутренний и внешний индексы на индекс для прямого доступа
                if (rgbSignal_matr[id]) {    // Если в пикселе кадра был обнаружен сигнал пламени по цвету
                    for (int k = 0; k < 3; ++k) {   // Организуем цикл по компонентам B,G,R пикселя
                       ema_matr[id * 3 + k] = ema_matr[id * 3 + k] * (1. - params->alpha1) + (float) frame_matr[id * 3 + k] * params->alpha1; // Вычисляем скользящее среднее по компонентам цвета
                    }
                    dma_matr[id] = (1. - params->alpha2) * dma_matr[id] + sqrt(             // Вычисляем cкользящее среднее
                        pow((float) frame_matr[id * 3]      - ema_matr[id * 3],     2.) +   // второго порядка
                        pow((float) frame_matr[id * 3 + 1]  - ema_matr[id * 3 + 1], 2.) +   // от расстояния между значениями
                        pow((float) frame_matr[id * 3 + 2]  - ema_matr[id * 3 + 2], 2.)     // цвета пикселей кадра
                    ) * params->alpha2;                                                     // и скользящего среднего 1 порядка
                } else {    // Если в пикселе кадра не был обнаружен сигнал пламени по цвету
                    dma_matr[id] = (1. - params->alpha2) * dma_matr[id] + params->alpha2 * 10;  // То ослабляем значение среднего 2 порядка
                }
                if (dma_matr[id] >= params->dma_thresh) {               // Если среднее 2 порядка в пикселе больше порога
                    overlay_matr[id * 3] = 0xff;                        // То закрашиваем
                    overlay_matr[id * 3 + 1] = frame_matr[id * 3 + 1];  // пиксель
                    overlay_matr[id * 3 + 2] = frame_matr[id * 3 + 2];  // оверлея
                    res_mask[id] = 1;                                   // Элемент динамической маски инициализируется значением 1 иначе 0
                } else {        // Если среднее 2 порядка в пискеле меньше порога
                    res_mask[id] = 0;   // Элемент динамической маски инициализируется значением 0
                }
            }
        }
    }
    return result;      // Возвращаем указатель на результат процессорного узла
}
