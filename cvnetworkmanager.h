#ifndef CVNETWORKMANAGER_H
#define CVNETWORKMANAGER_H

#include <QObject>
#include <QTimer>
#include <QTcpSocket>
#include <QTcpServer>
#include <QUdpSocket>


namespace CVKernel {

    class CVClient;
    class CVSupervisor;
    class CVIONode;
    class CVConnector;
    class CVInputProcessor;
    class CVProcessManager;
    class CVProcessForest;
    class CVProcessData;
    class CVProcessingNode;

    struct CVNetworkSettings {
        QString mac_address = "";
        int udp_broadcast_port = 0;
        int tcp_server_port = 0;
        CVNetworkSettings(QJsonObject& json_object);
    };

    class CVNetworkManager : public QObject
    {
        Q_OBJECT
    public:
        explicit CVNetworkManager(CVNetworkSettings& settings, CVProcessManager& pm);
        virtual ~CVNetworkManager();

        QList<QSharedPointer<CVClient>> get_clients();
        QList<QSharedPointer<CVSupervisor>> get_supervisors();

    private:
        QTcpServer* tcp_server;
        CVInputProcessor* receiver;
        CVProcessManager& process_manager;
        QMap<QTcpSocket*, QSharedPointer<CVClient>> clients;
        QMap<QTcpSocket*, QSharedPointer<CVSupervisor>> supervisors;
        unsigned connector_id = 0;

    private:
        void setup_tcp_server(CVNetworkSettings& settings);
        void init_udp_connection(QTcpSocket* tcp_client, QSharedPointer<CVProcessForest> proc_forest);

    signals:
        void udp_closed(CVIONode* io_node);

    public slots:
        void add_tcp_connection();
        void init_new_conector();
        void delete_connector();
        void add_new_connector(QTcpSocket* socket);
    };
}

#endif // CVNETWORKMANAGER_H
