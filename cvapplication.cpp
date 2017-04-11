#include "cvapplication.h"
#include "cvjsoncontroller.h"

#include <QThread>
#include <array>
#include <stdio.h>


CVKernel::CVApplicationState::CVApplicationState(QString mac, bool st)
    : mac_address(mac),
      state(st)
{
    const int buf_size = 16;
    char buffer[buf_size];
    FILE* pipe = popen("hostname -I", "r");
    if (pipe and fgets(buffer, buf_size, pipe))
    {
        ip_address = buffer;
        pclose(pipe);
    }
}

CVKernel::CVApplication::CVApplication(QString app_settings_json) : QObject(NULL)
{
    net_settings = CVJsonController::get_from_json_file<CVNetworkSettings>(app_settings_json);

    QThread *proc_manager_thread = new QThread(this);
    process_manager = new CVProcessManager(this);
    process_manager->moveToThread(proc_manager_thread);

    network_manager = new CVNetworkManager(*net_settings, *process_manager);

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(send_self_to_broadcast()));
    proc_manager_thread->start();
}

CVKernel::CVApplication::~CVApplication()
{
    broadcaster->writeDatagram(
        CVJsonController::pack_to_json_ascii<CVApplicationState>(CVApplicationState(net_settings->mac_address, false)),
        QHostAddress::Broadcast, net_settings->udp_broadcast_port
    );
}

void CVKernel::CVApplication::send_self_to_broadcast()
{
    broadcaster->writeDatagram(
        CVJsonController::pack_to_json_ascii<CVApplicationState>(CVApplicationState(net_settings->mac_address, true)),
        QHostAddress::Broadcast, net_settings->udp_broadcast_port
    );
}
