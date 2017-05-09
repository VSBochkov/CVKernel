#include "rfiremaskingmodel.h"
#include <opencv2/imgproc.hpp>

DataRFireMM::DataRFireMM(int rows, int cols) :
    CVKernel::CVNodeData() {
    mask = cv::Mat(rows, cols, cv::DataType<uchar>::type);
    pixel_cnt = 0;
}

DataRFireMM::~DataRFireMM() {}

RFireHistory::RFireHistory()
{
    ema_rgb_extracted = 1.;
}

void RFireHistory::clear_history()
{
    ema_rgb_extracted = 1.;
}

RFireParams::RFireParams(QJsonObject& json_obj) : CVKernel::CVNodeParams(json_obj)
{
    QJsonObject::iterator iter;
    rgb_area_threshold = (iter = json_obj.find("rgb_area_threshold")) == json_obj.end() ? 0.02 : iter.value().toDouble();
    ema_rgb_threshold = (iter = json_obj.find("ema_rgb_threshold")) == json_obj.end() ? 0.2 : iter.value().toDouble();
}

QSharedPointer<CVKernel::CVNodeData> RFireMaskingModel::compute(QSharedPointer<CVKernel::CVProcessData> process_data) {
    cv::Mat frame = CVKernel::video_data[process_data->video_name].frame;
    QSharedPointer<RFireParams> params = process_data->params[metaObject()->className()].staticCast<RFireParams>();
    QSharedPointer<RFireHistory> history = process_data->history[metaObject()->className()].staticCast<RFireHistory>();
    QSharedPointer<DataRFireMM> result(new DataRFireMM(frame.rows, frame.cols));
    uchar* frame_matr = frame.data;
    uchar* res_matr   = result->mask.data;

    ulong rgb_pixel_cnt = 0;
    if (!params->draw_overlay) {
    #pragma omp parallel for
        for (int i = 0; i < frame.rows; ++i) {
            for (int j = 0; j < frame.cols; ++j) {
                uchar b = frame_matr[(i * frame.cols + j) * 3], g = frame_matr[(i * frame.cols + j) * 3 + 1], r = frame_matr[(i * frame.cols + j) * 3 + 2];
                if (r > g && g > b && r > 190 && g > 100 && b < 140)
                {
                    rgb_pixel_cnt++;
                    result->pixel_cnt++;
                    res_matr[i * frame.cols + j] = 1;
                }
                else if (history->ema_rgb_extracted < params->ema_rgb_threshold and (float)(r * 0.299 + g * 0.587 + b * 0.114) > 150.)
                {
                    result->pixel_cnt++;
                    res_matr[i * frame.cols + j] = 1;
                }
                else
                {
                    res_matr[i * frame.cols + j] = 0;
                }
            }
        }
    } else {
        cv::Mat overlay = CVKernel::video_data[process_data->video_name].overlay;
        uchar* over_matr  = overlay.data;

    #pragma omp parallel for
        for (int i = 0; i < frame.rows; ++i) {
            for (int j = 0; j < frame.cols; ++j) {
                uchar b = frame_matr[(i * frame.cols + j) * 3], g = frame_matr[(i * frame.cols + j) * 3 + 1], r = frame_matr[(i * frame.cols + j) * 3 + 2];
                if (r > g && g > b && r > 190 && g > 100 && b < 140)
                {
                    rgb_pixel_cnt++;
                    result->pixel_cnt++;
                    res_matr[i * frame.cols + j] = 1;
                    over_matr[(i * frame.cols + j) * 3 + 1] = 0;
                    over_matr[(i * frame.cols + j) * 3 + 2] = 0xff;
                }
                else if (history->ema_rgb_extracted < params->ema_rgb_threshold and (float)(r * 0.299 + g * 0.587 + b * 0.114) > 150.)
                {
                    result->pixel_cnt++;
                    res_matr[i * frame.cols + j] = 1;
                    over_matr[(i * frame.cols + j) * 3 + 1] = 0;
                    over_matr[(i * frame.cols + j) * 3 + 2] = 0xff;
                }
                else
                {
                    res_matr[i * frame.cols + j] = 0;
                }
            }
        }
    }

    int rgb_enough_criteria = (double)rgb_pixel_cnt / (frame.rows * frame.cols) > params->rgb_area_threshold ? 1 : 0;
    history->ema_rgb_extracted = (process_data->fps * history->ema_rgb_extracted + rgb_enough_criteria) / (process_data->fps + 1);
    return result;
}
