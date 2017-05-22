#ifndef FIRE_BBOX_H
#define FIRE_BBOX_H

#include <QJsonArray>       // Подключение файла QJsonArray для использования одноименного класса
#include <QJsonObject>      // Подключение файла QJsonObject для использования одноименного класса
#include <QSharedPointer>   // Подключение файла QSharedPointer для использования одноименного класса
#include <QDataStream>      // Подключение файла QDataStream для использования одноименного класса
#include "cvgraphnode.h"    // Подключение файла cvgraphnode.h для использования базовых классов
#include <vector>           // Подключение файла vector для использования одноименного класса

struct object {         // Определение структуры объекта
    cv::Rect rect;      // Прямоугольная область на кадре
    long last_fnum;     // Последний номер кадра, когда был найден объект
    long first_fnum;    // Первый номер кадра, когда был найден объект

    object(cv::Rect bbox, long fnum):   // Определение конструктора структуры
        rect(bbox), last_fnum(fnum), first_fnum(fnum) {}    // Список инициализации
    object(): rect(cv::Rect()), last_fnum(-1), first_fnum(-1) {}    // Определение конструктора по-умолчанию

    QJsonObject pack_to_json() {    // Определение метода сериализации данных в объект JSON
        QJsonObject bbox;           // Объявление JSON объекта
        bbox["x"] = rect.x;         // Присвоение координаты x
        bbox["y"] = rect.y;         // Присвоение координаты y
        bbox["w"] = rect.width;     // Присвоение параметра ширины прямоугольника
        bbox["h"] = rect.height;    // Присвоение параметра высоты прямоугольника
        return bbox;                // Возврат JSON объекта
    }
};

struct FireBBoxData : public CVKernel::CVNodeData {     // Определение структуры FireBBoxData наследника класса данных процессорного узла
    FireBBoxData(std::vector<object>& bboxes);      // Объявление конструктора класса
    virtual ~FireBBoxData() = default;              // Определение виртуального деструктора по-умолчанию
    virtual QJsonObject pack_to_json() {            // Определение метода сериализации данных в формате JSON
        QJsonArray bboxes_json;                     // Объявление массива JSON
        for (auto bbox : fire_bboxes) {             // Организация цикла по всем прямоугольным областям
            QJsonObject bbox_obj = bbox.pack_to_json(); // Сериализация объекта в формате JSON
            bboxes_json.append(bbox_obj);               // Добавление объекта в конец массива JSON
        }
        QJsonObject fire_bbox_json;                 // Определение JSON объекта для массива прямоугольных областей
        if (not bboxes_json.empty()) {              // Если массив не пуст
            fire_bbox_json["bboxes"] = bboxes_json; // То присваиваем его объекту
        }
        return fire_bbox_json;  // Возвращаем объект JSON
    }
    std::vector<object> fire_bboxes;    // Определение вектора объектов нахождения пожара
};

struct FireBBoxParams : public CVKernel::CVNodeParams {     // Определение структуры FireBBoxParams - наследника класса параметров процессорного узла
    FireBBoxParams(QJsonObject& json_obj);          // Объявление конструктора класса
    virtual ~FireBBoxParams() {}                    // Определение виртуального деструктора
    double min_area_percent;                        // Порог минимальной области объекта на кадре, выраженный в процентах
    double intersect_thresh;                        // Порог пересечения прямоугольных областей предыдущего и текущего кадров для слежения за объектами
    double dtime_thresh;                            // Порог времени жизни объекта выраженный в секундах
};

struct FireBBoxHistory : public CVKernel::CVNodeHistory {   // Определение структуры FireBBoxHistory - наследника класса истории процессорного узла
    virtual void clear_history() {  // Определение метода очистки истории
        grav_thresh = 5.;           // Присвоение первоначального значения порога гравитации
        base_objects.clear();       // Очистка вектора базовых объектов
    }
    virtual ~FireBBoxHistory() {}       // Определение виртуального деструктора класса
    double grav_thresh = 5.;            // Определение порога гравитации
    std::vector<object> base_objects;   // Определение вектора базовых объектов
};

class FireBBox : public CVKernel::CVProcessingNode {    // Определение структуры FireBBox - наследника класса процессорного узла
    Q_OBJECT    // Макроопределение спецификации Qt класса
public:         // Открытая секция класса
    explicit FireBBox() {}      // Определение конструктора класса
    virtual QSharedPointer<CVKernel::CVNodeData> compute(QSharedPointer<CVKernel::CVProcessData> process_data); // Объявление виртуального метода вычисления над кадром

protected:      // Защищенная секция класса
    std::vector<object> object_tracking(cv::Mat proc_mask, cv::Mat overlay, ulong pixel_cnt, cv::Scalar bbox_color, CVKernel::CVProcessData &process_data); // Объявления метода слежения за объектами
    cv::Rect intersection(cv::Rect rect1, cv::Rect rect2) {     // Определение метода нахождения пересечения двух прямоугольников
        int x1 = std::max(rect1.x, rect2.x);    // Присваиваем координате x нового прямоугольника максимум от координат x двух прямоугольников
        int y1 = std::max(rect1.y, rect2.y);    // Присваиваем координате y нового прямоугольника максимум от координат y двух прямоугольников
        int x2 = std::min(rect1.x + rect1.width, rect2.x + rect2.width);   // Присваиваем координате x + w нового прямоугольника минимум от координат x + w двух прямоугольников
        int y2 = std::min(rect1.y + rect2.height, rect2.y + rect2.height); // Присваиваем координате y + h нового прямоугольника минимум от координат y + h двух прямоугольников
        if (x1 >= x2 || y1 >= y2)   // Если одна из начальных координат больше конечной
            return cv::Rect();      // То пересечения нет, возвращаем нулевой прямоугольник
        else                        // Иначе
            return cv::Rect(x1, y1, x2 - x1, y2 - y1);  // Возвращаем пересечение прямоугольников
    }
    cv::Rect rect_union(cv::Rect rect1, cv::Rect rect2) {   // Определение метода нахождения объединения двух прямоугольников
        int x1 = std::min(rect1.x, rect2.x);                // Присваиваем координате x нового прямоугольника минимум от координат x двух прямоугольников
        int y1 = std::min(rect1.y, rect2.y);                // Присваиваем координате y нового прямоугольника минимум от координат y двух прямоугольников
        int x2 = std::max(rect1.x + rect1.width, rect2.x + rect2.width);    // Присваиваем координате x + w нового прямоугольника максимум от координат x + w двух прямоугольников
        int y2 = std::max(rect1.y + rect2.height, rect2.y + rect2.height);  // Присваиваем координате y + h нового прямоугольника максимум от координат y + h двух прямоугольников
        return cv::Rect(x1, y1, x2 - x1, y2 - y1);      // Возвращаем объединение прямоугольников
    }
};

#endif // FIRE_BBOX_H   // Выход из области подключения
