#ifndef FIREMASKLOGICAND_H
#define FIREMASKLOGICAND_H

#include <QSharedPointer>
#include "cvgraphnode.h"
#include "videoproc/firebbox.h"

class DataFireWeightDistrib : public CVKernel::CVNodeData {
public:
    DataFireWeightDistrib(cv::Mat base, cv::Mat flame_src, ulong flame_pixel_cnt);
    virtual ~DataFireWeightDistrib();
    cv::Mat weights;
    cv::Mat flame;
    ulong pixel_cnt;
};

class FireWeightDistrib : public CVKernel::CVProcessingNode
{
    Q_OBJECT
public:
    explicit FireWeightDistrib(bool ip_del = false, bool over_draw = false);
    virtual QSharedPointer<CVKernel::CVNodeData> compute(CVKernel::CVProcessData &process_data);

private:
    bool ptInRect(int i, int j, cv::Rect& bbox) {
        return (i >= bbox.y && j >= bbox.x &&
                i <= bbox.y + bbox.height &&
                j <= bbox.x + bbox.width);
    }

    obj_bbox* getBbox(int i, int j, std::vector<obj_bbox> bboxes) {
        static size_t id = 0;
        for (size_t cnt = 0; cnt < bboxes.size(); ++cnt)
            if (ptInRect(i, j, bboxes[(id + cnt) % bboxes.size()].rect)) return &bboxes[(id + cnt) % bboxes.size()];
        return NULL;
    }

private:
    cv::Mat base;
    cv::Mat timings;
    int counter;
    float period;
    float weight_thr;
    float time_thr;
};

#endif // FIREMASKLOGICAND_H
