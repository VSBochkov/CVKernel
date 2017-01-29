#include <QApplication>
#include "cvgraphnode.h"
#include "cvprocessmanager.h"
#include <unistd.h>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    std::string json_file;
    if (argc > 1) {
        if (strcmp(argv[1], "off") == 0)
            json_file = "../../FireRobotDriver/fire_overlay_off";
        else
            json_file = "../../FireRobotDriver/fire_overlay_on";
    } else
        json_file = "../../FireRobotDriver/fire_overlay_on";

    if (argc > 2) {
        if (strcmp(argv[2], "rpi") == 0)
            json_file += "_rpi.json";
        else
            json_file += "_pc.json";
    } else
        json_file += "_pc.json";

    qRegisterMetaType<CVKernel::CVProcessData>("CVProcessData");
    CVKernel::CVProcessManager *manager(new CVKernel::CVProcessManager());
    CVKernel::CVForestParser process = CVKernel::CVForestParser()
            .parseJSON(json_file.c_str());
    manager->addNewStream(process);
    for (CVKernel::CVProcessTree* tree : process.get_forest()) delete tree;
    process.get_forest().clear();

    return app.exec();
}
