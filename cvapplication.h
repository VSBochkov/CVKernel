#ifndef CVAPPLICATION_H
#define CVAPPLICATION_H

#include <QObject>
#include <QJsonObject>
#include <QSocketNotifier>
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

    // Unix signal handlers.
        static int setup_unix_signal_handlers();
        static void usr1_handler(int unused);
        static void term_handler(int unused);

    public slots:
        void shutdown();
        void reboot();

    private:
        QUdpSocket* broadcaster;
        QTimer* timer;
        CVNetworkManager* network_manager;
        CVProcessManager process_manager;
        QSharedPointer<CVNetworkSettings> net_settings;
        QSocketNotifier *sn_usr1;
        QSocketNotifier *sn_term;
    };

    QString get_ip_address();
}

#endif // CVAPPLICATION_H
