#ifndef CVCLIENT_H
#define CVCLIENT_H

#include <QJsonObject>
#include <QByteArray>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>
#include <QMap>

#include <memory>

#include "cvconnector.h"

namespace CVKernel {
    struct CVIONode;
    struct CVProcessForest;
    struct CVProcessManager;
    struct CVNetworkManager;


    struct CVClientFactory : public CVConnectorFactory
    {
        CVClientFactory(const CVClientFactory&) = delete;
        CVClientFactory& operator==(const CVClientFactory&) = delete;
        virtual ~CVClientFactory() = default;

        virtual CVConnectorState* set_state(CVConnector& connector) override;
        static CVClientFactory* instance()
        {
            static CVClientFactory inst;
            return &inst;
        }

    private:
        CVClientFactory() {}
    };

    class CVClient : public CVConnector
    {
    Q_OBJECT
    public:
        explicit CVClient(unsigned index, CVProcessManager& pm, CVNetworkManager& nm, QTcpSocket& tcp);
        virtual ~CVClient();

        void set_io_node(CVIONode& io_node);
        QString get_overlay_path();
        int get_proc_nodes_number();
        void init_process_tree(QSharedPointer<CVProcessForest> proc_forest);

        virtual void run() override;
        virtual void stop() override;
        virtual void close() override;

    signals:
        void client_disabled(QTcpSocket& tcp_state);

    public slots:
        void send_datagramm(QByteArray byte_arr);
        void do_close_udp();
        void on_stream_closed();

    private:
        std::unique_ptr<QUdpSocket> meta_udp_client;
        QHostAddress udp_address;
        qint16 udp_port;
        CVIONode* video_io;
        friend class CVClientFactory;
    };

    struct CVInitProcTreeCommand : public CVConnectorCommand
    {
        CVInitProcTreeCommand(CVClient& cl, QSharedPointer<CVProcessForest> pf)
            : client(cl),
              proc_forest(pf)
        {}

        virtual void execute() override;

        CVClient& client;
        QSharedPointer<CVProcessForest> proc_forest;
    };

    struct CVClientClosed : public CVConnectorClosed
    {
        CVClientClosed(CVClient& c)
            : CVConnectorClosed(c)
        {}

        virtual void handleIncommingMessage(QByteArray& buffer) override;
    };
}

#endif // CVCLIENT_H
