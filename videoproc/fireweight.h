#ifndef FIREMASKLOGICAND_H
#define FIREMASKLOGICAND_H

#include <QSharedPointer>
#include "cvgraphnode.h"
#include "videoproc/firebbox.h"

class FireWeightDistribData : public CVKernel::CVNodeData {
public:
    FireWeightDistribData(cv::Mat base, cv::Mat flame_src, ulong flame_pixel_cnt);
    virtual ~FireWeightDistribData() {}
    cv::Mat weights;
    cv::Mat flame;
    ulong pixel_cnt;
};

struct FireWeightDistribParams : public CVKernel::CVNodeParams {
    FireWeightDistribParams(QJsonObject& json_obj);
    virtual ~FireWeightDistribParams() {}
    float weight_thr;
    float time_thr;
};

struct FireWeightDistribHistory : public CVKernel::CVNodeHistory {
    FireWeightDistribHistory();
    virtual void clear_history();
    virtual ~FireWeightDistribHistory() {}
    cv::Mat base;
    cv::Mat timings;
};

class FireWeightDistrib : public CVKernel::CVProcessingNode
{
    Q_OBJECT
public:
    explicit FireWeightDistrib() {}
    virtual QSharedPointer<CVKernel::CVNodeData> compute(QSharedPointer<CVKernel::CVProcessData> process_data);

private:
    bool ptInRect(int i, int j, const cv::Rect& bbox) {
        return (i >= bbox.y && j >= bbox.x &&
                i <= bbox.y + bbox.height &&
                j <= bbox.x + bbox.width);
    }

    obj_bbox* getBbox(int i, int j, std::vector<obj_bbox> bboxes) {
        static size_t id = 0;
        for (size_t cnt = 0; cnt < bboxes.size(); ++cnt) {
            if (ptInRect(i, j, bboxes[(id + cnt) % bboxes.size()].rect)) {
                return &bboxes[(id + cnt) % bboxes.size()];
            }
        }
        return NULL;
    }
};

#endif // FIREMASKLOGICAND_H
