#ifndef CVSUPERVISOR_H
#define CVSUPERVISOR_H

#include <QJsonObject>
#include <QByteArray>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>
#include <QMap>

#include "cvconnector.h"


namespace CVKernel {
    struct CVProcessForest;
    struct CVProcessManager;
    struct CVNetworkManager;
    struct CVClient;
    struct CVIONode;

    struct CVSupervisorFactory : CVConnectorFactory
    {
        CVSupervisorFactory(const CVSupervisorFactory&) = delete;
        CVSupervisorFactory& operator==(const CVSupervisorFactory&) = delete;
        virtual ~CVSupervisorFactory() = default;

        virtual CVConnectorState* set_state(CVConnector& connector) override;
        static CVSupervisorFactory* instance()
        {
            static CVSupervisorFactory inst;
            return &inst;
        }

    private:
        CVSupervisorFactory() {}
    };

    struct CVSupervisionInfo
    {
        int connected_clients;
        int used_process_nodes;
        QMap<QString, double> average_timings;
        QMap<QString, bool> overlay_state_changes;
        CVSupervisionInfo(): connected_clients(0), used_process_nodes(0) {}
        CVSupervisionInfo(CVSupervisionInfo& prev_info, int clients, QMap<QString, double> average_timings);
        QJsonObject pack_to_json();
    };

    struct CVSupervisionSettings
    {
        CVSupervisionSettings(QJsonObject& json_obj);
        int port;
        int update_timer_value;
    };

    class CVSupervisor : public CVConnector {
    Q_OBJECT
    public:
        explicit CVSupervisor(unsigned index, CVProcessManager& process_manager, CVNetworkManager& network_manager, QTcpSocket& sock);
        virtual ~CVSupervisor();

        void init_supervision(QSharedPointer<CVSupervisionSettings> settings);
        virtual void run() override;
        virtual void stop() override;
        virtual void close() override;

    public:
        QTimer* updateSupInfoTimer;
        QTcpSocket* tcp_supervision;
        CVSupervisionInfo info;
        int supervision_port;
        int update_timer_value;

    private:
        void init_update_timer();
        void stop_update_timer();


    private slots:
        void send_supervision_info();
        void send_connector_state_change(CVConnectorState&);
    };

    struct CVSupervisorClosed : CVConnectorClosed {
        CVSupervisorClosed(CVSupervisor& sup)
            : CVConnectorClosed(sup)
        {}

        virtual void handleIncommingMessage(QByteArray&) override;
    };

    struct CVInitSupervisionCommand : public CVConnectorCommand
    {
        CVInitSupervisionCommand(CVSupervisor& sup, QSharedPointer<CVSupervisionSettings> params)
            : supervisor(sup),
              settings(params)
        {}

        virtual void execute() override;

        CVSupervisor& supervisor;
        QSharedPointer<CVSupervisionSettings> settings;
    };

    struct CVSupervisorStartup {
        QList<QSharedPointer<CVClient>> client_list;
        CVSupervisorStartup(QList<QSharedPointer<CVClient>> clients);
        QJsonObject pack_to_json();
    };
}
#endif // CVSUPERVISOR_H
