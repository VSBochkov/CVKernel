#ifndef FIREMASKLOGICAND_H
#define FIREMASKLOGICAND_H

#include <QSharedPointer>       // Подключение файла QSharedPointer для использования одноименного класса
#include "cvgraphnode.h"        // Подключение файла cvgraphnode.h для использования базовых классов
#include "firebbox.h"           // Подключение файла firebbox.h для использования структуры object

struct FireWeightDistribData : public CVKernel::CVNodeData { // Определение структуры FireWeightDistribData - наследника класса данных процессорного узла
    FireWeightDistribData(cv::Mat base, cv::Mat flame_src, ulong flame_pixel_cnt);  // Объявление конструктора класса
    virtual ~FireWeightDistribData() {} // Определение виртуального деструктора
    cv::Mat weights;        // Матрица весов статического сигнала
    cv::Mat flame;          // Матрица сигнала источников пламени
    ulong pixel_cnt;        // Количество пикселей источников пламени
};

struct FireWeightDistribParams : public CVKernel::CVNodeParams { // Определение структуры FireWeightDistribParams - наследника класса параметров процессорного узла
    FireWeightDistribParams(QJsonObject& json_obj); // Объявление конструктора класса
    virtual ~FireWeightDistribParams() {}           // Определение виртуального деструктора класса
    float weight_thr;       // Порог по статичности сигнала
    float time_thr;         // Порог по времени жизни статичного сигнала
};

struct FireWeightDistribHistory : public CVKernel::CVNodeHistory {  // Определение структуры FireWeightDistribHistory - наследника класса истории процессорного узла0
    FireWeightDistribHistory() {}   // Определение конструктора класса
    virtual void clear_history();   // Объявление метода очистки истории
    virtual ~FireWeightDistribHistory() {}  // Определение деструктора класса
    cv::Mat base;       // Базовая матрица статичности сигнала
    cv::Mat timings;    // Матрица времени жизни сигнала
};

class FireWeightDistrib : public CVKernel::CVProcessingNode // Определение класса FireWeightDistrib - наследника класса процессорного узла
{
    Q_OBJECT    // Макроопределение спецификации класса Qt
public: // Открытая секция класса
    explicit FireWeightDistrib() {} // Определение конструктора класса
    virtual QSharedPointer<CVKernel::CVNodeData> compute(QSharedPointer<CVKernel::CVProcessData> process_data); // Объявление виртуального метода вычисления над кадром

private:    // Закрытая секция класса
    bool pt_in_rect(int i, int j, const cv::Rect& bbox) { // Определение метода нахождения точки в прямоугольнике
        return (i >= bbox.y && j >= bbox.x && // Проверка нахождения
                i <= bbox.y + bbox.height &&  // координат точки
                j <= bbox.x + bbox.width);    // в интервалах прямоугольника
    }

    object* get_bbox(int i, int j, std::vector<object> bboxes) { // Определение метода получения объекта в точке
        static size_t id = 0;   // Определение итератора по массиву объектов, с запоминанием последнего индекса для уменьшения времени выполнения поиска в соседних пикселях
        for (size_t cnt = 0; cnt < bboxes.size(); ++cnt) {  // Организация цикла по количеству элементов в массиве объектов
            if (pt_in_rect(i, j, bboxes[(id + cnt) % bboxes.size()].rect)) {    // Если точка находится в объекте
                return &bboxes[(id + cnt) % bboxes.size()]; // То вернуть его адрес
            }
        }
        return NULL;    // Если точка не найдена ни в одном объекте то вернуть нулевой указатель
    }
};

#endif // FIREMASKLOGICAND_H   // Выход из области подключения
