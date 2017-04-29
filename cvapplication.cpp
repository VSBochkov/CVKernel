#include "cvapplication.h"
#include "cvjsoncontroller.h"

#include <QThread>
#include <QNetworkInterface>
#include <array>
#include <stdio.h>

QString CVKernel::get_ip_address()
{
    QString ip_address;
    foreach (const QHostAddress &address, QNetworkInterface::allAddresses())
    {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && address != QHostAddress(QHostAddress::LocalHost))
        {
            ip_address = address.toString();
            break;
        }
    }
    return ip_address;
}

CVKernel::CVApplication::CVApplication(QString app_settings_json) : QObject(NULL)
{
    net_settings = CVJsonController::get_from_json_file<CVNetworkSettings>(app_settings_json);

    QThread *proc_manager_thread = new QThread(this);
    process_manager = new CVProcessManager;
    process_manager->moveToThread(proc_manager_thread);
    network_manager = new CVNetworkManager(this, *net_settings, *process_manager);
    proc_manager_thread->start();
}

CVKernel::CVApplication::~CVApplication()
{
}
