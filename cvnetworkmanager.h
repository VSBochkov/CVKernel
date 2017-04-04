#ifndef CVNETWORKMANAGER_H
#define CVNETWORKMANAGER_H

#include <QObject>
#include <QTimer>
#include <QTcpSocket>
#include <QTcpServer>
#include <QUdpSocket>


namespace CVKernel {

    class CVClient;
    class CVIONode;
    class CVConnector;
    class CVInputProcessor;
    class CVProcessManager;
    class CVProcessForest;
    class CVProcessData;
    class CVProcessingNode;

    struct CVNetworkManagerSettings {
        int tcp_server_port = 0;
        CVNetworkManagerSettings(QJsonObject& json_object);
    };

    class CVNetworkManager : public QObject
    {
        Q_OBJECT
    public:
        explicit CVNetworkManager(CVNetworkManagerSettings& settings);
        virtual ~CVNetworkManager();

        void set_process_manager(CVProcessManager* proc_manager) { process_manager = proc_manager; }
        int get_clients_number();
        void propagate_to_supervisors(QByteArray buffer);

        void activate_new_client(QTcpSocket *connection, QSharedPointer<CVProcessForest> proc_forest);
        void run(CVIONode *io_node) { emit run_io_node(io_node); }
        void stop(CVIONode *io_node) { emit stop_io_node(io_node); }
        void close(CVIONode *io_node) { emit close_io_node(io_node); }

    private:
        CVProcessManager* process_manager;
        QTcpServer* tcp_server;
        CVInputProcessor* receiver;
        QMap<QTcpSocket*, CVConnector*> connectors;
        QTimer* supervision_timer;
        QMap<QUdpSocket*, QTcpSocket*> udp_to_tcp_map;
        QMap<CVIONode*, QTcpSocket*> ionode_to_tcp_map;
        unsigned connector_id = 0;

    private:
        void setup_tcp_server(CVNetworkManagerSettings& settings);
        void init_udp_connection(QTcpSocket* tcp_client, QSharedPointer<CVProcessForest> proc_forest);

    signals:
        void got_process_forest(QTcpSocket* client, QSharedPointer<CVProcessForest> proc_forest);
        void run_io_node(CVIONode* io_node);
        void stop_io_node(CVIONode* io_node);
        void close_io_node(CVIONode* io_node);
        void udp_closed(CVIONode* io_node);

    public slots:
        void connect_new_client();
        void recv_client_data();
        void transmit_metadata(QByteArray metadata);
        void do_close_udp();
        void on_io_node_created(QTcpSocket* client, CVIONode* new_io_node);
        void on_io_node_started(CVIONode* io_node);
        void on_io_node_stopped(CVIONode* io_node);
        void on_io_node_closed(CVIONode* io_node);
        void add_new_connector(QTcpSocket* socket);
    };
}

#endif // CVNETWORKMANAGER_H
