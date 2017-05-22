#ifndef FIREVALIDATION_H
#define FIREVALIDATION_H

#include <QSharedPointer>   // Подключение файла QSharedPointer для использования одноименного класса
#include "cvgraphnode.h"    // Подключение файла cvgraphnode для использования базовых классов

struct FireValidationData : public CVKernel::CVNodeData {   // Определение структуры FireValidationData - наследника класса данных процессорного узла
    FireValidationData(int rows, int cols);     // Объявление конструктора класса
    virtual ~FireValidationData() = default;    // Определение деструктора класса
    cv::Mat mask;   // Определение маски динамического сигнала пожара
};

struct FireValidationParams : public CVKernel::CVNodeParams {   // Определение структуры FireValidationParams - наследника класса параметров процессорного узла
    FireValidationParams(QJsonObject& json_obj);    // Объявление конструктора класса
    virtual ~FireValidationParams() {}              // Определение деструктора класса
    double alpha1;      // Параметр окна скользящего среднего 1 порядка
    double alpha2;      // Параметр окна скользящего среднего 2 порядка
    double dma_thresh;  // Порог скользящего среднего второго порядка
};

struct FireValidationHistory : public CVKernel::CVNodeHistory { // Определение структуры FireValidationHistory - наследника класса истории процессорного узла
    virtual void clear_history() {  // Определение метода очистки истории
        deffered_init = true;       // Активация отложенной инициализации
    }
    virtual ~FireValidationHistory() {} // Определение деструктора класса
    cv::Mat ema;        // Матрица скользящего среднего кадра 1 порядка
    cv::Mat dma;        // Матрица скользящего среднего кадоа 2 порядка
    bool deffered_init = true;  // Флаг отложенной инициализации
};

class FireValidation : public CVKernel::CVProcessingNode {  // Определение класса FireValidation - наследника класса процессорного узла
    Q_OBJECT    // Макроопределение спецификации Qt класса
public:     // Открытая секция класса
    explicit FireValidation() {}        // Определение конструктора класса
    virtual QSharedPointer<CVKernel::CVNodeData> compute(QSharedPointer<CVKernel::CVProcessData> process_data); // Объявление виртуального метода вычисления над кадром
};

#endif // FIREVALIDATION_H   // Выход из области подключения
