#include "cvconnector.h"
#include "cvgraphnode.h"
#include "cvprocessmanager.h"
#include "cvnetworkmanager.h"
#include "cvjsoncontroller.h"


CVKernel::CVConnector::CVConnector(unsigned index, CVConnectorFactory& f, CVProcessManager& pm, CVNetworkManager& nm, QTcpSocket& sock)
    : QObject(&sock),
      id(index),
      tcp_socket(sock),
      factory(f),
      running(false),
      network_manager(nm),
      process_manager(pm)
{
    disconnect(&sock, SIGNAL(readyRead()), &network_manager, SLOT(recv_data()));
    connect(&sock, SIGNAL(readyRead()), this, SLOT(process_incoming_message()));
}

CVKernel::CVConnector::~CVConnector()
{
    disconnect(&tcp_socket, SIGNAL(readyRead()), this, SLOT(recv_data()));
    connect(&tcp_socket, SIGNAL(readyRead()), &network_manager, SLOT(recv_data()));
}

void CVKernel::CVConnector::process_incoming_message()
{
    if (not receiver.receive_message(tcp_socket))
        return;

    state->handleIncommingMessage(receiver.buffer);
    if (state->command.get())
        state->command->execute();

    receiver.clear_buffer();
}

void CVKernel::CVConnector::state_changed()
{
    state.reset(factory.set_state(*this));
    send_buffer(CVJsonController::pack_to_json_ascii<CVConnectorState>(state.get()));
    emit notify_supervisors(*state);
}

void CVKernel::CVConnectorRun::handleIncommingMessage(QByteArray &buffer)
{
    QSharedPointer<CVCommand> com = CVJsonController::get_from_json_buffer<CVCommand>(buffer);
    if (*com == CVCommand::stop)
    {
        command.reset(new CVStopCommand(connector));
    }
}

void CVKernel::CVConnectorReady::handleIncommingMessage(QByteArray &buffer)
{
    QSharedPointer<CVCommand> com = CVJsonController::get_from_json_buffer<CVCommand>(buffer);
    switch (com->command)
    {
        case CVCommand::run:
            command.reset(new CVRunCommand(connector));
            break;
        case CVCommand::close:
            command.reset(new CVCloseCommand(connector));
            break;
        default:
            break;
    }
}

void CVKernel::CVRunCommand::execute()
{
    connector.run();
}

void CVKernel::CVStopCommand::execute()
{
    connector.stop();
}

void CVKernel::CVCloseCommand::execute()
{
    connector.close();
}

void CVKernel::CVConnector::run()
{
    running = true;
    state_changed();
}

void CVKernel::CVConnector::stop()
{
    running = false;
    state_changed();
}

unsigned CVKernel::CVConnector::get_id()
{
    return id;
}

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
        qint64 bytes = tcp_socket.write(packet);
        packet.mid((int) bytes);
        bytes_sent += bytes;
    }
}

bool CVKernel::CVInputProcessor::receive_message(QTcpSocket& tcp_socket)
{
    if (state == recv_state::GET_SIZE) {
        buffer += tcp_socket.readAll();
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
        QByteArray bytes_read = tcp_socket.readAll();
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
