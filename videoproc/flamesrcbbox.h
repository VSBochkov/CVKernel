#ifndef FLAMESRCBBOX_H
#define FLAMESRCBBOX_H

#include "firebbox.h"

class FlameSrcBBox: public FireBBox
{
public:
    explicit FlameSrcBBox(QObject* parent = 0);
    virtual QSharedPointer<CVKernel::CVNodeData> compute(CVKernel::CVProcessData &process_data);
};

#endif // FLAMESRCBBOX_H
