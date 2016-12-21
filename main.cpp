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
            json_file = "../fire_overlay_off.json";
        else
            json_file = "../fire_overlay_on.json";
    } else
        json_file = "../fire_overlay_on.json";

    qRegisterMetaType<CVKernel::CVProcessData>("CVProcessData");
    CVKernel::CVProcessManager *manager(new CVKernel::CVProcessManager());
    CVKernel::CVForestParser process = CVKernel::CVForestParser()
            .parseJSON(json_file.c_str());
    manager->addNewStream(process);
    for (CVKernel::CVProcessTree* tree : process.get_forest()) delete tree;
    process.get_forest().clear();

    return app.exec();
}
