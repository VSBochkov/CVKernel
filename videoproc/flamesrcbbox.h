#ifndef FLAMESRCBBOX_H
#define FLAMESRCBBOX_H

#include "firebbox.h"   // Подключение файла firebbox.h для использования базового класса

struct FlameSrcBBoxParams : public FireBBoxParams { // Определение структуры FlameSrcBBoxParams - наследника класса FireBBoxParams
    FlameSrcBBoxParams(QJsonObject& json_obj) : FireBBoxParams(json_obj) {} // Определение конструктора
    virtual ~FlameSrcBBoxParams() {}        // Определение деструктора
};

struct FlameSrcBBoxHistory : public FireBBoxHistory {   // Определение структуры FlameSrcBBoxHistory - наследника класса FireBBoxHistory
    FlameSrcBBoxHistory() {     // Определение конструктора
        grav_thresh = 10.;      // Инициализация начального значения гравитации
    }
    virtual void clear_history() {  // Определение метода очистки истории
        grav_thresh = 10.;      // Инициализация начального значения гравитации
        base_objects.clear();   // Очистка массива базовых объектов
    }
    virtual ~FlameSrcBBoxHistory() {}   // Определение виртуального деструктора класса
};

class FlameSrcBBox: public FireBBox // Определение структуры FlameSrcBBox - наследника класса FireBBox
{
    Q_OBJECT    // Макроопределение спецификации объекта Qt
public: // Открытая секция класса
    explicit FlameSrcBBox() {}  // Определение конструктора
    virtual QSharedPointer<CVKernel::CVNodeData> compute(QSharedPointer<CVKernel::CVProcessData> process_data); // Объявление виртуального метода вычисления над кадром
};

#endif // FLAMESRCBBOX_H   // Выход из области подключения
