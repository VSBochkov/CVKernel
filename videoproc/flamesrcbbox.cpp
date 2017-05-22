#include "flamesrcbbox.h"       // Подключаем файл flamesrcbbox.h для опредения методов
#include "fireweight.h"         // Подключаем файл fireweight.h для использования класса FireWeightDistribData
#include <opencv2/core.hpp>     // Подключаем файл core.hpp для использования класса Mat


// Определение метода compute класса FireBBox
// Входящий параметр process_data - структура данных видеоаналитики
// Возвращаемое значение - указатель на результат процессорного узла инициализированный классам FireBBoxData
QSharedPointer<CVKernel::CVNodeData> FlameSrcBBox::compute(QSharedPointer<CVKernel::CVProcessData> process_data) {
    ulong pixel_cnt = dynamic_cast<FireWeightDistribData *>(process_data->data["FireWeightDistrib"].data())->pixel_cnt; // Берем количество пискселей статического сигнала пламени из структуры
    cv::Mat flame = dynamic_cast<FireWeightDistribData *>(process_data->data["FireWeightDistrib"].data())->flame;       // Берем маску статического сигнала пламени из структуры
    cv::Mat overlay = CVKernel::video_data[process_data->video_name].overlay;                                           // Берем матрицу оверлея из структуры
    std::vector<object> bboxes = object_tracking(flame.clone(), overlay, pixel_cnt, cv::Scalar(0xff, 0, 0), *process_data); // Формируем список объектов очагов пламени
    QSharedPointer<FireBBoxData> result(new FireBBoxData(bboxes));  // Создаем указатель на результат процессорного узла
    return result;  // Возвращаем указатель на результат процессорного узла
}
