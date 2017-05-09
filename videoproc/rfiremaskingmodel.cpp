#include "rfiremaskingmodel.h"
#include <opencv2/imgproc.hpp>

DataRFireMM::DataRFireMM(int rows, int cols) :
    CVKernel::CVNodeData() {
    mask = cv::Mat(rows, cols, cv::DataType<uchar>::type);
    pixel_cnt = 0;
}

DataRFireMM::~DataRFireMM() {}

RFireParams::RFireParams(QJsonObject& json_obj) : CVKernel::CVNodeParams(json_obj)
{
    QJsonObject::iterator iter;
    brightness_threshold = (iter = json_obj.find("brightness_threshold")) == json_obj.end() ? 150 : iter.value().toInt();
}

QSharedPointer<CVKernel::CVNodeData> RFireMaskingModel::compute(QSharedPointer<CVKernel::CVProcessData> process_data) {
    cv::Mat frame = CVKernel::video_data[process_data->video_name].frame;
    QSharedPointer<RFireParams> params = process_data->params[metaObject()->className()].staticCast<RFireParams>();
    QSharedPointer<DataRFireMM> result(new DataRFireMM(frame.rows, frame.cols));
    uchar* frame_matr = frame.data;
    uchar* res_matr   = result->mask.data;


    cv::Mat left_top(frame.rowRange(0, 5).colRange(0, 5));
    auto left_top_sum = cv::sum(left_top);
    cv::Mat right_top(frame.rowRange(0, 5).colRange(frame.cols - 5, frame.cols));
    auto right_top_sum = cv::sum(left_top);

    if (left_top_sum[0] + left_top_sum[1] + left_top_sum[2] < 150 * left_top.rows * left_top.cols or
            right_top_sum[0] + right_top_sum[1] + right_top_sum[2] < 150 * right_top.rows * right_top.cols)
    {
        /*Video capture can be blinded or there is night, work into GRAYSCALE*/
        cv::Mat gray;
        cv::cvtColor(frame, gray, CV_BGR2GRAY);
        result->mask = gray > 180;
        cv::threshold(result->mask, result->mask, 180., 1., cv::THRESH_BINARY);
        result->pixel_cnt = (ulong) cv::sum(result->mask)[0];
    }
    else
    {
        if (!params->draw_overlay) {
        #pragma omp parallel for
            for (int i = 0; i < frame.rows; ++i) {
                for (int j = 0; j < frame.cols; ++j) {
                    uchar b = frame_matr[(i * frame.cols + j) * 3], g = frame_matr[(i * frame.cols + j) * 3 + 1], r = frame_matr[(i * frame.cols + j) * 3 + 2];
                    res_matr[i * frame.cols + j] = (r > g && g > b && r > 190 && g > 100 && b < 140) ? 1 : 0;
                    result->pixel_cnt += res_matr[i * frame.cols + j];
                }
            }
        } else {
            cv::Mat overlay = CVKernel::video_data[process_data->video_name].overlay;
            uchar* over_matr  = overlay.data;

        #pragma omp parallel for
            for (int i = 0; i < frame.rows; ++i) {
                for (int j = 0; j < frame.cols; ++j) {
                    uchar b = frame_matr[(i * frame.cols + j) * 3], g = frame_matr[(i * frame.cols + j) * 3 + 1], r = frame_matr[(i * frame.cols + j) * 3 + 2];
                    res_matr[i * frame.cols + j] = (r > g && g > b && r > 190 && g > 100 && b < 140) ? 1 : 0;
                    if (res_matr[i * frame.cols + j]) {
                       over_matr[(i * frame.cols + j) * 3 + 1] = 0;
                       over_matr[(i * frame.cols + j) * 3 + 2] = 0xff;
                       result->pixel_cnt++;
                    }
                }
            }
        }
    }

    return result;
}
