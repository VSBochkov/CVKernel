#include "cvclient.h"
#include "cvsupervisor.h"
#include "cvconnector.h"
#include "cvnetworkmanager.h"
#include "cvprocessmanager.h"
#include "cvgraphnode.h"
#include "cvjsoncontroller.h"

#include <QObject>
#include <memory>

CVKernel::CVConnectorState* CVKernel::CVClientFactory::set_state(CVConnector& connector)
{
    CVClient& client = static_cast<CVClient&>(connector);
    if (client.video_io == nullptr)
    {
        return new CVClientClosed(client);
    }
    else if (client.running)
    {
        return new CVConnectorRun(connector);
    }
    else
    {
        return new CVConnectorReady(connector);
    }
}

CVKernel::CVClient::CVClient(unsigned index, CVProcessManager& pm, CVNetworkManager& nm, QTcpSocket& tcp)
    : CVConnector(index, *CVClientFactory::instance(), pm, nm, tcp)
{
    video_io = nullptr;
    meta_udp_client.reset(new QUdpSocket);

    for (QSharedPointer<CVSupervisor> sup : network_manager.get_supervisors())
    {
        QObject::connect(this, SIGNAL(notify_supervisors(CVConnectorState&)), sup.data(), SLOT(send_connector_state_change(CVConnectorState&)));
    }

    state_changed();
    qDebug() << "added CVClient id: " << id;
}

CVKernel::CVClient::~CVClient()
{
    for (QSharedPointer<CVSupervisor> sup : network_manager.get_supervisors())
    {
        QObject::disconnect(this, SIGNAL(notify_supervisors(CVConnectorState&)), sup.data(), SLOT(send_connector_state_change(CVConnectorState&)));
    }
    qDebug() << "deleted CVClient id: " << id;
}

QString CVKernel::CVClient::get_overlay_path()
{
    return video_io->get_overlay_path();
}

int CVKernel::CVClient::get_proc_nodes_number()
{
    return video_io->nodes_number;
}

void CVKernel::CVClient::close()
{
    video_io->close();
}

void CVKernel::CVClient::run()
{
    video_io->start();
    CVConnector::run();
}

void CVKernel::CVClient::stop()
{
    video_io->stop();
    CVConnector::stop();
}

void CVKernel::CVClient::init_process_tree(QSharedPointer<CVProcessForest> proc_forest)
{
    video_io = process_manager.add_new_stream(proc_forest);
    udp_address = proc_forest->meta_udp_addr;
    udp_port = proc_forest->meta_udp_port;
    connect(video_io, SIGNAL(node_closed()), this, SLOT(on_stream_closed()));
    connect(video_io, SIGNAL(send_metadata(QByteArray)), this, SLOT(send_datagramm(QByteArray)));
    connect(video_io, SIGNAL(close_udp()), this, SLOT(do_close_udp()));
}

void CVKernel::CVClient::on_stream_closed()
{
    if (running == true)
    {
        running = false;
        state_changed();
    }

    video_io = nullptr;
    state_changed();
    emit client_disabled(tcp_state);
}

void CVKernel::CVClient::send_datagramm(QByteArray byte_arr)
{
    if (byte_arr.isEmpty()) {
        return;
    }
    quint64 arr_size = (quint64) byte_arr.size();
    char size[sizeof(quint64)];
    for (uint i = 0; i < sizeof(quint64); ++i) {
        size[i] = (char)((arr_size >> (i * 8)) & 0xff);
    }

    meta_udp_client->writeDatagram(QByteArray::fromRawData(size, sizeof(quint64)), udp_address, udp_port);
    quint64 bytes_to_send = byte_arr.size(), bytes_sent = 0;
    while (bytes_sent < bytes_to_send) {
        meta_udp_client->waitForBytesWritten(-1);
        qint64 bytes = meta_udp_client->writeDatagram(byte_arr, udp_address, udp_port);
        byte_arr.mid((int) bytes);
        bytes_sent += bytes;
    }
}

void CVKernel::CVClient::do_close_udp()
{
    send_datagramm(QByteArray("{}"));
    video_io->udp_closed();
}

void CVKernel::CVClientClosed::handleIncommingMessage(QByteArray &buffer)
{
    QSharedPointer<CVProcessForest> proc_forest = CVJsonController::get_from_json_buffer<CVProcessForest>(buffer);
    proc_forest->meta_udp_addr = connector.get_ip_address();
    if (not proc_forest->proc_forest.empty())
    {
        command.reset(new CVInitProcTreeCommand(static_cast<CVClient&>(connector), proc_forest));
    }
}

void CVKernel::CVInitProcTreeCommand::execute()
{
    client.init_process_tree(proc_forest);
}
