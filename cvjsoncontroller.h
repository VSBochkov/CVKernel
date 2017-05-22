#ifndef CVJSONCONTROLLER_H
#define CVJSONCONTROLLER_H

#include <QSharedPointer>   // Подключение файла QSharedPointer для использования одноименного класса
#include <QJsonDocument>    // Подключение файла QJsonDocument для использования одноименного класса
#include <QJsonObject>      // Подключение файла QJsonObject для использования одноименного класса
#include <QJsonArray>       // Подключение файла QJsonArray для использования одноименного класса
#include <QTextStream>      // Подключение файла QTextStream для использования одноименного класса
#include <QFile>            // Подключение файла QFile для использования одноименного класса

namespace CVKernel {    // Определение области видимости
    class CVJsonController {    // Определение класса CVJsonController
    public: // Открытая секция класса
        template <class Structure> static QSharedPointer<Structure> get_from_json_buffer(QByteArray& json_arr) {    // Определение шаблонной функции получения объекта из буфера в формате JSON
            QJsonDocument json = QJsonDocument::fromJson(json_arr);     // Прочитываем буфер в структуру документа JSON
            QJsonObject json_obj = json.object();                       // Преобразуем тип JSON документа в JSON объект
            return QSharedPointer<Structure>(new Structure(json_obj));  // Возвращаем указатель на целевой тип инициализированный JSON объектом
        }

        template <class Structure> static QSharedPointer<Structure> get_from_json_file(QString& json_fname) {   // Определение шаблонной функции получения объекта из файла в формате JSON
            QFile json_file(json_fname);    // Создаем объект файла
            if (!json_file.open(QIODevice::ReadOnly | QIODevice::Text)) // Если не получается открыть файл в режиме чтения
                return QSharedPointer<Structure>(); //  То возвращаем целевой объект по-умолчанию

            QJsonDocument json = QJsonDocument::fromJson(json_file.readAll());  // Прочитываем файл в структуру документа JSON
            json_file.close();                      // Закрываем файл
            QJsonObject json_obj = json.object();   // Преобразуем тип JSON документа в JSON объект

            return QSharedPointer<Structure>(new Structure(json_obj));  // Возвращаем указатель на целевой тип инициализированный JSON объектом
        }

        template <class Structure> static QByteArray pack_to_json(Structure data) { // Определение шаблонной функции перевода объекта данных в формат JSON
            QJsonObject json_obj = data.pack_to_json();         // Вызываем метод конвертации целевого объекта в формат JSON
            if (json_obj.empty())                               // Если JSON объект пустой
                return QByteArray();                            // Возвращаем пустой буфер

            QJsonDocument json_doc(json_obj);                   // Определяем структуру JSON документа из JSON объекта
            return json_doc.toJson(QJsonDocument::Compact);     // Возвращаем JSON документ сериализованный в буффер
        }

        template <class Structure> static QByteArray pack_to_json(Structure* data) { // Определение шаблонной функции перевода указателя на объект данных в формат JSON
            QJsonObject json_obj = data->pack_to_json();        // Вызываем метод конвертации целевого объекта в формат JSON
            if (json_obj.empty())                               // Если JSON объект пустой
                return QByteArray();                            // Возвращаем пустой буфер

            QJsonDocument json_doc(json_obj);                   // Определяем структуру JSON документа из JSON объекта
            return json_doc.toJson(QJsonDocument::Compact);     // Возвращаем JSON документ сериализованный в буффер
        }
    };
}

#endif // CVJSONCONTROLLER_H   // Выход из области подключения
