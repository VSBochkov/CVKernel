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

    class CVApplication : public QObject
    {
        Q_OBJECT
    public:
        explicit CVApplication(QString app_settings_json);
        virtual ~CVApplication();

    private:
        QUdpSocket* broadcaster;
        QTimer* timer;
        CVNetworkManager* network_manager;
        CVProcessManager* process_manager;
        QSharedPointer<CVNetworkSettings> net_settings;
    };

    QString get_ip_address();
}

#endif // CVAPPLICATION_H
