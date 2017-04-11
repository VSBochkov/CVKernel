#include "cvsupervisor.h"
#include "cvclient.h"
#include "cvconnector.h"
#include "cvjsoncontroller.h"
#include "cvnetworkmanager.h"
#include "cvprocessmanager.h"
#include "cvgraphnode.h"
#include "cvjsoncontroller.h"


CVKernel::CVConnectorState* CVKernel::CVSupervisorFactory::set_state(CVConnector& connector)
{
    CVSupervisor& sup = static_cast<CVSupervisor&>(connector);

    if (&sup == nullptr)
    {
        return new CVSupervisorClosed(sup);
    }
    else if (sup.running)
    {
        return new CVConnectorRun(sup);
    }
    else
    {
        return new CVConnectorReady(sup);
    }
}

CVKernel::CVSupervisorStartup::CVSupervisorStartup(QList<QSharedPointer<CVClient>> clients)
    : client_list(clients)
{
}

CVKernel::CVSupervisor::CVSupervisor(unsigned int id, CVProcessManager& pm, CVNetworkManager& nm, QTcpSocket& sock)
    : CVConnector(id, *CVSupervisorFactory::instance(), pm, nm, sock)
{
    state_changed();
    send_buffer(CVJsonController::pack_to_json_ascii<CVSupervisorStartup>(CVSupervisorStartup(network_manager.get_clients())));
    updateSupInfoTimer = new QTimer(&sock);
    QObject::connect(updateSupInfoTimer, SIGNAL(timeout()), this, SLOT(send_supervision_info()));
    for (QSharedPointer<CVClient> client : network_manager.get_clients())
    {
        QObject::connect(client.data(), SIGNAL(notify_supervisors(CVConnector&,CVConnectorState&)), this, SLOT(send_connector_state_change(CVConnector&,CVConnectorState&)));
    }
    for (QSharedPointer<CVSupervisor> sup : network_manager.get_supervisors())
    {
        QObject::connect(sup.data(), SIGNAL(notify_supervisors(CVConnector&,CVConnectorState&)), this, SLOT(send_connector_state_change(CVConnector&,CVConnectorState&)));
    }
}

CVKernel::CVSupervisor::~CVSupervisor()
{
    for (QSharedPointer<CVClient> client : network_manager.get_clients())
    {
        QObject::disconnect(client.data(), SIGNAL(notify_supervisors(CVConnector&,CVConnectorState&)), this, SLOT(send_connector_state_change(CVConnector&,CVConnectorState&)));
    }
    for (QSharedPointer<CVSupervisor> sup : network_manager.get_supervisors())
    {
        QObject::disconnect(sup.data(), SIGNAL(notify_supervisors(CVConnector&,CVConnectorState&)), this, SLOT(send_connector_state_change(CVConnector&,CVConnectorState&)));
    }
}

void CVKernel::CVSupervisor::stop()
{
    updateSupInfoTimer->stop();
    CVConnector::stop();
}

void CVKernel::CVSupervisor::run()
{
    updateSupInfoTimer->start(updateTimerValue);
    CVConnector::run();
}

void CVKernel::CVSupervisor::close()
{
//    tcp_socket.close();
}

void CVKernel::CVSupervisor::send_supervision_info()
{
    send_buffer(
        CVJsonController::pack_to_json_ascii<CVSupervisionInfo>(
            CVSupervisionInfo(
                info,
                network_manager.get_clients().size(),
                process_manager.get_average_timings()
            )
        )
    );
}

void CVKernel::CVSupervisor::send_connector_state_change(CVConnectorState& state)
{
    send_buffer(CVJsonController::pack_to_json_ascii<CVConnectorState>(&state));
}

CVKernel::CVSupervisionInfo::CVSupervisionInfo(CVSupervisionInfo& prev_info, int clients, QMap<QString, double> timings)
    : connected_clients(clients)
{
    used_process_nodes = timings.size();
    average_timings = timings;
    prev_info = *this;
}


QJsonObject CVKernel::CVSupervisorStartup::pack_to_json() {
    QJsonObject clients_json;
    QJsonArray clients_json_array;
    std::unique_ptr<CVConnectorState> state;
    for (QSharedPointer<CVClient>& client : client_list)
    {
        QJsonObject client_json;
        state.reset(CVClientFactory::instance()->set_state(*client));
        client_json["id"] = (int) client->get_id();
        client_json["state"] = (int) state->state;
        client_json["used_proc_nodes"] = client->get_proc_nodes_number();
        client_json["overlay_path"] = client->get_overlay_path();
        clients_json_array.push_back(client_json);
    }
    clients_json["clients"] = clients_json_array;
    return clients_json;
}

QJsonObject CVKernel::CVSupervisionInfo::pack_to_json() {
    QJsonObject sup_json;
    sup_json["connected_clients"] = connected_clients;
    sup_json["used_process_nodes"] = used_process_nodes;
    QJsonObject timings_json;
    for (auto iter = average_timings.begin(); iter != average_timings.end(); iter++)
    {
        timings_json[iter.key()] = iter.value();
    }
    sup_json["average_timings"] = timings_json;
    return sup_json;
}
