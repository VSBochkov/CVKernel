#ifndef FIREVALIDATION_H
#define FIREVALIDATION_H

#include <QSharedPointer>
#include "cvgraphnode.h"

class DataFireValidation : public CVKernel::CVNodeData {
public:
    DataFireValidation(int rows, int cols);
    virtual ~DataFireValidation();
    cv::Mat mask;
};

class FireValidation : public CVKernel::CVProcessingNode {
    Q_OBJECT
public:
    explicit FireValidation(QObject *parent = 0, bool ip_del = false, bool over_draw = false);
    virtual QSharedPointer<CVKernel::CVNodeData> compute(CVKernel::CVProcessData &process_data);

private:
    template<class Type1, class Type2> double dist(cv::Point3_<Type1> p1, cv::Point3_<Type2> p2);

private:
    cv::Mat ema;
    cv::Mat dma;
    double alpha1;
    double alpha2;
    double dma_thresh;
};

#endif // FIREVALIDATION_H
