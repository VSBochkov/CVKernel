#include "flamesrcbbox.h"
#include "fireweight.h"

#include <QSharedPointer>
#include <vector>

#include <omp.h>
#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>


FlameSrcBBox::FlameSrcBBox(QObject *parent) :
    FireBBox(parent) {
    grav_thresh = 10.;
    min_area_percent = 10;
    intersect_thresh = 0.4;
    dtime_thresh = 0.5;
    debug_overlay_index = 3;
}

QSharedPointer<CVKernel::CVNodeData> FlameSrcBBox::compute(CVKernel::CVProcessData &process_data) {
    ulong pixel_cnt = dynamic_cast<DataFireWeightDistrib *>(process_data.data["FireWeightDistrib"].data())->pixel_cnt;
    cv::Mat flame = dynamic_cast<DataFireWeightDistrib *>(process_data.data["FireWeightDistrib"].data())->flame;
    cv::Mat overlay = CVKernel::video_data[process_data.video_name].overlay;

    std::vector<obj_bbox> bboxes = calc_bboxes(flame.clone(), overlay, pixel_cnt, cv::Scalar(0xff, 0, 0), true, process_data);
    QSharedPointer<DataFireBBox> result(new DataFireBBox(bboxes));
    return result;
}
