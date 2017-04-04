#ifndef FLAMESRCBBOX_H
#define FLAMESRCBBOX_H

#include "firebbox.h"


struct FlameSrcBBoxParams : public FireBBoxParams {
    FlameSrcBBoxParams(QJsonObject& json_obj) : FireBBoxParams(json_obj) {}
    virtual ~FlameSrcBBoxParams() {}
};

struct FlameSrcBBoxHistory : public FireBBoxHistory {
    FlameSrcBBoxHistory() {
        grav_thresh = 10.;
    }
    virtual void clear_history() {
        grav_thresh = 10.;
        base_bboxes.clear();
    }
    virtual ~FlameSrcBBoxHistory() {}
};

class FlameSrcBBox: public FireBBox
{
    Q_OBJECT
public:
    explicit FlameSrcBBox() {}
    virtual QSharedPointer<CVKernel::CVNodeData> compute(QSharedPointer<CVKernel::CVProcessData> process_data);
};

#endif // FLAMESRCBBOX_H
