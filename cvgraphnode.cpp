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

CVIONode::CVIONode(int device_id, bool draw_overlay, QString ip_addr, int ip_p, bool show_overlay) :
    QObject(NULL), show_overlay(show_overlay) {
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

CVIONode::CVIONode(QString video_name, bool draw_overlay, QString ip_addr, int ip_p, QString over_name, bool show_overlay) :
    QObject(NULL), video_name(video_name), show_overlay(show_overlay) {
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
    cv::Mat frame;
    bool draw_overlay = !overlay_name.isEmpty();
    if (show_overlay)
        cv::namedWindow(overlay_name.toStdString(), CV_WINDOW_AUTOSIZE);
    clock_t t1 = clock();
    while(in_stream.read(frame)) {
        video_timings[frame_number] = {clock(), nodes_number};
        process_data = CVProcessData(video_name, frame, frame_number, get_fps(), draw_overlay);
        emit nextNode(process_data, this);
        usleep(get_delay((unsigned int)((double) (clock() - t1) / CLOCKS_PER_SEC) * 1000000));
        cv::Mat overlay = video_data[video_name].overlay;
        if (!overlay.empty()) {
            if (!out_stream.isOpened())
                out_stream.open(overlay_name.toStdString(), 1482049860, fps, overlay.size());

            out_stream << overlay;
            if (show_overlay)
                cv::imshow(overlay_name.toStdString(), overlay);

            emit save_log(video_name, frame_number);
            frame_number++;
        }
        if (!process_data.data_serialized.isEmpty())
            udp_socket->writeDatagram(process_data.data_serialized, *udp_addr, udp_port);

        t1 = clock();

        if (frame_number % 300 == 0)
        {
            std::cout << "===============================================\n";
            emit print_stat();
        }
    }

    if (show_overlay)
        cv::destroyWindow(overlay_name.toStdString());
    while (in_stream.isOpened()) in_stream.release();
    emit EOS();
}

CVProcessingNode::CVProcessingNode(bool ip_deliver_en, bool draw_overlay_en) :
    QObject(nullptr), ip_mutex(new QMutex) {
    fill_buf = false;
    counter = 0;
    average_time = 0.;
    ip_deliever = ip_deliver_en;
    draw_overlay = draw_overlay_en;
}

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

void CVProcessingNode::process(CVProcessData process_data, CVIONode *stream_node) {
    clock_t start = clock();
    QSharedPointer<CVNodeData> node_data = compute(process_data);
    process_data.data[QString(this->metaObject()->className())] = node_data;
    timings[counter++] = (double)(clock() - start) / CLOCKS_PER_SEC;
    auto clocks = stream_node->video_timings.find(process_data.frame_num);
    if (clocks == stream_node->video_timings.end())
        std::cout << "Failure with video_timings number processes" << std::endl;
    average_delay = ((double)average_delay * frame_processed + (double)(clock() - clocks->first) / CLOCKS_PER_SEC) / ++frame_processed;
    calcAverageTime();

    if (--clocks->second == 0)
        stream_node->video_timings.erase(clocks);
    emit pushLog(process_data.video_name, make_log(node_data));
    emit nextNode(process_data, stream_node);
}

void CVProcessingNode::printStat() {
    ip_mutex->lock();
    std::cout << "Thread '" << this->metaObject()->className() << "' (" << std::hex << (unsigned long)this << ") has " << std::dec << average_delay << " seconds lag" << std::endl;
    ip_mutex->unlock();
    emit nextStat();
}
