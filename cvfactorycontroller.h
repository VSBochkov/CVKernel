#ifndef CVFACTORYCONTROLLER_H
#define CVFACTORYCONTROLLER_H

#include <QMap>
#include <QList>
#include <QString>
#include <QJsonObject>
#include <QSharedPointer>


namespace CVKernel {
    struct CVProcessingNode;
    struct CVNodeParams;
    struct CVNodeHistory;


    struct CVNodeFactory {
        CVNodeFactory() {}
        virtual CVProcessingNode* create_processing_node(QString& node) = 0;
        virtual CVNodeParams* create_node_params(QJsonObject& json_obj) = 0;
        virtual CVNodeHistory* create_history(QString& node) = 0;
        virtual ~CVNodeFactory() {}
    };

    struct CVFactoryController
    {
        CVFactoryController() {}
        ~CVFactoryController() {
            for (CVNodeFactory* factory: enabled_factories) {
                delete factory;
            }
            enabled_factories.clear();
        }

        CVProcessingNode* create_processing_node(QString &node) {
            CVProcessingNode* proc_node = nullptr;
            for (CVNodeFactory* factory: enabled_factories) {
                proc_node = factory->create_processing_node(node);
                if (proc_node != nullptr) {
                    break;
                }
            }
            return proc_node;
        }

        CVNodeParams* create_node_params(QJsonObject& json_obj) {
            CVNodeParams* node_params = nullptr;
            for (CVNodeFactory* factory: enabled_factories) {
                node_params = factory->create_node_params(json_obj);
                if (node_params != nullptr) {
                    break;
                }
            }
            return node_params;
        }

        QMap<QString, QSharedPointer<CVNodeHistory>> create_history(QList<QString> nodes) {
            QMap<QString, QSharedPointer<CVNodeHistory>> history;
            for (auto& node: nodes) {
                for (CVNodeFactory* factory: enabled_factories) {
                    CVNodeHistory* hist_item = factory->create_history(node);
                    if (hist_item != nullptr) {
                        history[node] = QSharedPointer<CVNodeHistory>(hist_item);
                        break;
                    }
                }
            }
            return history;
        }

        static CVFactoryController& get_instance() {
            static CVFactoryController instance;
            return instance;
        }

        void set_factories(QList<CVNodeFactory*>& factories) {
            enabled_factories = factories;
        }

        QList<CVNodeFactory*> enabled_factories;
    };
}

#endif // CVFACTORYCONTROLLER_H
