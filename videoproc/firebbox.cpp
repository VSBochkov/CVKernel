#include "firebbox.h"
#include "firevalidation.h"
#include "rfiremaskingmodel.h"

#include <omp.h>
#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>

#include <iostream>

FireBBoxData::FireBBoxData(std::vector<obj_bbox> &bboxes) :
    CVKernel::CVNodeData() {
    fire_bboxes = bboxes;
    std::sort(fire_bboxes.begin(), fire_bboxes.end(),
        [](const obj_bbox &a, const obj_bbox &b) -> bool {
            return a.rect.area() < b.rect.area();
        }
    );
}

FireBBoxData::~FireBBoxData() {}

FireBBoxParams::FireBBoxParams(QJsonObject& json_obj) : CVKernel::CVNodeParams(json_obj)
{
    QJsonObject::iterator iter;
    min_area_percent = (iter = json_obj.find("min_area_percent")) == json_obj.end() ? 10. : iter.value().toDouble();
    intersect_thresh = (iter = json_obj.find("intersect_thresh")) == json_obj.end() ? 0.4 : iter.value().toDouble();
    dtime_thresh     = (iter = json_obj.find("dtime_thresh"))     == json_obj.end() ? 0.1 : iter.value().toDouble();
}

std::vector<obj_bbox> FireBBox::calc_bboxes(cv::Mat proc_mask, cv::Mat overlay, ulong pixel_cnt,
                                            cv::Scalar bbox_color, CVKernel::CVProcessData &process_data)
{
    QSharedPointer<FireBBoxParams> params = process_data.params[metaObject()->className()].staticCast<FireBBoxParams>();
    QSharedPointer<FireBBoxHistory> history = process_data.history[metaObject()->className()].staticCast<FireBBoxHistory>();
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(proc_mask.clone(), contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

    std::vector<cv::Moments> mu(contours.size());
    for (uint i = 0; i < contours.size(); i++)
        mu[i] = moments(contours[i], false);

    auto dist2 = [&](double x1, double y1, double x2, double y2) {
        return (x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1);
    };

    for (uint i = 0; i < contours.size(); ++i) {
        for (uint j = 0; j < contours.size(); ++j) {
            if (i == j) continue;
            double x1 = (double) mu[i].m10 / mu[i].m00;
            double y1 = (double) mu[i].m01 / mu[i].m00;
            double x2 = (double) mu[j].m10 / mu[j].m00;
            double y2 = (double) mu[j].m01 / mu[j].m00;
            double grav = (double) (cv::contourArea(contours[i]) * cv::contourArea(contours[j])) / dist2(x1, y1, x2, y2);
            if (grav > history->grav_thresh) {
                cv::line(proc_mask, cv::Point((int) x1, (int) y1), cv::Point((int) x2, (int) y2), 0xff, 2);
            }
        }
    }

    contours.clear();
    cv::findContours(proc_mask, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

    double area_thresh = params->min_area_percent / 100. * pixel_cnt;
    std::vector<cv::Rect> bboxes;
    for (uint i = 0; i < contours.size(); ++i) {
        auto bbox = cv::boundingRect(contours[i]);
        if (bbox.area() > area_thresh)
            bboxes.push_back(bbox);
    }

    std::vector<cv::Rect> merged_bboxes;
    for (uint i = 0; i < bboxes.size(); ++i) {
        cv::Rect merge_rect;
        for (uint j = 0; j < bboxes.size(); ++j) {
            if (i == j) continue;
            cv::Rect intersect = intersection(bboxes[i], bboxes[j]);
            if (std::max((double) intersect.area() / bboxes[i].area(), (double) intersect.area() / bboxes[j].area()) > params->intersect_thresh)
                merge_rect = rect_union(bboxes[i], bboxes[j]);
        }
        merged_bboxes.push_back(merge_rect.area() > 0 ? merge_rect : bboxes[i]);
    }
    bboxes = merged_bboxes;
    if (!history->base_bboxes.empty()) {
        for (auto& bbox : bboxes) {
            double best_rel_koeff = 0.;
            obj_bbox* target_obj = nullptr;
            for (auto& base_bbox : history->base_bboxes) {
                if (intersection(bbox, base_bbox.rect).area() > params->intersect_thresh) {
                    double relation_koeff = bbox.area() > base_bbox.rect.area() ? ((double)base_bbox.rect.area() / (double)bbox.area()) : ((double)bbox.area() / (double)base_bbox.rect.area());
                    if (relation_koeff > best_rel_koeff) {
                        best_rel_koeff = relation_koeff;
                        target_obj = &base_bbox;
                    }
                }
            }
            if (best_rel_koeff > 0. and target_obj != nullptr) {
                target_obj->last_fnum = process_data.frame_num;
                target_obj->rect = bbox;
            } else
                history->base_bboxes.push_back(obj_bbox(bbox, process_data.frame_num));
        }
    } else {
        history->base_bboxes.reserve(bboxes.size());
        for (cv::Rect& bbox : bboxes)
            history->base_bboxes.push_back(obj_bbox(bbox, process_data.frame_num));
    }

    std::vector<obj_bbox> result_bboxes;

    int deleted = 0;
    for (auto& base_bbox : history->base_bboxes) {
        if ((process_data.frame_num - base_bbox.last_fnum) <= params->dtime_thresh * std::max(process_data.fps, 25.)) {
            if (params->draw_overlay)
                cv::rectangle(overlay, base_bbox.rect, bbox_color, 1);

            result_bboxes.push_back(base_bbox);
        } else
            deleted++;
    }

    if (result_bboxes.empty())
        return result_bboxes;

    double aver_bbox_square = 0.;
    for (auto& base_bbox : result_bboxes)
        aver_bbox_square += base_bbox.rect.area();

    aver_bbox_square /= result_bboxes.size();

    if (aver_bbox_square < params->min_area_percent / 10000. * (proc_mask.rows * proc_mask.cols) && history->grav_thresh > 1)
        history->grav_thresh -= 0.5;
    else if (aver_bbox_square > (proc_mask.rows * proc_mask.cols) / 50.)
        history->grav_thresh += 0.5;

    return result_bboxes;
}

QSharedPointer<CVKernel::CVNodeData> FireBBox::compute(QSharedPointer<CVKernel::CVProcessData> process_data) {
    cv::Mat fire_valid = dynamic_cast<FireValidationData *>(process_data->data["FireValidation"].data())->mask;
    ulong pixel_cnt = dynamic_cast<DataRFireMM *>(process_data->data["RFireMaskingModel"].data())->pixel_cnt;
    cv::Mat overlay = CVKernel::video_data[process_data->video_name].overlay;
    std::vector<obj_bbox> bboxes = calc_bboxes(fire_valid.clone(), overlay, pixel_cnt, cv::Scalar(0, 0xff, 0xff), *process_data);
    QSharedPointer<FireBBoxData> result(new FireBBoxData(bboxes));
    return result;
}
