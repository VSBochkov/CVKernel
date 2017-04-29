#include "cvnetworkmanager.h"
#include "cvjsoncontroller.h"
#include "cvprocessmanager.h"
#include "cvgraphnode.h"
#include "cvconnector.h"
#include "implementation/cvclient.h"
#include "implementation/cvsupervisor.h"

#include <QObject>


CVKernel::CVNetworkManager::CVNetworkManager(QObject* parent, CVNetworkSettings& settings, CVProcessManager& pm) :
    QObject(parent), process_manager(pm)
{
    receiver = new CVInputProcessor;
    setup_tcp_server(settings);
}

CVKernel::CVNetworkManager::~CVNetworkManager()
{
    delete receiver;
}

void CVKernel::CVNetworkManager::setup_tcp_server(CVNetworkSettings& settings) {
    tcp_server = new QTcpServer(this);
    tcp_server->listen(QHostAddress::Any, settings.tcp_server_port);
    connect(tcp_server, SIGNAL(newConnection()), this, SLOT(add_tcp_connection()));
}

void CVKernel::CVNetworkManager::add_tcp_connection() {
    QTcpSocket *new_connection = tcp_server->nextPendingConnection();
    if (not new_connection->isValid())
        return;

    connect(new_connection, SIGNAL(readyRead()), this, SLOT(init_new_conector()));
    connect(new_connection, SIGNAL(disconnected()), this, SLOT(delete_connector()));
}

void CVKernel::CVNetworkManager::init_new_conector()
{
    QTcpSocket* connection = (QTcpSocket*) sender();
    if (clients.find(connection) != clients.end() or supervisors.find(connection) != supervisors.end())
    {
        return;
    }

    if (receiver->receive_message(*connection))
    {
        add_new_connector(connection);
        receiver->clear_buffer();
    }
}

void CVKernel::CVNetworkManager::delete_connector()
{
    QTcpSocket* connection = (QTcpSocket*) sender();
    auto client_it = clients.find(connection);
    auto supervisor_it = supervisors.find(connection);
    if (client_it != clients.end())
    {
        client_it.value().clear();
        clients.erase(client_it);
    }
    else if(supervisor_it != supervisors.end())
    {
        supervisor_it.value().clear();
        supervisors.erase(supervisor_it);
    }

}

void CVKernel::CVNetworkManager::add_new_connector(QTcpSocket* socket)
{
    QSharedPointer<CVConnectorType> connector = CVJsonController::get_from_json_buffer<CVConnectorType>(receiver->buffer);
    if (connector->type == CVConnectorType::client) {
        QSharedPointer<CVClient> new_client(new CVClient(connector_id++, process_manager, *this, *socket));
        clients[socket] = new_client;
    } else if (connector->type == CVConnectorType::supervisor) {
        QSharedPointer<CVSupervisor> new_supervisor(new CVSupervisor(connector_id++, process_manager, *this, *socket));
        supervisors[socket] = new_supervisor;
    }
}

QList<QSharedPointer<CVKernel::CVClient>> CVKernel::CVNetworkManager::get_clients()
{
    return clients.values();
}

QList<QSharedPointer<CVKernel::CVSupervisor>> CVKernel::CVNetworkManager::get_supervisors()
{
    return supervisors.values();
}
