#include <QApplication>
#include <cstdlib>
#include <unistd.h>
#include "cvapplication.h"
#include "videoproc/firedetectionfactory.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    qRegisterMetaType<QSharedPointer<CVKernel::CVProcessData>>("QSharedPointer<CVProcessData>");
    qRegisterMetaType<QSharedPointer<CVKernel::CVProcessForest>>("QSharedPointer<CVProcessForest>");
    QList<CVKernel::CVNodeFactory*> factories = { new FireDetectionFactory };
    CVKernel::CVFactoryController::get_instance().set_factories(factories);
    qDebug() << "CVKERNEL pid = " << ::getpid();
    int result = CVKernel::CVApplication::setup_unix_signal_handlers();
    if (result > 0) {
        return result;
    }
    auto cv_app = new CVKernel::CVApplication(std::getenv("CVKERNEL_PATH") + QString("/cv_kernel_settings.json"));
    QObject::connect(cv_app, SIGNAL(destroyed()), &app, SLOT(quit()));
    return app.exec();
}
