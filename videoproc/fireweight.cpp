#include "fireweight.h"
#include "firebbox.h"
#include "rfiremaskingmodel.h"
#include "firevalidation.h"
#include <omp.h>
#include <opencv2/imgproc.hpp>

#include <iostream>

DataFireWeightDistrib::DataFireWeightDistrib(cv::Mat base, cv::Mat flame_src, ulong flame_pixel_cnt) :
    CVKernel::CVNodeData() {
    weights   = base;
    flame     = flame_src;
    pixel_cnt = flame_pixel_cnt;
}

DataFireWeightDistrib::~DataFireWeightDistrib() {}

FireWeightDistrib::FireWeightDistrib(QObject *parent) :
    CVProcessingNode(parent) {
    counter = 0;
    period = 15;
    weight_thr = 0.3;
    time_thr = 3;
}

QSharedPointer<CVKernel::CVNodeData> FireWeightDistrib::compute(CVKernel::CVProcessData &process_data) {
    std::vector<obj_bbox> fire_bboxes = dynamic_cast<DataFireBBox *>(process_data.data["FireBBox"].data())->fire_bboxes;
    cv::Mat  fire_mask = dynamic_cast<DataRFireMM *>(process_data.data["RFireMaskingModel"].data())->mask;
    cv::Mat  overlay   = CVKernel::video_data[process_data.video_name].overlay;
    cv::Mat  flame = cv::Mat::zeros(overlay.rows, overlay.cols, CV_8UC1);
    uchar* fire_mask_matr = fire_mask.data;
    uchar* overlay_matr = overlay.data;
    uchar* flame_matr = flame.data;

    if (counter == 0) {
        base = cv::Mat::zeros(fire_mask.rows, fire_mask.cols, CV_32FC1);
        timings = cv::Mat::zeros(fire_mask.rows, fire_mask.cols, CV_8U);
    }
    period = process_data.fps > 15. ? (process_data.fps / 2) : period;
    uchar* timings_matr = timings.data;
    float* base_matr = (float*)base.data;

    ulong flame_pixel_cnt = 0;

#pragma omp parallel for
    for (int i = 0; i < fire_mask.rows; ++i) {
        for (int j = 0; j < fire_mask.cols; ++j) {
            int id = i * fire_mask.cols + j;
            obj_bbox* object = getBbox(i, j, fire_bboxes);
            float window = (float)counter / (counter + 1.);
            if (object)
                base_matr[id] = window * base_matr[id] + fire_mask_matr[id] / (counter + 1.);
            else
                base_matr[id] = window * base_matr[id];

            if (base_matr[id] > weight_thr) {
                timings_matr[id] = std::min(timings_matr[id] + 1, 0xff);
                if (timings_matr[id] > period * time_thr) {
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

    counter = std::min(counter + 1, (int)period * 2);
    cv::Mat gray_weights = base * 255;
    gray_weights.assignTo(gray_weights, CV_8UC1);
    cv::Mat rgb_weights = CVKernel::video_data[process_data.video_name].debug_overlay.rowRange(overlay.rows, overlay.rows * 2);
    cv::cvtColor(gray_weights, rgb_weights, CV_GRAY2BGR);

    QSharedPointer<DataFireWeightDistrib> result(new DataFireWeightDistrib(base, flame, flame_pixel_cnt));
    return result;
}
