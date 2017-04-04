#ifndef FIRE_BBOX_H
#define FIRE_BBOX_H

#include <QJsonArray>
#include <QJsonObject>
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

    QJsonObject pack_to_json() {
        QJsonObject bbox;
        bbox["x"] = rect.x;
        bbox["y"] = rect.y;
        bbox["w"] = rect.width;
        bbox["h"] = rect.height;
        return bbox;
    }
};

struct FireBBoxData : public CVKernel::CVNodeData {
    FireBBoxData(std::vector<obj_bbox>& bboxes);
    virtual ~FireBBoxData();
    virtual QJsonObject pack_to_json() {
        QJsonArray bboxes_json;
        for (auto bbox : fire_bboxes) {
            QJsonObject bbox_obj = bbox.pack_to_json();
            bboxes_json.append(bbox_obj);
        }
        QJsonObject fire_bbox_json;
        if (not bboxes_json.empty()) {
            fire_bbox_json["bboxes"] = bboxes_json;
        }
        return fire_bbox_json;
    }
    std::vector<obj_bbox> fire_bboxes;
};

struct FireBBoxParams : public CVKernel::CVNodeParams {
    FireBBoxParams(QJsonObject& json_obj);
    virtual ~FireBBoxParams() {}
    double min_area_percent;
    double intersect_thresh;
    double dtime_thresh;
};

struct FireBBoxHistory : public CVKernel::CVNodeHistory {
    FireBBoxHistory() {
        grav_thresh = 5.;
    }
    virtual void clear_history() {
        grav_thresh = 5.;
        base_bboxes.clear();
    }
    virtual ~FireBBoxHistory() {}
    double grav_thresh;
    std::vector<obj_bbox> base_bboxes;
};

class FireBBox : public CVKernel::CVProcessingNode {
    Q_OBJECT
public:
    explicit FireBBox() {}
    virtual QSharedPointer<CVKernel::CVNodeData> compute(QSharedPointer<CVKernel::CVProcessData> process_data);

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
};

#endif // FIRE_BBOX_H
