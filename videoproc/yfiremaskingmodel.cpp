#include "yfiremaskingmodel.h"
#include "opencv2/imgproc/imgproc.hpp"


DataYFireMM::DataYFireMM(int rows, int cols) :
    CVKernel::CVNodeData() {
    mask = cv::Mat(rows, cols, cv::DataType<uchar>::type);
}

DataYFireMM::~DataYFireMM() {}

QSharedPointer<CVKernel::CVNodeData> YFireMaskingModel::compute(QSharedPointer<CVKernel::CVProcessData> process_data)
{
    cv::Mat frame = CVKernel::video_data[process_data->video_name].frame;
    QSharedPointer<YFireParams> params = process_data->params[metaObject()->className()].staticCast<YFireParams>();
    QSharedPointer<DataYFireMM> result(new DataYFireMM(frame.rows, frame.cols));
    cv::Mat ycrcb;
    cv::cvtColor(frame, ycrcb, cv::COLOR_BGR2YCrCb);
    uchar* ycrcb_matr = ycrcb.data;
    uchar* res_matr   = result->mask.data;

    if (!params->draw_overlay) {
    #pragma omp parallel for
        for (int i = 0; i < frame.rows; ++i) {
            for (int j = 0; j < frame.cols; ++j) {
                uchar y = ycrcb_matr[(i * frame.cols + j) * 3], cr = ycrcb_matr[(i * frame.cols + j) * 3 + 1], cb = ycrcb_matr[(i * frame.cols + j) * 3 + 2];
                res_matr[i * frame.cols + j] = (y >= cb && cr >= cb) ? 1 : 0;
                result->pixel_cnt += res_matr[i * frame.cols + j];
            }
        }
    } else {
        cv::Mat overlay = CVKernel::video_data[process_data->video_name].overlay;
        uchar* over_matr  = overlay.data;

    #pragma omp parallel for
        for (int i = 0; i < frame.rows; ++i) {
            for (int j = 0; j < frame.cols; ++j) {
                uchar y = ycrcb_matr[(i * frame.cols + j) * 3], cr = ycrcb_matr[(i * frame.cols + j) * 3 + 1], cb = ycrcb_matr[(i * frame.cols + j) * 3 + 2];
                res_matr[i * frame.cols + j] = (y >= cb && cr >= cb) ? 1 : 0;
                if (res_matr[i * frame.cols + j]) {
                   over_matr[(i * frame.cols + j) * 3 + 1] = 0xff;
                   over_matr[(i * frame.cols + j) * 3 + 2] = 0xff;
                   result->pixel_cnt++;
                }
            }
        }
    }

    return result;
}
