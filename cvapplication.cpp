#include "cvapplication.h"
#include "cvjsoncontroller.h"

#include <QThread>
#include <QNetworkInterface>
#include <array>

#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

static int sig_usr1_fd[2];
static int sig_term_fd[2];

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
    network_manager = new CVNetworkManager(this, *net_settings, process_manager);

    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sig_usr1_fd)) {
       qFatal("Couldn't create USR1 socketpair");
    }
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sig_term_fd)) {
       qFatal("Couldn't create TERM socketpair");
    }

    sn_usr1 = new QSocketNotifier(sig_usr1_fd[1], QSocketNotifier::Read, this);
    connect(sn_usr1, SIGNAL(activated(int)), this, SLOT(shutdown()));
    sn_term = new QSocketNotifier(sig_term_fd[1], QSocketNotifier::Read, this);
    connect(sn_term, SIGNAL(activated(int)), this, SLOT(reboot()));
}

CVKernel::CVApplication::~CVApplication()
{
}

int CVKernel::CVApplication::setup_unix_signal_handlers()
{
    if (signal(SIGUSR1, CVKernel::CVApplication::usr1_handler) == SIG_ERR)
        return 1;
    if (signal(SIGTERM, CVKernel::CVApplication::term_handler) == SIG_ERR)
        return 2;
    return 0;
}

void CVKernel::CVApplication::usr1_handler(int)
{
    qDebug() << "SIGUSR1 handler";
    char a = 1;
    if (::write(sig_usr1_fd[0], &a, sizeof(a)) < 0)
    {
        qDebug() << "SIGUSR1 handler error: " << errno;
    }
}

void CVKernel::CVApplication::term_handler(int)
{
    qDebug() << "SIGTERM handler";
    char a = 1;
    if (::write(sig_term_fd[0], &a, sizeof(a)) < 0)
    {
        qDebug() << "SIGTERM handler error: " << errno;
    }
}

void CVKernel::CVApplication::reboot()
{
    sn_term->setEnabled(false);
    char tmp;
    ::read(sig_term_fd[1], &tmp, sizeof(tmp));

    network_manager->close_all_connectors();

    sn_term->setEnabled(true);
}

void CVKernel::CVApplication::shutdown()
{
    sn_term->setEnabled(false);
    char tmp;
    ::read(sig_usr1_fd[1], &tmp, sizeof(tmp));

    for (CVProcessingNode* node: process_manager.processing_nodes)
    {
        connect(network_manager, SIGNAL(all_connectors_closed()), node->thread(), SLOT(quit()));
    }
    connect(network_manager, SIGNAL(all_connectors_closed()), this, SLOT(deleteLater()));
    network_manager->close_all_connectors();

    sn_usr1->setEnabled(true);
}
