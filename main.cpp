#include <QApplication>
#include "cvapplication.h"
#include <unistd.h>
#include "videoproc/firedetectionfactory.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    qRegisterMetaType<QSharedPointer<CVKernel::CVProcessData>>("QSharedPointer<CVProcessData>");
    qRegisterMetaType<QSharedPointer<CVKernel::CVProcessForest>>("QSharedPointer<CVProcessForest>");
    QList<CVKernel::CVNodeFactory*> factories = { new FireDetectionFactory };
    CVKernel::CVFactoryController::get_instance().set_factories(factories);
    CVKernel::CVApplication* cv_kernel(new CVKernel::CVApplication("../cv_kernel_settings.json"));
    return app.exec();
}
