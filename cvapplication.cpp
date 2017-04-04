#include "cvapplication.h"
#include "cvjsoncontroller.h"

#include <QThread>

CVKernel::CVApplication::CVApplication(QString app_settings_json) : QObject(NULL)
{
    QSharedPointer<CVNetworkManagerSettings> net_settings = CVJsonController::get_from_json_file<CVNetworkManagerSettings>(app_settings_json);

    QThread *net_manager_thread = new QThread(this);
    network_manager = new CVNetworkManager(*net_settings);
    network_manager->moveToThread(net_manager_thread);

    process_manager = new CVProcessManager(this, network_manager);
    network_manager->set_process_manager(process_manager);

    net_manager_thread->start();
}
