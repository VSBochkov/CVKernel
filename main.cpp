#include <QApplication>
#include "cvgraphnode.h"
#include "cvprocessmanager.h"
#include <unistd.h>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    qRegisterMetaType<CVKernel::CVProcessData>("CVProcessData");
    CVKernel::CVProcessManager *manager(new CVKernel::CVProcessManager());
    CVKernel::CVForestParser process = CVKernel::CVForestParser()
            .parseJSON("/home/vbochkov/workspace/documents/FIRE/fire5.json");
    manager->addNewStream(process.get_video_in(), process.get_video_out(), process.get_forest());
    for (CVKernel::CVProcessTree* tree : process.get_forest()) delete tree;
    process.get_forest().clear();

    return app.exec();
}
