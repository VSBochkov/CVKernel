#include "yfiremaskingmodel.h"
#include "opencv2/imgproc/imgproc.hpp"


DataYFireMM::DataYFireMM(int rows, int cols) :
    CVKernel::CVNodeData() {
    mask = cv::Mat(rows, cols, cv::DataType<uchar>::type);
}

DataYFireMM::~DataYFireMM() {}

YFireMaskingModel::YFireMaskingModel(bool ip_del, bool over_draw) :
    CVProcessingNode(ip_del, over_draw) {}

QSharedPointer<CVKernel::CVNodeData> YFireMaskingModel::compute(CVKernel::CVProcessData &process_data) {
    cv::Mat frame = CVKernel::video_data[process_data.video_name].frame;
    cv::Mat overlay = CVKernel::video_data[process_data.video_name].overlay;
    cv::Mat ycrcb;
    cv::cvtColor(frame, ycrcb, cv::COLOR_BGR2YCrCb);
    QSharedPointer<DataYFireMM> result(new DataYFireMM(overlay.rows, overlay.cols));

    for (int i = 0; i < ycrcb.rows; ++i) {
        for (int j = 0; j < ycrcb.cols; ++j) {
            cv::Point3_<uchar> pt = ycrcb.at<cv::Point3_<uchar>>(i, j);
            uchar y = pt.x, cr = pt.y, cb = pt.z;
            result->mask.at<uchar>(i,j) = (y >= cb && cr >= cb) ? 0xff : 0;
            if (result->mask.at<uchar>(i, j) == 0xff) {
                cv::Point3_<uchar> rgb = frame.at<cv::Point3_<uchar>>(i, j);
                uchar b = rgb.x, r = rgb.z;
                overlay.at<cv::Point3_<uchar>>(i, j) = cv::Point3_<uchar>(b, 0xff, r);
            }
        }
    }

    return result;
}
