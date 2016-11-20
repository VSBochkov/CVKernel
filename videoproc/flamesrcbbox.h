#ifndef FLAMESRCBBOX_H
#define FLAMESRCBBOX_H

#include "firebbox.h"

class FlameSrcBBox: public FireBBox
{
public:
    explicit FlameSrcBBox(bool ip_del = false, bool over_draw = false);
    virtual QSharedPointer<CVKernel::CVNodeData> compute(CVKernel::CVProcessData &process_data);
};

#endif // FLAMESRCBBOX_H
