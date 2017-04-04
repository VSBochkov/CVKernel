#include "flamesrcbbox.h"
#include "fireweight.h"

#include <QSharedPointer>
#include <vector>

#include <omp.h>
#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>


QSharedPointer<CVKernel::CVNodeData> FlameSrcBBox::compute(QSharedPointer<CVKernel::CVProcessData> process_data) {
    ulong pixel_cnt = dynamic_cast<FireWeightDistribData *>(process_data->data["FireWeightDistrib"].data())->pixel_cnt;
    cv::Mat flame = dynamic_cast<FireWeightDistribData *>(process_data->data["FireWeightDistrib"].data())->flame;
    cv::Mat overlay = CVKernel::video_data[process_data->video_name].overlay;

    std::vector<obj_bbox> bboxes = calc_bboxes(flame.clone(), overlay, pixel_cnt, cv::Scalar(0xff, 0, 0), *process_data);
    QSharedPointer<FireBBoxData> result(new FireBBoxData(bboxes));
    return result;
}
