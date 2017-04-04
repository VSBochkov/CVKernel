#ifndef CVCONNECTOR_H
#define CVCONNECTOR_H

#include <QJsonObject>
#include <QByteArray>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>
#include <QMap>


namespace CVKernel {

    class CVIONode;
    class CVProcessForest;
    class CVProcessingNode;
    class CVNetworkManager;
    class CVProcessManager;

    struct CVConnectorType {
        enum type_value {client = false, supervisor = true} type;
        CVConnectorType(QJsonObject& json_obj);
    };

    struct CVInputProcessor {
        enum class recv_state {GET_SIZE, GET_BUFFER} state = recv_state::GET_SIZE;
        uint64_t buffer_size = 0;
        uint64_t bytes_received = 0;
        QByteArray buffer;
        bool receive_message(QTcpSocket* tcp_socket);
        void clear_buffer();
    };

    struct CVConnector {
        unsigned id;
        CVNetworkManager* network_manager;
        QTcpSocket* tcp_socket;
        CVInputProcessor receiver;
        bool run = false;
        CVConnector(unsigned index, CVNetworkManager* nm, QTcpSocket* sock)
            : id(index),
              network_manager(nm),
              tcp_socket(sock),
              receiver(),
              run(false) {}

        void send_buffer(QByteArray byte_arr);
        virtual ~CVConnector() {}
        virtual void process_incoming_message() = 0;
    };

    struct CVConnectorState {
        enum state_value {closed = 0, ready = 1, run = 2} state;
        QJsonObject pack_to_json();
        inline bool operator== (const CVConnectorState::state_value& desired_state) {
            return state == desired_state;
        }
    };

    struct CVConnectorChangedParams {
        unsigned id;
        CVConnectorChangedParams(CVConnector* connector) : id(connector->id) {}
        QJsonObject pack_to_json();
    };

    struct CVSupervisionInfo {
        int connected_clients;
        int used_process_nodes;
        QMap<QString, double> average_timings;
        QMap<QString, bool> overlay_state_changes;
        CVSupervisionInfo(): connected_clients(0), used_process_nodes(0) {}
        CVSupervisionInfo(CVSupervisionInfo& prev_info, int clients, QMap<QString, double> average_timings);
        QJsonObject pack_to_json();
    };

    struct CVSupervisor : public QObject, public CVConnector {
    Q_OBJECT
    public:
        explicit CVSupervisor(unsigned index, CVProcessManager* process_manager, CVNetworkManager* network_manager, QTcpSocket* sock);

        virtual ~CVSupervisor() {}
        virtual void process_incoming_message() override;

        void init_update_timer();
        void stop_update_timer();

    public:
        CVProcessManager* process_manager;
        QTimer* updateSupInfoTimer;
        CVSupervisionInfo info;
        const unsigned updateTimerValue = 20;


    private slots:
        void send_supervision_info();
    };

    struct CVSupervisorState : public CVConnectorState {
        CVSupervisorState(CVSupervisor* supervisor);
    };

    struct CVSupervisorChangedParams : public CVConnectorChangedParams {
        CVSupervisorState state;
        CVSupervisorChangedParams(CVSupervisor* supervisor)
            : CVConnectorChangedParams(supervisor), state(supervisor) {}
        QJsonObject pack_to_json();
    };

    struct CVClient : public CVConnector {
        QUdpSocket* meta_udp_client;
        QHostAddress udp_address;
        qint16 udp_port;
        CVIONode* video_io = nullptr;

        CVClient(unsigned index, CVNetworkManager* nm, QTcpSocket* tcp, QUdpSocket* udp = nullptr, CVIONode* node = nullptr)
            : CVConnector(index, nm, tcp), meta_udp_client(udp), video_io(node) {}
        virtual ~CVClient() {}
        virtual void process_incoming_message() override;
        void send_datagramm(QByteArray byte_arr);
        void init_udp_connection(QSharedPointer<CVProcessForest> proc_forest);
    };

    struct CVClientState : public CVConnectorState {
        CVClientState(CVClient* client);
    };

    struct CVClientChangedParams : public CVConnectorChangedParams {
        CVClientState state;
        QString overlay_path;
        CVClientChangedParams(CVClient* client);
        QJsonObject pack_to_json();
    };

    struct CVSupervisorStartup {
        QList<QSharedPointer<CVClient>> client_list;
        CVSupervisorStartup(QMap<QTcpSocket*, CVConnector*> connectors);
        QJsonObject pack_to_json();
    };

    struct CVCommand {
        enum command_value {run = 0, stop = 1, close = 2} command;
        CVCommand(QJsonObject& json_object);
        inline bool operator== (const CVCommand::command_value& desired_command) {
            return command == desired_command;
        }
    };
}

#endif // CVCONNECTOR_H
