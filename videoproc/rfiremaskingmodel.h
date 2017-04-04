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

struct RFireParams : public CVKernel::CVNodeParams {
    RFireParams(QJsonObject& json_obj) : CVNodeParams(json_obj) {}
    virtual ~RFireParams() {}
};

struct RFireHistory : public CVKernel::CVNodeHistory {
    RFireHistory() {}
    virtual void clear_history() {}
    virtual ~RFireHistory() {}
};

class RFireMaskingModel : public CVKernel::CVProcessingNode {
    Q_OBJECT
public:
    explicit RFireMaskingModel() {}
    virtual QSharedPointer<CVKernel::CVNodeData> compute(QSharedPointer<CVKernel::CVProcessData> process_data);
};

#endif // RFIREMASKINGMODEL_H
