#include "cvconnector.h"
#include "cvgraphnode.h"
#include "cvprocessmanager.h"
#include "cvnetworkmanager.h"
#include "cvjsoncontroller.h"


CVKernel::CVClientChangedParams::CVClientChangedParams(CVClient *client)
    : CVConnectorChangedParams(client),
      state(client),
      overlay_path(client->video_io->get_overlay_path()) {}

void CVKernel::CVConnector::send_buffer(QByteArray byte_arr)
{
    quint64 arr_size = (quint64) byte_arr.size();
    char size[sizeof(quint64)];
    for (uint i = 0; i < sizeof(quint64); ++i) {
        size[i] = (char)((arr_size >> (i * 8)) & 0xff);
    }
    QByteArray packet = QByteArray::fromRawData(size, sizeof(quint64)) + byte_arr;
    quint64 bytes_to_send = packet.size();
    quint64 bytes_sent = 0;
    while (bytes_sent < bytes_to_send) {
        qint64 bytes = tcp_socket->write(packet);
        packet.mid((int) bytes);
        bytes_sent += bytes;
    }
}

void CVKernel::CVClient::send_datagramm(QByteArray byte_arr)
{
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

bool CVKernel::CVInputProcessor::receive_message(QTcpSocket *tcp_socket)
{
    if (state == recv_state::GET_SIZE) {
        buffer += tcp_socket->readAll();
        if ((uint) buffer.size() < sizeof(quint64))
            return false;

        buffer_size = 0;
        for (uint i = 0; i < sizeof(quint64); ++i) {
            buffer_size += ((uchar) buffer[i]) << (i * 8);
        }

        buffer = buffer.mid(sizeof(quint64));
        bytes_received = buffer.size();
        if (bytes_received < buffer_size) {
            state = recv_state::GET_BUFFER;
        }
    } else if (state == recv_state::GET_BUFFER) {
        QByteArray bytes_read = tcp_socket->readAll();
        bytes_received += bytes_read.size();
        buffer += bytes_read;
    }

    return (bytes_received == buffer_size);
}

void CVKernel::CVInputProcessor::clear_buffer()
{
    buffer.clear();
    buffer_size = 0;
    state = recv_state::GET_SIZE;
}

CVKernel::CVClientState::CVClientState(CVClient* client)
{
    state = (client->video_io == nullptr) ? closed : (client->run ? run : ready);
}

void CVKernel::CVClient::process_incoming_message() {
    if (not receiver.receive_message(tcp_socket))
        return;

    QByteArray& incoming_data = receiver.buffer;
    CVClientState client_state(this);
    switch (client_state.state) {
        case CVClientState::state_value::closed: {
            QSharedPointer<CVProcessForest> proc_forest = CVJsonController::get_from_json_buffer<CVProcessForest>(incoming_data);
            if (not proc_forest->proc_forest.empty()) {
                network_manager->activate_new_client(tcp_socket, proc_forest);
                init_udp_connection(proc_forest);
            }
            break;
        }
        case CVClientState::state_value::ready: {
            QSharedPointer<CVCommand> command = CVJsonController::get_from_json_buffer<CVCommand>(incoming_data);
            if (*command == CVCommand::close) {
                network_manager->close(video_io);
            } else if (*command == CVCommand::run) {
                network_manager->run(video_io);
            }
            break;
        }
        case CVClientState::state_value::run: {
            QSharedPointer<CVCommand> command = CVJsonController::get_from_json_buffer<CVCommand>(incoming_data);
            if (*command == CVCommand::stop) {
                network_manager->stop(video_io);
            }
        }
    }
    receiver.clear_buffer();
}

void CVKernel::CVClient::init_udp_connection(QSharedPointer<CVProcessForest> proc_forest)
{
    meta_udp_client = new QUdpSocket();
    udp_address = QHostAddress(proc_forest->meta_udp_addr);
    udp_port = proc_forest->meta_udp_port;
}

CVKernel::CVSupervisor::CVSupervisor(unsigned int id, CVProcessManager* pm, CVNetworkManager* nm, QTcpSocket* sock)
    : QObject(sock),
      CVConnector(id, nm, sock),
      process_manager(pm)
{
    updateSupInfoTimer = new QTimer(sock);
    connect(updateSupInfoTimer, SIGNAL(timeout()), this, SLOT(send_supervision_info()));
}

CVKernel::CVSupervisorState::CVSupervisorState(CVSupervisor* supervisor)
{
    state = (supervisor == nullptr) ? closed : (supervisor->run ? run : ready);
}

void CVKernel::CVSupervisor::process_incoming_message()
{
    if (not receiver.receive_message(tcp_socket))
        return;

    QByteArray& incoming_data = receiver.buffer;
    QSharedPointer<CVCommand> command = CVJsonController::get_from_json_buffer<CVCommand>(incoming_data);

    if (run and *command == CVCommand::stop) {
        stop_update_timer();
        send_buffer(CVJsonController::pack_to_json_ascii<CVSupervisorState>(CVSupervisorState(this)));
        network_manager->propagate_to_supervisors(CVJsonController::pack_to_json_ascii<CVSupervisorChangedParams>(this));
    } else if (*command == CVCommand::run) {
        init_update_timer();
        send_buffer(CVJsonController::pack_to_json_ascii<CVSupervisorState>(CVSupervisorState(this)));
        network_manager->propagate_to_supervisors(CVJsonController::pack_to_json_ascii<CVSupervisorChangedParams>(this));
    }
    receiver.clear_buffer();
}

void CVKernel::CVSupervisor::init_update_timer()
{
    updateSupInfoTimer->start(updateTimerValue);
    run = true;
}

void CVKernel::CVSupervisor::send_supervision_info()
{
    send_buffer(
        CVJsonController::pack_to_json_ascii<CVSupervisionInfo>(
            CVSupervisionInfo(
                info,
                network_manager->get_clients_number(),
                process_manager->get_average_timings()
            )
        )
    );
}

void CVKernel::CVSupervisor::stop_update_timer()
{
    updateSupInfoTimer->stop();
    run = false;
}

CVKernel::CVSupervisionInfo::CVSupervisionInfo(CVSupervisionInfo& prev_info, int clients, QMap<QString, double> timings)
    : connected_clients(clients)
{
    used_process_nodes = timings.size();
    average_timings = timings;
    prev_info = *this;
}
