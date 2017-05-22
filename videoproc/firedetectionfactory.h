#ifndef FIREDETECTIONFACTORY_H
#define FIREDETECTIONFACTORY_H

#include "cvfactorycontroller.h"    // Подключение файла cvfactorycontroller для использования класса родителя
#include "cvgraphnode.h"            // Подключение файла cvgraphnode.h для использования классов CVProcessingNode, CVNodeParams, CVNodeHistory
#include "rfiremaskingmodel.h"      // Подключение
#include "yfiremaskingmodel.h"      // файлов
#include "firevalidation.h"         // библиотеки
#include "firebbox.h"               // нахождения
#include "fireweight.h"             // пожаров
#include "flamesrcbbox.h"           // на видео

struct FireDetectionFactory : public CVKernel::CVNodeFactory    // Определение конкретной фабрики создания объектов классов библиотеки детекции пламени на видео
{    
    FireDetectionFactory() {}       // Определение конструктора
    virtual CVKernel::CVProcessingNode* create_processing_node(QString &node)   // Определение виртуального метода создания процессорного узла
    {
        if      (node == "RFireMaskingModel")   return new RFireMaskingModel();     // Возвращаем
        else if (node == "YFireMaskingModel")   return new YFireMaskingModel();     // новый
        else if (node == "FireValidation")      return new FireValidation();        // объект
        else if (node == "FireWeightDistrib")   return new FireWeightDistrib();     // процессорного
        else if (node == "FireBBox")            return new FireBBox();              // узла по
        else if (node == "FlameSrcBBox")        return new FlameSrcBBox();          // имени класса
        else                                    return nullptr; // Если класс по имени node не определен в библиотеке то возвращаем нулевой указатель
    }

    virtual CVKernel::CVNodeParams* create_node_params(QJsonObject& json_obj)   // Определение метода создания структуры параметров узла видеоаналитики
    {
        QJsonObject::iterator iter = json_obj.find("node");     // Находим элемент node в JSON объекте
        if (iter == json_obj.end())     // Если элемент не найден
            return nullptr;             // То возвращаем нулевой указатель

        QString node = iter.value().toString();  // Инициализируем строковое значение класса процессорного узла

        if (node == "RFireMaskingModel")        return new RFireParams(json_obj);               // Возвращаем
        else if (node == "YFireMaskingModel")   return new YFireParams(json_obj);               // новый
        else if (node == "FireValidation")      return new FireValidationParams(json_obj);      // объект
        else if (node == "FireWeightDistrib")   return new FireWeightDistribParams(json_obj);   // параметров узла
        else if (node == "FireBBox")            return new FireBBoxParams(json_obj);            // по имени класса
        else if (node == "FlameSrcBBox")        return new FlameSrcBBoxParams(json_obj);        // процессорного узла
        else                                    return nullptr; // Если класс по имени node не определен в библиотеке то возвращаем нулевой указатель
    }

    virtual CVKernel::CVNodeHistory* create_history(QString& node) {        // Определение метода создания структуры истории процессорного узла
        if      (node == "RFireMaskingModel")   return new RFireHistory();              // Возвращаем
        else if (node == "YFireMaskingModel")   return new YFireHistory();              // новый
        else if (node == "FireValidation")      return new FireValidationHistory();     // объект
        else if (node == "FireWeightDistrib")   return new FireWeightDistribHistory();  // истории узла
        else if (node == "FireBBox")            return new FireBBoxHistory();           // по имени класса
        else if (node == "FlameSrcBBox")        return new FlameSrcBBoxHistory();       // процессорного узла
        else                                    return nullptr; // Если класс по имени node не определен в библиотеке то возвращаем нулевой указатель
    }
};

#endif // FIREDETECTIONFACTORY_H   // Выход из области подключения

