#ifndef CVJSONCONTROLLER_H
#define CVJSONCONTROLLER_H

#include <QSharedPointer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>
#include <QFile>
#include <QDebug>


namespace CVKernel {
    class CVJsonController {
    public:
        template <class Structure> static QSharedPointer<Structure> get_from_json_buffer(QByteArray& json_arr) {
            QJsonDocument json = QJsonDocument::fromJson(json_arr);
            QJsonObject json_obj = json.object();
            return QSharedPointer<Structure>(new Structure(json_obj));
        }

        template <class Structure> static QSharedPointer<Structure> get_from_json_file(QString& json_fname) {
            QFile json_file(json_fname);
            if (!json_file.open(QIODevice::ReadOnly | QIODevice::Text))
                return QSharedPointer<Structure>();

            QJsonDocument json = QJsonDocument::fromJson(json_file.readAll());
            json_file.close();
            QJsonObject json_obj = json.object();

            return QSharedPointer<Structure>(new Structure(json_obj));
        }

        template <class Structure> static QByteArray pack_to_json_binary(Structure data) {
            QJsonObject json_obj = data.pack_to_json();
            if (json_obj.empty()) {
                return QByteArray();
            }
            QJsonDocument json_doc(json_obj);
            return json_doc.toBinaryData();
        }

        template <class Structure> static QByteArray pack_to_json_ascii(Structure data) {
            QJsonObject json_obj = data.pack_to_json();
            if (json_obj.empty()) {
                return QByteArray();
            }
            QJsonDocument json_doc(json_obj);
            return json_doc.toJson(QJsonDocument::Compact);
        }

        template <class Structure> static void pack_to_json_file(Structure data, QString json_filename) {
            QJsonObject json_obj = data.pack_to_json();
            if (json_obj.empty()) {
                return;
            }
            QJsonDocument json_doc(json_obj);
            QFile json_file(json_filename);
            if (!json_file.open(QIODevice::WriteOnly | QIODevice::Text))
                return;

            QTextStream stream(&json_file);
            stream << json_doc.toJson();
            json_file.close();
        }
    };
}

#endif // CVJSONCONTROLLER_H
