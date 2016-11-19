#ifndef FIRE_BBOX_H
#define FIRE_BBOX_H

#include <QSharedPointer>
#include <QDataStream>
#include "cvgraphnode.h"
#include <vector>

struct obj_bbox {
    cv::Rect rect;
    long last_fnum;
    long first_fnum;

    obj_bbox(cv::Rect bbox, long fnum):
        rect(bbox), last_fnum(fnum), first_fnum(fnum) {}
    obj_bbox(): rect(cv::Rect()), last_fnum(-1), first_fnum(-1) {}

    friend QDataStream& operator << (QDataStream& out, const obj_bbox& bbox) {
        out << qint16(bbox.rect.x) << qint16(bbox.rect.y) << qint16(bbox.rect.width) << qint16(bbox.rect.height)
            << qint64(bbox.last_fnum) << qint64(bbox.first_fnum);
        return out;
    }
    friend QDataStream& operator >> (QDataStream& in, obj_bbox& bbox) {
        qint16 rect_x, rect_y, rect_width, rect_height;
        qint64 last, first;
        in >> rect_x >> rect_y >> rect_width >> rect_height >> last >> first;
        bbox.rect.x = (int)rect_x;
        bbox.rect.y = (int)rect_y;
        bbox.rect.width = (int)rect_width;
        bbox.rect.height = (int)rect_height;
        bbox.last_fnum = (long)last;
        bbox.first_fnum = (long)first;
        return in;
    }
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
    explicit FireBBox(QObject *parent = 0, bool ip_del = false, bool over_draw = false);
    virtual QSharedPointer<CVKernel::CVNodeData> compute(CVKernel::CVProcessData &process_data);

protected:
    std::vector<obj_bbox> calc_bboxes(cv::Mat proc_mask, cv::Mat overlay, ulong pixel_cnt, cv::Scalar bbox_color, CVKernel::CVProcessData &process_data);

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
