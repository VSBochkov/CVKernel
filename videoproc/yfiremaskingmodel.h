#ifndef YFIREMASKINGMODEL_H
#define YFIREMASKINGMODEL_H

#include <QSharedPointer>
#include "cvgraphnode.h"

class DataYFireMM : public CVKernel::CVNodeData {
public:
    DataYFireMM(int rows, int cols);
    virtual ~DataYFireMM();
    cv::Mat mask;
};

class YFireMaskingModel : public CVKernel::CVProcessingNode {
    Q_OBJECT
public:
    explicit YFireMaskingModel(QObject *parent = 0);
    virtual QSharedPointer<CVKernel::CVNodeData> compute(CVKernel::CVProcessData &process_data);
};
#endif // YFIREMASKINGMODEL_H
