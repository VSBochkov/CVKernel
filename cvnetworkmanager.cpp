#include "cvnetworkmanager.h"
#include "cvjsoncontroller.h"
#include "cvprocessmanager.h"
#include "cvgraphnode.h"
#include "cvconnector.h"

#include <QObject>


CVKernel::CVNetworkManager::CVNetworkManager(CVNetworkManagerSettings& settings) :
    QObject(NULL), process_manager(nullptr)
{
    receiver = new CVInputProcessor;
    setup_tcp_server(settings);
}

CVKernel::CVNetworkManager::~CVNetworkManager()
{
    delete receiver;
}

void CVKernel::CVNetworkManager::setup_tcp_server(CVNetworkManagerSettings& settings) {
    tcp_server = new QTcpServer(this);
    tcp_server->listen(QHostAddress::Any, settings.tcp_server_port);
    connect(tcp_server, SIGNAL(newConnection()), this, SLOT(connect_new_client()));
}

void CVKernel::CVNetworkManager::connect_new_client() {
    QTcpSocket *new_connection = tcp_server->nextPendingConnection();
    if (not new_connection->isValid())
        return;

    connect(new_connection, SIGNAL(readyRead()), this, SLOT(recv_client_data()));
}

void CVKernel::CVNetworkManager::recv_client_data()
{
    QTcpSocket* connection = (QTcpSocket*) sender();
    QMap<QTcpSocket*, CVConnector*>::iterator connector_it = connectors.find(connection);
    if (connector_it == connectors.end())
    {
        connector_it.value()->process_incoming_message();
    }
    else if (receiver->receive_message(connection))
    {
        add_new_connector(connection);
        receiver->clear_buffer();
    }
}

void CVKernel::CVNetworkManager::add_new_connector(QTcpSocket* socket)
{
    QSharedPointer<CVConnectorType> connector = CVJsonController::get_from_json_buffer<CVConnectorType>(receiver->buffer);
    if (connector->type == CVConnectorType::client) {
        CVClient* new_client = new CVClient(connector_id++, this, socket);
        new_client->send_buffer(CVJsonController::pack_to_json_ascii<CVClientState>(CVClientState(new_client)));
        connectors[socket] = new_client;
    } else if (connector->type == CVConnectorType::supervisor) {
        CVSupervisor* new_supervisor = new CVSupervisor(connector_id++, process_manager, this, socket);
        new_supervisor->send_buffer(CVJsonController::pack_to_json_ascii<CVSupervisorState>(CVSupervisorState(new_supervisor)));
        new_supervisor->send_buffer(CVJsonController::pack_to_json_ascii<CVSupervisorStartup>(CVSupervisorStartup(connectors)));
        connectors[socket] = new_supervisor;
    }
}

void CVKernel::CVNetworkManager::activate_new_client(QTcpSocket *connection, QSharedPointer<CVProcessForest> proc_forest)
{
    emit got_process_forest(connection, proc_forest);
    connect(process_manager, SIGNAL(io_node_created(QTcpSocket*, CVIONode*)), this, SLOT(on_io_node_created(QTcpSocket*, CVIONode*)));
    connect(process_manager, SIGNAL(io_node_stopped(CVIONode*)), this, SLOT(on_io_node_stopped(CVIONode*)));
    connect(process_manager, SIGNAL(io_node_started(CVIONode*)), this, SLOT(on_io_node_started(CVIONode*)));
    connect(process_manager, SIGNAL(io_node_closed(CVIONode*)), this, SLOT(on_io_node_closed(CVIONode*)));
}

void CVKernel::CVNetworkManager::transmit_metadata(QByteArray metadata)
{
    if (metadata.isEmpty()) {
        return;
    }
    CVIONode* io_node = (CVIONode*) sender();
    CVClient* client = static_cast<CVClient*>(connectors[ionode_to_tcp_map[io_node]]);
    client->send_datagramm(metadata);
}

void CVKernel::CVNetworkManager::on_io_node_created(QTcpSocket *tcp_socket, CVIONode *new_io_node)
{
    ionode_to_tcp_map[new_io_node] = tcp_socket;
    CVClient* client = static_cast<CVClient*>(connectors[tcp_socket]);
    client->video_io = new_io_node;
    connect(new_io_node, SIGNAL(send_metadata(QByteArray)), this, SLOT(transmit_metadata(QByteArray)));
    connect(new_io_node, SIGNAL(close_udp()), this, SLOT(do_close_udp()));
    client->send_buffer(CVJsonController::pack_to_json_ascii<CVClientState>(CVClientState(client)));
    propagate_to_supervisors(CVJsonController::pack_to_json_ascii<CVClientChangedParams>(CVClientChangedParams(client)));
}

void CVKernel::CVNetworkManager::do_close_udp()
{
    CVIONode* io_node = (CVIONode*) sender();
    CVClient* client = static_cast<CVClient*>(connectors[ionode_to_tcp_map[io_node]]);
    client->send_datagramm(QByteArray());
    emit udp_closed((CVIONode*) sender());
}

void CVKernel::CVNetworkManager::on_io_node_started(CVIONode* io_node)
{
    QTcpSocket* tcp_socket = ionode_to_tcp_map[io_node];
    CVClient* client = static_cast<CVClient*>(connectors[tcp_socket]);
    client->run = true;
    client->send_buffer(CVJsonController::pack_to_json_ascii<CVClientState>(CVClientState(client)));
    propagate_to_supervisors(CVJsonController::pack_to_json_ascii<CVClientChangedParams>(CVClientChangedParams(client)));
}

void CVKernel::CVNetworkManager::on_io_node_stopped(CVIONode* io_node)
{
    QTcpSocket* tcp_socket = ionode_to_tcp_map[io_node];
    CVClient* client = static_cast<CVClient*>(connectors[tcp_socket]);
    client->run = false;
    client->send_buffer(CVJsonController::pack_to_json_ascii<CVClientState>(CVClientState(client)));
    propagate_to_supervisors(CVJsonController::pack_to_json_ascii<CVClientChangedParams>(CVClientChangedParams(client)));
}

void CVKernel::CVNetworkManager::on_io_node_closed(CVIONode* io_node)
{
    auto io_to_tcp_it = ionode_to_tcp_map.find(io_node);   /*io_node is clearing but pointer value is not nullptr*/
    QTcpSocket* tcp_socket = io_to_tcp_it.value();
    CVClient* client = static_cast<CVClient*>(connectors[tcp_socket]);
    client->video_io = nullptr;
    ionode_to_tcp_map.erase(io_to_tcp_it);
    client->send_buffer(CVJsonController::pack_to_json_ascii<CVClientState>(CVClientState(client)));
    propagate_to_supervisors(CVJsonController::pack_to_json_ascii<CVClientChangedParams>(CVClientChangedParams(client)));
}

int CVKernel::CVNetworkManager::get_clients_number()
{
    int number = 0;
    for (CVConnector* connector: connectors) {
        if (dynamic_cast<CVClient*>(connector) != nullptr) {
            number++;
        }
    }
    return number;
}

void CVKernel::CVNetworkManager::propagate_to_supervisors(QByteArray buffer)
{
    for (CVConnector* connector: connectors) {
        if (dynamic_cast<CVSupervisor*>(connector) != nullptr) {
            connector->send_buffer(buffer);
        }
    }
}
