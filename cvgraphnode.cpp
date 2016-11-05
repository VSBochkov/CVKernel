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

CVProcessData::CVProcessData(QString video_name, double fps):
    video_name(video_name) {
    fps = fps;
}

CVProcessData::CVProcessData(QString video_name, cv::Mat frame, int fnum, double fps):
    video_name(video_name) {
    video_data[video_name].frame = frame;
    video_data[video_name].debug_overlay = cv::Mat(frame.rows * 2, frame.cols, CV_8UC3);
    video_data[video_name].overlay = video_data[video_name].debug_overlay.rowRange(0, frame.rows);
    frame.copyTo(video_data[video_name].overlay);
    frame_num = fnum;
    fps = fps;
}

CVIONode::CVIONode(QObject *parent, int device_id) :
    QObject(parent), video_name(QString("device_") + QString::number(device_id)) {
    frame_number = 1;
    if (!in_stream.isOpened()) {
        in_stream.open(device_id);
        fps = in_stream.get(CV_CAP_PROP_FPS) > 1.0 ? in_stream.get(CV_CAP_PROP_FPS) : 30.;
    }
}

CVIONode::CVIONode(QObject *parent, QString video_name, QString overlay_name) :
    QObject(parent), video_name(video_name), overlay_name(overlay_name) {
    frame_number = 1;
    if (!in_stream.isOpened()) {
        in_stream.open(video_name.toStdString());
        fps = in_stream.get(CV_CAP_PROP_FPS) > 1.0 ? in_stream.get(CV_CAP_PROP_FPS) : 30.;
    }
}

void CVIONode::process() {
    cv::Mat frame, scale_frame;
    cv::namedWindow(overlay_name.toStdString(), CV_WINDOW_AUTOSIZE);
    clock_t t1 = clock();
    while(in_stream.read(frame)) {
        cv::resize(frame, scale_frame, cv::Size(), 400. / frame.cols, 400. / frame.cols);
        process_data = CVProcessData(video_name, scale_frame, frame_number, get_fps());
        emit nextNode(process_data);
        usleep(get_delay((unsigned int)((double) (clock() - t1) / CLOCKS_PER_SEC) * 1000000));
        cv::Mat overlay = video_data[video_name].debug_overlay;
        if (!overlay.empty()) {
            if (!out_stream.isOpened())
                out_stream.open(overlay_name.toStdString(), 1482049860, fps, overlay.size());

            out_stream << overlay;
            cv::imshow(overlay_name.toStdString(), overlay);

            emit save_log(video_name, frame_number);
            frame_number++;
        }
        t1 = clock();
    }
    cv::destroyWindow(overlay_name.toStdString());
    while (in_stream.isOpened()) in_stream.release();
    emit EOS();
}

CVProcessingNode::CVProcessingNode(QObject *parent) :
    QObject(parent) {fill_buf = false; counter = 0; average_time = 0.;}

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

CVLoggerNode::CVLoggerNode(QObject *parent, int frame_num) :
    QObject(parent) {
    frames_cnt = frame_num;
}

void CVLoggerNode::save_log(QString video_name, int frame_num) {
    if (frame_num % frames_cnt == 0) {
        QFile file(video_name + "_log.txt");
        if (!file.open(QIODevice::Append | QIODevice::Text))
            return;
        QTextStream stream(&file);
        stream << log_map[video_name];
        file.close();
        log_map[video_name].clear();
    }
}

void CVLoggerNode::add_log(QString video_name, QString log) {
    if (log_map.contains(video_name))
        if (log_map[video_name].isEmpty())
            log_map[video_name] = log;
        else
            log_map[video_name] += log;
    else
        log_map[video_name] = log;
}
