#ifndef CVAPPLICATION_H
#define CVAPPLICATION_H

#include <QObject>
#include <QJsonObject>
#include "cvgraphnode.h"
#include "cvnetworkmanager.h"
#include "cvprocessmanager.h"
#include "cvjsoncontroller.h"
#include "cvfactorycontroller.h"

namespace CVKernel {

    struct CVApplicationState
    {
        QString mac_address;
        bool state;
        QString ip_address;

        CVApplicationState(QString mac, bool st);
        QJsonObject pack_to_json();
    };

    class CVApplication : public QObject
    {
        Q_OBJECT
    public:
        explicit CVApplication(QString app_settings_json);
        virtual ~CVApplication();

    public slots:
        void send_self_to_broadcast();

    private:
        QUdpSocket* broadcaster;
        QTimer* timer;
        CVNetworkManager* network_manager;
        CVProcessManager* process_manager;
        QSharedPointer<CVNetworkSettings> net_settings;
    };
}

#endif // CVAPPLICATION_H
