#include "firevalidation.h"
#include "rfiremaskingmodel.h"

#include <QSharedPointer>

#include <iostream>
#include <omp.h>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

DataFireValidation::DataFireValidation(int rows, int cols) :
    CVKernel::CVNodeData() {
    mask = cv::Mat(rows, cols, cv::DataType<uchar>::type);
}

DataFireValidation::~DataFireValidation() {}

FireValidation::FireValidation(bool ip_del, bool over_draw) :
    CVProcessingNode(ip_del, over_draw) {
    alpha1 = 0.25;
    alpha2 = 0.75;
    dma_thresh = 12.;
}

template<class Type1, class Type2> double FireValidation::dist(cv::Point3_<Type1> p1, cv::Point3_<Type2> p2) {
    return sqrt(pow((float)p1.x - (float)p2.x, 2.) + pow((float)p1.y - (float)p2.y, 2.) + pow((float)p1.z - (float)p2.z, 2.));
}


QSharedPointer<CVKernel::CVNodeData> FireValidation::compute(CVKernel::CVProcessData &process_data) {
    cv::Mat frame = CVKernel::video_data[process_data.video_name].frame;
    cv::Mat rgbSignal = dynamic_cast<DataRFireMM *>(process_data.data["RFireMaskingModel"].data())->mask;
    QSharedPointer<DataFireValidation> result(new DataFireValidation(frame.rows, frame.cols));

    if (process_data.frame_num == 1) {
        ema = frame.clone();
        ema.convertTo(ema, CV_32FC3);
        dma = cv::Mat(frame.rows, frame.cols, CV_32FC1);
        return result;
    }

    uchar* frame_matr       = frame.data;
    uchar* rgbSignal_matr   = rgbSignal.data;
    float* ema_matr         = (float*)ema.data;
    float* dma_matr         = (float*)dma.data;
    uchar* res_mask         = result->mask.data;

    if (!draw_overlay) {
    #pragma omp parallel for
        for (int i = 0; i < frame.rows; ++i) {
            for (int j = 0; j < frame.cols; ++j) {
                int id = i * frame.cols + j;
                if (rgbSignal_matr[id]) {
                    for (int k = 0; k < 3; ++k)
                       ema_matr[id * 3 + k] = ema_matr[id * 3 + k] * (1. - alpha1) + (float) frame_matr[id * 3 + k] * alpha1;
                    dma_matr[id] = (1. - alpha2) * dma_matr[id] + sqrt(
                        pow((float) frame_matr[id * 3]      - ema_matr[id * 3],     2.) +
                        pow((float) frame_matr[id * 3 + 1]  - ema_matr[id * 3 + 1], 2.) +
                        pow((float) frame_matr[id * 3 + 2]  - ema_matr[id * 3 + 2], 2.)
                    ) * alpha2;
                } else
                    dma_matr[id] = (1. - alpha2) * dma_matr[id] + alpha2 * 10;
                res_mask[id] = dma_matr[id] >= dma_thresh ? 1 : 0;
            }
        }
    } else {
        cv::Mat overlay = CVKernel::video_data[process_data.video_name].overlay;
        uchar* overlay_matr     = overlay.data;
    #pragma omp parallel for
        for (int i = 0; i < frame.rows; ++i) {
            for (int j = 0; j < frame.cols; ++j) {
                int id = i * frame.cols + j;
                if (rgbSignal_matr[id]) {
                    for (int k = 0; k < 3; ++k)
                       ema_matr[id * 3 + k] = ema_matr[id * 3 + k] * (1. - alpha1) + (float) frame_matr[id * 3 + k] * alpha1;
                    dma_matr[id] = (1. - alpha2) * dma_matr[id] + sqrt(
                        pow((float) frame_matr[id * 3]      - ema_matr[id * 3],     2.) +
                        pow((float) frame_matr[id * 3 + 1]  - ema_matr[id * 3 + 1], 2.) +
                        pow((float) frame_matr[id * 3 + 2]  - ema_matr[id * 3 + 2], 2.)
                    ) * alpha2;
                } else
                    dma_matr[id] = (1. - alpha2) * dma_matr[id] + alpha2 * 10;

                if (dma_matr[id] >= dma_thresh) {
                    overlay_matr[id * 3] = 0xff;
                    overlay_matr[id * 3 + 1] = frame_matr[id * 3 + 1];
                    overlay_matr[id * 3 + 2] = frame_matr[id * 3 + 2];
                    res_mask[id] = 1;
                } else {
                    res_mask[id] = 0;
                }
            }
        }
    }
//    cv::Mat rgb_fvalid = CVKernel::video_data[process_data.video_name].debug_overlay.rowRange(overlay.rows * 2, overlay.rows * 3);
//    cv::cvtColor(result->mask * 255, rgb_fvalid, CV_GRAY2BGR);
    return result;
}
