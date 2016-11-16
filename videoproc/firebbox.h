#ifndef FIRE_BBOX_H
#define FIRE_BBOX_H

#include <QSharedPointer>
#include "cvgraphnode.h"
#include <vector>

struct obj_bbox {
    cv::Rect rect;
    int last_fnum;
    int first_fnum;

    obj_bbox(cv::Rect bbox, int fnum):
        rect(bbox), last_fnum(fnum), first_fnum(fnum) {}
    obj_bbox(): rect(cv::Rect()), last_fnum(-1), first_fnum(-1) {}
};

class DataFireBBox : public CVKernel::CVNodeData {
public:
    DataFireBBox(std::vector<obj_bbox>& bboxes);
    virtual ~DataFireBBox();
    std::vector<obj_bbox> fire_bboxes;
};

class FireBBox : public CVKernel::CVProcessingNode {
    Q_OBJECT
public:
    explicit FireBBox(QObject *parent = 0);
    virtual QSharedPointer<CVKernel::CVNodeData> compute(CVKernel::CVProcessData &process_data);

protected:
    std::vector<obj_bbox> calc_bboxes(cv::Mat proc_mask, cv::Mat overlay, ulong pixel_cnt, cv::Scalar bbox_color, bool draw, CVKernel::CVProcessData &process_data);

    cv::Rect intersection(cv::Rect rect1, cv::Rect rect2) {
        int x1 = std::max(rect1.x, rect2.x);
        int y1 = std::max(rect1.y, rect2.y);
        int x2 = std::min(rect1.x + rect1.width, rect2.x + rect2.width);
        int y2 = std::min(rect1.y + rect2.height, rect2.y + rect2.height);
        if (x1 >= x2 || y1 >= y2)
            return cv::Rect();
        else
            return cv::Rect(x1, y1, x2 - x1, y2 - y1);
    }
    cv::Rect rect_union(cv::Rect rect1, cv::Rect rect2) {
        int x1 = std::min(rect1.x, rect2.x);
        int y1 = std::min(rect1.y, rect2.y);
        int x2 = std::max(rect1.x + rect1.width, rect2.x + rect2.width);
        int y2 = std::max(rect1.y + rect2.height, rect2.y + rect2.height);
        return cv::Rect(x1, y1, x2 - x1, y2 - y1);
    }


protected:
    double grav_thresh;
    double min_area_percent;
    double intersect_thresh;
    double dtime_thresh;
    double aver_bbox_square;
    int debug_overlay_index;
    std::vector<obj_bbox> base_bboxes;
};

#endif // FIRE_BBOX_H
