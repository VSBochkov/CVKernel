#ifndef YFIREMASKINGMODEL_H
#define YFIREMASKINGMODEL_H

#include <QSharedPointer>
#include "cvgraphnode.h"

class DataYFireMM : public CVKernel::CVNodeData {
public:
    DataYFireMM(int rows, int cols);
    virtual ~DataYFireMM();
    cv::Mat mask;
    ulong pixel_cnt;
};

struct YFireParams : public CVKernel::CVNodeParams {
    YFireParams(QJsonObject& json_obj) : CVNodeParams(json_obj) {}
    virtual ~YFireParams() {}
};

struct YFireHistory : public CVKernel::CVNodeHistory {
    YFireHistory() {}
    virtual void clear_history() {}
    virtual ~YFireHistory() {}
};

class YFireMaskingModel : public CVKernel::CVProcessingNode {
    Q_OBJECT
public:
    explicit YFireMaskingModel() {}
    virtual QSharedPointer<CVKernel::CVNodeData> compute(QSharedPointer<CVKernel::CVProcessData> process_data);
};
#endif // YFIREMASKINGMODEL_H   // Выход из области подключения
