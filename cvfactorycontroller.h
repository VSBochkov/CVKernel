#ifndef CVFACTORYCONTROLLER_H
#define CVFACTORYCONTROLLER_H

#include <QMap>             // Подключение файла QMap для использования одноименного класса
#include <QList>            // Подключение файла QList для использования одноименного класса
#include <QString>          // Подключение файла QString для использования одноименного класса
#include <QJsonObject>      // Подключение файла QJsonObject для использования одноименного класса
#include <QSharedPointer>   // Подключение файла QSharedPointer для использования одноименного класса


namespace CVKernel {    // Определение области видимости
    struct CVProcessingNode;    // Предварительное объявление класса CVProcessingNode
    struct CVNodeParams;        // Предварительное объявление класса CVNodeParams
    struct CVNodeHistory;       // Предварительное объявление класса CVNodeHistory

    struct CVNodeFactory {      // Определение абстрактной фабрики создания процессорных узлов - (Паттерн ООП "Абстрактная фабрика")
        virtual CVProcessingNode* create_processing_node(QString& node) = 0;    // Объявление чисто-виртуальной функции создания процессорного узла
        virtual CVNodeParams* create_node_params(QJsonObject& json_obj) = 0;    // Объявление чисто-виртуальной функции создания параметров процессорного узла
        virtual CVNodeHistory* create_history(QString& node) = 0;               // Объявление чисто-виртуальной функции создания истории процессорного узла
        virtual ~CVNodeFactory() {}     // Определение виртуального деструктора класса
    };

    struct CVFactoryController  // Определение структуры менеджера фабрик
    {
        ~CVFactoryController() {    // Определение деструктора класса
            for (CVNodeFactory* factory: enabled_factories) {   // Организуем цикл по фабрикам списка
                delete factory;     // Удаляем фабрику
            }
            enabled_factories.clear();  // Очищаем список фабрик
        }

        CVProcessingNode* create_processing_node(QString &node) {       // Определение метода создания процессорного узла
            CVProcessingNode* proc_node = nullptr;                      // Инициализация процессорного узла нулевым указателем
            for (CVNodeFactory* factory: enabled_factories) {           // Организация цикла по всем фабрикам списка
                proc_node = factory->create_processing_node(node);      // Создание фабрикой объекта
                if (proc_node != nullptr) {     // Если объект был создан
                    break;                      // То выходим из цикла
                }
            }
            return proc_node;   // Возвращаем указатель на созданный объект
        }

        CVNodeParams* create_node_params(QJsonObject& json_obj) {       // Определение метода создания параметров процессорного узла
            CVNodeParams* node_params = nullptr;                        // Инициализация параметров узла нулевым указателем
            for (CVNodeFactory* factory: enabled_factories) {           // Организация цикла по всем фабрикам списка
                node_params = factory->create_node_params(json_obj);    // Создание фабрикой объекта
                if (node_params != nullptr) {   // Если объект был создан
                    break;                      // То выходим из цикла
                }
            }
            return node_params; // Возвращаем указатель на созданный объект
        }

        QMap<QString, QSharedPointer<CVNodeHistory>> create_history(QList<QString> nodes) { // Определение метода создания истории видеопроцессинга
            QMap<QString, QSharedPointer<CVNodeHistory>> history;       // Определение словаря истории
            for (auto& node: nodes) {           // Организуем цикл по всем строковым значениям процессорных узлов
                for (CVNodeFactory* factory: enabled_factories) {   // Организуем цикл по всем фабрикам списка
                    CVNodeHistory* hist_item = factory->create_history(node);   // Создание фабрикой объекта
                    if (hist_item != nullptr) { // Если объект создан
                        history[node] = QSharedPointer<CVNodeHistory>(hist_item);   // То сохраняем его в словаре
                        break;  // Выходим из внутреннего цикла
                    }
                }
            }
            return history; // Возвращаем словарь истории видеопроцессинга
        }

        static CVFactoryController& get_instance() {    // Определение метода get_instance() - Паттерн ООП "Одиночка"
            static CVFactoryController instance;    // Создаем статический объект класса только при первом вызове метода, далее
            return instance;                        // Возвращаем ссылку на объект
        }

        void set_factories(QList<CVNodeFactory*>& factories) {  // Определение метода установки списка фабрик для ядра видеоаналитики
            enabled_factories = factories;  // Присвоение списка фабрик
        }

        QList<CVNodeFactory*> enabled_factories;    // Список доступных фабрик для создания узлов видеопроцессинга
    };
}

#endif // CVFACTORYCONTROLLER_H   // Выход из области подключения
