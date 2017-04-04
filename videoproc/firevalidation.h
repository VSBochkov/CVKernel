#ifndef FIREVALIDATION_H
#define FIREVALIDATION_H

#include <QSharedPointer>
#include "cvgraphnode.h"


struct FireValidationData : public CVKernel::CVNodeData {
    FireValidationData(int rows, int cols);
    virtual ~FireValidationData();
    cv::Mat mask;
};

struct FireValidationParams : public CVKernel::CVNodeParams {
    FireValidationParams(QJsonObject& json_obj);
    virtual ~FireValidationParams() {}
    double alpha1;
    double alpha2;
    double dma_thresh;
};

struct FireValidationHistory : public CVKernel::CVNodeHistory {
    FireValidationHistory() {
        deffered_init = true;
    }
    virtual void clear_history() {
        deffered_init = true;
    }
    virtual ~FireValidationHistory() {}
    cv::Mat ema;
    cv::Mat dma;
    bool deffered_init;
};

class FireValidation : public CVKernel::CVProcessingNode {
    Q_OBJECT
public:
    explicit FireValidation() {}
    virtual QSharedPointer<CVKernel::CVNodeData> compute(QSharedPointer<CVKernel::CVProcessData> process_data);

private:
    template<class Type1, class Type2> double dist(cv::Point3_<Type1> p1, cv::Point3_<Type2> p2);
};

#endif // FIREVALIDATION_H
