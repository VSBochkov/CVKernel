#include "fireweight.h"
#include "firebbox.h"
#include "rfiremaskingmodel.h"
#include "firevalidation.h"
#include <omp.h>
#include <opencv2/imgproc.hpp>

#include <iostream>

FireWeightDistribData::FireWeightDistribData(cv::Mat base, cv::Mat flame_src, ulong flame_pixel_cnt) :
    CVKernel::CVNodeData()
{
    weights   = base;
    flame     = flame_src;
    pixel_cnt = flame_pixel_cnt;
}

FireWeightDistribParams::FireWeightDistribParams(QJsonObject &json_obj) : CVNodeParams(json_obj)
{
    QJsonObject::iterator iter;
    weight_thr = (iter = json_obj.find("weight_thr")) == json_obj.end() ? 0.3 : iter.value().toDouble();
    time_thr   = (iter = json_obj.find("time_thr")) == json_obj.end()   ? 1.5 : iter.value().toDouble();
}

FireWeightDistribHistory::FireWeightDistribHistory() {}

void FireWeightDistribHistory::clear_history()
{
    if (!base.empty())
        base.release();
    if (!timings.empty())
        timings.release();
}

QSharedPointer<CVKernel::CVNodeData> FireWeightDistrib::compute(QSharedPointer<CVKernel::CVProcessData> process_data)
{
    std::vector<obj_bbox> fire_bboxes = dynamic_cast<FireBBoxData *>(process_data->data["FireBBox"].data())->fire_bboxes;
    cv::Mat  fire_mask = dynamic_cast<DataRFireMM *>(process_data->data["RFireMaskingModel"].data())->mask;
    cv::Mat  flame = cv::Mat::zeros(fire_mask.rows, fire_mask.cols, CV_8UC1);
    QSharedPointer<FireWeightDistribParams> params = process_data->params[metaObject()->className()].staticCast<FireWeightDistribParams>();
    QSharedPointer<FireWeightDistribHistory> history = process_data->history[metaObject()->className()].staticCast<FireWeightDistribHistory>();

    if (history->base.empty()) {
        history->base = cv::Mat::zeros(fire_mask.rows, fire_mask.cols, CV_32FC1);
    }
    if (history->timings.empty()) {
        history->timings = cv::Mat::zeros(fire_mask.rows, fire_mask.cols, CV_8U);
    }

    uchar* fire_mask_matr = fire_mask.data;
    uchar* flame_matr = flame.data;
    double frames_in_second = process_data->fps > 15. ? process_data->fps : 15.;
    float window = (float) (frames_in_second) / (frames_in_second + 1.);
    uchar* timings_matr = history->timings.data;
    float* base_matr = (float*) history->base.data;
    ulong flame_pixel_cnt = 0;


    if (!params->draw_overlay) {
    #pragma omp parallel for
        for (int i = 0; i < fire_mask.rows; ++i) {
            for (int j = 0; j < fire_mask.cols; ++j) {
                int id = i * fire_mask.cols + j;
                base_matr[id] = window * base_matr[id] + (getBbox(i, j, fire_bboxes) ? fire_mask_matr[id] / (frames_in_second + 1.) : 0.);
                if (base_matr[id] > params->weight_thr) {
                    timings_matr[id] = std::min(timings_matr[id] + 1, 0xff);
                    if (timings_matr[id] > frames_in_second * params->time_thr) {
                        flame_matr[id] = 1;
                        flame_pixel_cnt++;
                    }
                } else
                    timings_matr[id] = 0;
            }
        }
    } else {
        cv::Mat  overlay   = CVKernel::video_data[process_data->video_name].overlay;
        uchar* overlay_matr = overlay.data;
    #pragma omp parallel for
        for (int i = 0; i < fire_mask.rows; ++i) {
            for (int j = 0; j < fire_mask.cols; ++j) {
                int id = i * fire_mask.cols + j;
                base_matr[id] = window * base_matr[id] + (getBbox(i, j, fire_bboxes) ? fire_mask_matr[id] / (frames_in_second + 1.) : 0.);
                if (base_matr[id] > params->weight_thr) {
                    timings_matr[id] = std::min(timings_matr[id] + 1, 0xff);
                    if (timings_matr[id] > frames_in_second * params->time_thr) {
                        overlay_matr[id * 3]     = 0xff;
                        overlay_matr[id * 3 + 1] = 0;
                        flame_matr[id] = 1;
                        flame_pixel_cnt++;
                    } else {
                        overlay_matr[id * 3]     = 0;
                        overlay_matr[id * 3 + 1] = 0xff;
                    }
                    overlay_matr[id * 3 + 2] = 0;
                } else
                    timings_matr[id] = 0;
            }
        }
    }

    QSharedPointer<FireWeightDistribData> result(new FireWeightDistribData(history->base, flame, flame_pixel_cnt));
    return result;
}
