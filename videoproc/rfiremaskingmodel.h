#ifndef RFIREMASKINGMODEL_H
#define RFIREMASKINGMODEL_H

#include <QSharedPointer>
#include "cvgraphnode.h"

class DataRFireMM : public CVKernel::CVNodeData {
public:
    DataRFireMM(int rows, int cols);
    virtual ~DataRFireMM();
    cv::Mat mask;
    ulong pixel_cnt;
};

class RFireMaskingModel : public CVKernel::CVProcessingNode {
    Q_OBJECT
public:
    explicit RFireMaskingModel(bool ip_del = false, bool over_draw = false);
    virtual QSharedPointer<CVKernel::CVNodeData> compute(CVKernel::CVProcessData &process_data);
};

#endif // RFIREMASKINGMODEL_H
