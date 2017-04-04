#ifndef CVAPPLICATION_H
#define CVAPPLICATION_H

#include <QObject>
#include "cvgraphnode.h"
#include "cvnetworkmanager.h"
#include "cvprocessmanager.h"
#include "cvjsoncontroller.h"
#include "cvfactorycontroller.h"

namespace CVKernel {

    class CVApplication : public QObject
    {
        Q_OBJECT
    public:
        explicit CVApplication(QString app_settings_json);

    private:
        CVNetworkManager* network_manager;
        CVProcessManager* process_manager;
    };
}

#endif // CVAPPLICATION_H
