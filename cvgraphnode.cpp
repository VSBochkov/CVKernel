#include "cvgraphnode.h"

#include <QFile>
#include <QTextStream>
#include <QTimer>
#include <opencv2/opencv.hpp>
#include <iostream>
#include <unistd.h>
#include <time.h>

using namespace CVKernel;
QMap<QString, VideoData> CVKernel::video_data;

CVProcessData::CVProcessData(QString video_name, cv::Mat frame, int fnum, double fps, bool draw_overlay):
    video_name(video_name) {
    video_data[video_name].frame = frame;
    if (draw_overlay)
    {
        frame.copyTo(video_data[video_name].overlay);
    }
    frame_num = fnum;
    fps = fps;
    data_serialized.clear();
}

CVIONode::CVIONode(QObject *parent, int device_id, bool draw_overlay, QString ip_addr, int ip_p) :
    QObject(parent) {
    frame_number = 1;
    if (draw_overlay)
        overlay_name = QString("device_") + QString::number(device_id);
    if (!ip_addr.isEmpty()) {
        udp_socket = new QUdpSocket(this);
        udp_addr = new QHostAddress(ip_addr);
        udp_port = ip_p;
    } else {
        udp_socket = nullptr;
        udp_addr = nullptr;
    }
    if (!in_stream.isOpened()) {
        in_stream.open(device_id);
        fps = in_stream.get(CV_CAP_PROP_FPS) > 1.0 ? in_stream.get(CV_CAP_PROP_FPS) : 30.;
    }
}

CVIONode::CVIONode(QObject *parent, QString video_name, bool draw_overlay, QString ip_addr, int ip_p, QString over_name) :
    QObject(parent), video_name(video_name) {
    frame_number = 1;
    if (draw_overlay)
        overlay_name = over_name;
    if (!ip_addr.isEmpty()) {
        udp_socket = new QUdpSocket(this);
        udp_addr = new QHostAddress(ip_addr);
        udp_port = ip_p;
    } else {
        udp_socket = nullptr;
        udp_addr = nullptr;
    }
    if (!in_stream.isOpened()) {
        in_stream.open(video_name.toStdString());
        fps = in_stream.get(CV_CAP_PROP_FPS) > 1.0 ? in_stream.get(CV_CAP_PROP_FPS) : 30.;
    }
}

void CVIONode::process() {
    cv::Mat frame, scale_frame;
    bool draw_overlay = !overlay_name.isEmpty();
    if (draw_overlay)
        cv::namedWindow(overlay_name.toStdString(), CV_WINDOW_AUTOSIZE);
    clock_t t1 = clock();
    while(in_stream.read(frame)) {
        cv::resize(frame, scale_frame, cv::Size(), 400. / frame.cols, 400. / frame.cols);
        process_data = CVProcessData(video_name, scale_frame, frame_number, get_fps(), draw_overlay);
        emit nextNode(process_data);
        usleep(get_delay((unsigned int)((double) (clock() - t1) / CLOCKS_PER_SEC) * 1000000));
        cv::Mat overlay = video_data[video_name].overlay;
        if (!overlay.empty()) {
            if (!out_stream.isOpened())
                out_stream.open(overlay_name.toStdString(), 1482049860, fps, overlay.size());

            out_stream << overlay;
            cv::imshow(overlay_name.toStdString(), overlay);

            emit save_log(video_name, frame_number);
            frame_number++;
        }
        if (!process_data.data_serialized.isEmpty())
        {
            udp_socket->writeDatagram(process_data.data_serialized, *udp_addr, udp_port);
        }
        t1 = clock();
    }
    cv::destroyWindow(overlay_name.toStdString());
    while (in_stream.isOpened()) in_stream.release();
    emit EOS();
}

CVProcessingNode::CVProcessingNode(QObject *parent, bool ip_deliver_en, bool draw_overlay_en) :
    QObject(parent) {fill_buf = false; counter = 0; average_time = 0.; ip_deliever = ip_deliver_en; draw_overlay = draw_overlay_en;}

void CVProcessingNode::calcAverageTime() {
    average_time = 0.;
    if (fill_buf) {
        counter %= SIZE_BUF;
        for(int i = 0; i < SIZE_BUF; ++i) average_time += timings[i] / SIZE_BUF;
    } else {
        for(int i = 0; i < counter; ++i) average_time += timings[i] / counter;
        if (counter == SIZE_BUF) { fill_buf = true; counter %= SIZE_BUF; }
    }
}

void CVProcessingNode::process(CVProcessData process_data) {
    clock_t start = clock();
    QSharedPointer<CVNodeData> node_data = compute(process_data);
    process_data.data[QString(this->metaObject()->className())] = node_data;
    timings[counter++] = (double)(clock() - start) / CLOCKS_PER_SEC;
    calcAverageTime();
    emit pushLog(process_data.video_name, make_log(node_data));
    emit nextNode(process_data);
}
