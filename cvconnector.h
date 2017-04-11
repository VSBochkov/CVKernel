#ifndef CVCONNECTOR_H
#define CVCONNECTOR_H

#include <QSharedPointer>
#include <QJsonObject>
#include <QByteArray>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>
#include <QMap>

#include <memory>


namespace CVKernel {

    class CVIONode;
    class CVProcessForest;
    class CVProcessingNode;
    class CVNetworkManager;
    class CVProcessManager;

    class CVConnector;

    struct CVConnectorType
    {
        enum type_value {client = false, supervisor = true} type;
        CVConnectorType(QJsonObject& json_obj);
    };

    struct CVInputProcessor
    {
        enum class recv_state {GET_SIZE, GET_BUFFER} state = recv_state::GET_SIZE;
        uint64_t buffer_size = 0;
        uint64_t bytes_received = 0;
        QByteArray buffer;
        bool receive_message(QTcpSocket& tcp_socket);
        void clear_buffer();
    };

    struct CVConnectorCommand
    {
        virtual ~CVConnectorCommand() {}
        virtual void execute() = 0;
    };

    struct CVConnectorState
    {
        enum value {closed = 0, ready = 1, run = 2} state;
        std::unique_ptr<CVConnectorCommand> command;

        CVConnectorState(CVConnectorState::value st, CVConnector& conn)
            : state(st),
              connector(conn)
        {}

        virtual ~CVConnectorState() = default;
        virtual void handleIncommingMessage(QByteArray& buffer) = 0;

        QJsonObject pack_to_json();

        CVConnector& connector;
    };

    struct CVConnectorClosed : CVConnectorState
    {
        CVConnectorClosed(CVConnector& conn)
            : CVConnectorState(CVConnectorState::closed, conn)
        {}

        virtual void handleIncommingMessage(QByteArray& buffer) = 0;
    };

    struct CVConnectorReady : CVConnectorState
    {
        CVConnectorReady(CVConnector& conn)
            : CVConnectorState(CVConnectorState::ready, conn)
        {}

        virtual void handleIncommingMessage(QByteArray& buffer) override;
    };

    struct CVConnectorRun : CVConnectorState
    {
        CVConnectorRun(CVConnector& conn)
            : CVConnectorState(CVConnectorState::ready, conn)
        {}

        virtual void handleIncommingMessage(QByteArray& buffer) override;
    };

    struct CVRunCommand : public CVConnectorCommand
    {
        CVRunCommand(CVConnector& conn)
            : connector(conn)
        {}

        virtual void execute() override;

        CVConnector& connector;
    };

    struct CVStopCommand : public CVConnectorCommand
    {
        CVStopCommand(CVConnector& conn)
            : connector(conn)
        {}

        virtual void execute() override;

        CVConnector& connector;
    };

    struct CVCloseCommand : public CVConnectorCommand
    {
        CVCloseCommand(CVConnector& conn)
            : connector(conn)
        {}

        virtual void execute() override;

        CVConnector& connector;
    };

    struct CVConnectorFactory
    {
        virtual ~CVConnectorFactory() = default;
        virtual CVConnectorState* set_state(CVConnector& connector) = 0;
    };

    struct CVCommand
    {
        enum command_value {run = 0, stop = 1, close = 2} command;
        CVCommand(QJsonObject& json_object);
        inline bool operator== (const CVCommand::command_value& desired_command) {
            return command == desired_command;
        }
    };

    class CVConnector : public QObject
    {
    Q_OBJECT
    public:
        CVConnector(unsigned index, CVConnectorFactory& f, CVProcessManager& process_manager, CVNetworkManager& nm, QTcpSocket& sock);

        virtual ~CVConnector();

        virtual void run();
        virtual void stop();
        virtual void close() = 0;

        void send_buffer(QByteArray byte_arr);

        void state_changed();

        unsigned get_id();

    signals:
        void notify_supervisors(CVConnectorState&);

    public slots:
        void process_incoming_message();

    private:
        unsigned id;
        QTcpSocket& tcp_socket;
        CVInputProcessor receiver;
        CVConnectorFactory& factory;
        std::unique_ptr<CVConnectorState> state;

    protected:
        bool running;
        CVNetworkManager& network_manager;
        CVProcessManager& process_manager;

    public:
        friend class CVClientFactory;
        friend class CVSupervisorFactory;
    };
}

#endif // CVCONNECTOR_H
