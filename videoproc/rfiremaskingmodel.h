#ifndef RFIREMASKINGMODEL_H
#define RFIREMASKINGMODEL_H

#include <QSharedPointer>       // Подключение файла QSharedPointer для использования одноименного класса
#include "cvgraphnode.h"        // Подключение файла cvgraphnode для использования базовых классов

struct DataRFireMM : public CVKernel::CVNodeData {   // Определение структуры DataRFireMM - наследника класса данных процессорного узла
    DataRFireMM(int rows, int cols);    // Объявление конструктора
    virtual ~DataRFireMM() = default;   // Определение виртуального деструктора
    cv::Mat mask;               // Маска цветового выявления сигнала пламени
    ulong pixel_cnt;            // Количество пикселей извлеченного пламени по модели RGB
    double ema_rgb_extracted;   // Текущее значение достаточности детектирования пламени по модели RGB
};

struct RFireParams : public CVKernel::CVNodeParams {    // Определение структуры RFireParams - наследника класса параметров процессорного узла
    RFireParams(QJsonObject& json_obj); // Объявление конструктора
    virtual ~RFireParams() {}   // Определение виртуального деструктора
    double rgb_area_threshold;  // Порог по количеству найденных пикселей пламени
    double rgb_freq_threshold;  // Порог частоты нахождения достаточного количества пикселей пламени по модели RGB
};

struct RFireHistory : public CVKernel::CVNodeHistory {  // Объявление класса RFireHistory - наследника класса истории процессорного узла
    RFireHistory(); // Объявление конструктора класса
    virtual void clear_history();   // Объявление метода очистки истории
    virtual ~RFireHistory() {}      // Определение виртуального деструктора

    double ema_rgb_extracted;   // Скользящее среднее значения достаточности детектирования пламени по модели RGB
};

class RFireMaskingModel : public CVKernel::CVProcessingNode {   // Определение класса RFireMaskingModel - наследника класса процессорного узла
    Q_OBJECT    // Макроопределение спецификации класса Qt
public: // Открытая секция класса
    explicit RFireMaskingModel() {} // Определение конструктора класса
    virtual QSharedPointer<CVKernel::CVNodeData> compute(QSharedPointer<CVKernel::CVProcessData> process_data); // Объявление виртуального метода вычисления над кадром
};

#endif // RFIREMASKINGMODEL_H   // Выход из области подключения
