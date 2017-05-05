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

    if (sup.supervision_port == 0)
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
    : CVConnector(id, *CVSupervisorFactory::instance(), pm, nm, sock),
      supervision_port(0)
{
    state_changed();
    updateSupInfoTimer = new QTimer(&sock);
    tcp_supervision = new QTcpSocket(&sock);
    QObject::connect(updateSupInfoTimer, SIGNAL(timeout()), this, SLOT(send_supervision_info()));
    for (QSharedPointer<CVClient> client : network_manager.get_clients())
    {
        connect(client.data(), SIGNAL(notify_supervisors(CVConnectorState&)), this, SLOT(send_connector_state_change(CVConnectorState&)));
    }
    qDebug() << "added CVSupervisor id: " << id;
}

CVKernel::CVSupervisor::~CVSupervisor()
{
    for (QSharedPointer<CVClient> client : network_manager.get_clients())
    {
        disconnect(client.data(), SIGNAL(notify_supervisors(CVConnectorState&)), this, SLOT(send_connector_state_change(CVConnectorState&)));
    }
    qDebug() << "deleted CVSupervisor id: " << id;
}

void CVKernel::CVSupervisor::init_supervision(QSharedPointer<CVSupervisionSettings> settings)
{
    supervision_port = settings->port;
    update_timer_value = settings->update_timer_value;
    tcp_supervision->connectToHost(tcp_state.localAddress(), supervision_port);
    send_buffer(
        CVJsonController::pack_to_json_ascii<CVSupervisorStartup>(
            CVSupervisorStartup(network_manager.get_clients())
        ),
        *tcp_supervision
    );
}

void CVKernel::CVSupervisor::run()
{
    updateSupInfoTimer->start(update_timer_value * 1000);
    CVConnector::run();
}

void CVKernel::CVSupervisor::stop()
{
    updateSupInfoTimer->stop();
    CVConnector::stop();
}

void CVKernel::CVSupervisor::close()
{
    if (running)
    {
        stop();
    }
    send_buffer(QByteArray("{}"), *tcp_supervision);
    tcp_supervision->close();
    supervision_port = 0;
    state_changed();
}

void CVKernel::CVSupervisorClosed::handleIncommingMessage(QByteArray &buffer)
{
    QSharedPointer<CVSupervisionSettings> sup_settings = CVJsonController::get_from_json_buffer<CVSupervisionSettings>(buffer);
    if (sup_settings->port != 0)
    {
        command.reset(new CVInitSupervisionCommand(static_cast<CVSupervisor&>(connector), sup_settings));
    }
}

void CVKernel::CVInitSupervisionCommand::execute()
{
    supervisor.init_supervision(settings);
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
        ),
        *tcp_supervision
    );
}

void CVKernel::CVSupervisor::send_connector_state_change(CVConnectorState& state)
{
    send_buffer(CVJsonController::pack_to_json_ascii<CVConnectorState>(&state), *tcp_supervision);
}

CVKernel::CVSupervisionInfo::CVSupervisionInfo(CVSupervisionInfo& prev_info, int clients, QMap<QString, double> timings)
    : connected_clients(clients)
{
    used_process_nodes = timings.size();
    average_timings = timings;
    prev_info = *this;
}

