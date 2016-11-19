#include "rfiremaskingmodel.h"
#include <opencv2/imgproc.hpp>

DataRFireMM::DataRFireMM(int rows, int cols) :
    CVKernel::CVNodeData() {
    mask = cv::Mat(rows, cols, cv::DataType<uchar>::type);
    pixel_cnt = 0;
}

DataRFireMM::~DataRFireMM() {}

RFireMaskingModel::RFireMaskingModel(QObject *parent, bool ip_del, bool over_draw) :
    CVProcessingNode(parent, ip_del, over_draw) {}

QSharedPointer<CVKernel::CVNodeData> RFireMaskingModel::compute(CVKernel::CVProcessData &process_data) {
    cv::Mat frame = CVKernel::video_data[process_data.video_name].frame;
    QSharedPointer<DataRFireMM> result(new DataRFireMM(frame.rows, frame.cols));
    uchar* frame_matr = frame.data;
    uchar* res_matr   = result->mask.data;

    if (!draw_overlay) {
    #pragma omp parallel for
        for (int i = 0; i < frame.rows; ++i) {
            for (int j = 0; j < frame.cols; ++j) {
                uchar b = frame_matr[(i * frame.cols + j) * 3], g = frame_matr[(i * frame.cols + j) * 3 + 1], r = frame_matr[(i * frame.cols + j) * 3 + 2];
                res_matr[i * frame.cols + j] = (r > g && g > b && r > 190 && g > 100 && b < 140) ? 1 : 0;
                result->pixel_cnt += res_matr[i * frame.cols + j];
            }
        }
    } else {
        cv::Mat overlay = CVKernel::video_data[process_data.video_name].overlay;
        uchar* over_matr  = overlay.data;

    #pragma omp parallel for
        for (int i = 0; i < frame.rows; ++i) {
            for (int j = 0; j < frame.cols; ++j) {
                uchar b = frame_matr[(i * frame.cols + j) * 3], g = frame_matr[(i * frame.cols + j) * 3 + 1], r = frame_matr[(i * frame.cols + j) * 3 + 2];
                res_matr[i * frame.cols + j] = (r > g && g > b && r > 190 && g > 100 && b < 140) ? 1 : 0;
                if (res_matr[i * frame.cols + j]) {
                   over_matr[(i * frame.cols + j) * 3 + 1] = 0xff;
                   over_matr[(i * frame.cols + j) * 3 + 2] = 0xff;
                   result->pixel_cnt++;
                }
            }
        }
    }

//    cv::Mat rgb_rmodel = CVKernel::video_data[process_data.video_name].debug_overlay.rowRange(overlay.rows, overlay.rows * 2);
//    cv::cvtColor(result->mask * 255, rgb_rmodel, CV_GRAY2BGR);

    return result;
}
