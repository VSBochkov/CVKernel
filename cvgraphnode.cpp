#include "cvgraphnode.h"
#include "cvjsoncontroller.h"
#include "cvfactorycontroller.h"
#include "cvapplication.h"

#include <QTimer>
#include <opencv2/opencv.hpp>
#include <utility>
#include <unistd.h>
#include <time.h>

#include "videoproc/firebbox.h"

QMap<QString, CVKernel::VideoData> CVKernel::video_data;

CVKernel::CVProcessData::CVProcessData(QString video_name, cv::Mat frame, int fnum, double fps, bool draw_overlay,
                                       QMap<QString, QSharedPointer<CVNodeParams>>& pars,
                                       QMap<QString, QSharedPointer<CVNodeHistory>>& hist)
    : video_name(video_name), frame_num(fnum), fps(fps)
{
    params = pars;
    history = hist;
    video_data[video_name].frame = frame;
    if (draw_overlay) {
        frame.copyTo(video_data[video_name].overlay);
    }
}

CVKernel::CVIONode::CVIONode(int device_id, QString output_path,
                             int width, int height, double framespersecond,
                             QMap<QString, QSharedPointer<CVNodeParams>>& pars)
    : QObject(NULL),
      video_path(QString("video") + QString::number(device_id)),
      frame_number(1),
      frame_width(width),
      frame_height(height),
      params(pars)
{
    in_stream.open(device_id);
    if (not in_stream.isOpened())
    {
        qDebug() << "Video stream is not opened";
        return;
    }

    if (in_stream.get(CV_CAP_PROP_FPS) > 1.0)
        stopwatch.fps = in_stream.get(CV_CAP_PROP_FPS);
    else
        stopwatch.fps = framespersecond;

    qDebug() << "USB cam: resolution: [" << (int) in_stream.get(CV_CAP_PROP_FRAME_WIDTH) << ", "
             << (int) in_stream.get(CV_CAP_PROP_FRAME_HEIGHT) << "] fps = " << in_stream.get(CV_CAP_PROP_FPS);

    qDebug() << "Setup: resolution: [" << width << ", " << height << "] fps = " << stopwatch.fps;

    in_stream.set(CV_CAP_PROP_FRAME_WIDTH, width);
    in_stream.set(CV_CAP_PROP_FRAME_HEIGHT, height);
    in_stream.set(CV_CAP_PROP_FPS, stopwatch.fps);

    frame_width = (int) in_stream.get(CV_CAP_PROP_FRAME_WIDTH);
    frame_height = (int) in_stream.get(CV_CAP_PROP_FRAME_HEIGHT);

    qDebug() << "Calculate: resolution: [" << frame_width << ", " << frame_height << "] fps = " << in_stream.get(CV_CAP_PROP_FPS);

    int colon_id = output_path.indexOf(":");
    if (not output_path.isEmpty() and colon_id > 0)
    {
        QString protocol = output_path.left(colon_id);
        overlay_path = (protocol == "file") ? output_path.mid(colon_id + 1) :
                        protocol + "://" + get_ip_address() + ":" + output_path.mid(colon_id + 1);
    }
    else
    {
        overlay_path = "";
    }

    state = std::make_unique<CVIONodeClosed>(*this);
}

CVKernel::CVIONode::CVIONode(QString input_path, QString output_path,
                             int width, int height, double framespersecond,
                             QMap<QString, QSharedPointer<CVNodeParams>>& pars)
    : QObject(NULL),
      video_path(input_path),
      frame_number(1),
      frame_width(width),
      frame_height(height),
      params(pars)
{
    in_stream.open(video_path.toStdString());
    if (not in_stream.isOpened())
    {
        qDebug() << "Video stream is not opened";
        return;
    }

    if (in_stream.get(CV_CAP_PROP_FPS) > 1.0)
        stopwatch.fps = in_stream.get(CV_CAP_PROP_FPS);
    else
        stopwatch.fps = framespersecond;

    qDebug() << "Video: resolution: [" << (int) in_stream.get(CV_CAP_PROP_FRAME_WIDTH) << ", "
             << (int) in_stream.get(CV_CAP_PROP_FRAME_HEIGHT) << "] fps = " << in_stream.get(CV_CAP_PROP_FPS);

    in_stream.set(CV_CAP_PROP_FRAME_WIDTH, width);
    in_stream.set(CV_CAP_PROP_FRAME_HEIGHT, height);
    in_stream.set(CV_CAP_PROP_FPS, stopwatch.fps);

    frame_width = (int) in_stream.get(CV_CAP_PROP_FRAME_WIDTH);
    frame_height = (int) in_stream.get(CV_CAP_PROP_FRAME_HEIGHT);

    qDebug() << "Calculate: resolution: [" << frame_width << ", " << frame_height << "] fps = " << in_stream.get(CV_CAP_PROP_FPS);

    int colon_id = output_path.indexOf(":");
    if (not output_path.isEmpty() and colon_id > 0)
    {
        QString protocol = output_path.left(colon_id);
        overlay_path = (protocol == "file") ? output_path.mid(colon_id + 1) :
                        protocol + "://" + get_ip_address() + ":" + output_path.mid(colon_id + 1);
    }
    else
    {
        overlay_path = "";
    }

    state = std::make_unique<CVIONodeClosed>(*this);
}

CVKernel::CVIONode::~CVIONode()
{
    if (in_stream.isOpened()) {
        in_stream.release();
        qDebug() << "in_stream.release();";
    }
    if (out_stream.isOpened()) {
        out_stream.release();
        qDebug() << "out_stream.release();";
    }
}

void CVKernel::CVIONode::start()
{
    guard_lock.lock();
        state.reset(new CVIONodeRun(*this));
        state_changed_cv.wakeOne();
    guard_lock.unlock();
}

void CVKernel::CVIONode::stop()
{
    guard_lock.lock();
        state.reset(new CVIONodeStopped(*this));
    guard_lock.unlock();
}

void CVKernel::CVIONode::close()
{
    guard_lock.lock();
        state.reset(new CVIONodeClosed(*this));
        state_changed_cv.wakeOne();
    guard_lock.unlock();
}

void CVKernel::CVIONode::process()
{
    guard_lock.lock();
        state_changed_cv.wait(&guard_lock);
    guard_lock.unlock();

    history = CVKernel::CVFactoryController::get_instance().create_history(params.keys());

    while(state.get() != nullptr)
        state->process();
}

void CVKernel::CVIONodeRun::process()
{
    io_node.on_run();
}

void CVKernel::CVIONodeStopped::process()
{
    io_node.on_stopped();
}

void CVKernel::CVIONodeClosed::process()
{
    io_node.on_closed();
}

void CVKernel::CVIONode::on_run()
{
    cv::Mat frame;
    stopwatch.start();
    if (not in_stream.read(frame))
    {
        state.reset(new CVIONodeClosed(*this));
        return;
    }
    if ((not overlay_path.isEmpty()) and (not out_stream.isOpened()))
    {
        out_stream.open(overlay_path.toStdString(), 1482049860, stopwatch.fps, frame.size());
    }
    QSharedPointer<CVProcessData> process_data(
        new CVProcessData(
            video_path, frame, frame_number,
            get_fps(), out_stream.isOpened(),
            params, history
        )
    );
    emit next_node(process_data);
    usleep(stopwatch.time());
    out_stream << video_data[video_path].overlay;
    emit send_metadata(CVJsonController::pack_to_json_ascii<CVProcessData>(*process_data.data()));
    frame_number++;
}

void CVKernel::CVIONode::on_stopped()
{
    for (auto hist_item : history)
    {
        hist_item->clear_history();
    }

    if ((not overlay_path.isEmpty()) and (out_stream.isOpened()))
    {
        out_stream.release();
    }

    guard_lock.lock();
        state_changed_cv.wait(&guard_lock);
    guard_lock.unlock();
}

void CVKernel::CVIONode::on_closed()
{
    guard_lock.lock();
        emit close_udp();
        state_changed_cv.wait(&guard_lock);
    guard_lock.unlock();
    state.reset(nullptr);
    emit node_closed();
}

void CVKernel::CVIONode::udp_closed() {
    guard_lock.lock();
        state_changed_cv.wakeOne();
    guard_lock.unlock();
}

CVKernel::CVProcessingNode::CVProcessingNode() :
    QObject(nullptr)
{
    fill_buf = false;
    frame_counter = 0;
    average_time = 0.;
    frame_processed = 0;
}

double CVKernel::CVProcessingNode::calc_average_time()
{
    double acc_time = 0.;

    if (fill_buf)
    {
        frame_counter %= SIZE_BUF;
        for(int i = 0; i < SIZE_BUF; ++i)
        {
            acc_time += timings[i];
        }
        acc_time /= SIZE_BUF;
    }
    else
    {
        for(int i = 0; i < frame_counter; ++i)
        {
            acc_time += timings[i];
        }
        acc_time /= frame_counter;
        if (frame_counter == SIZE_BUF)
        {
            fill_buf = true;
            frame_counter %= SIZE_BUF;
        }
    }
    return acc_time;
}

void CVKernel::CVProcessingNode::process(QSharedPointer<CVProcessData> process_data)
{
    clock_t start = clock();

    QSharedPointer<CVNodeData> node_data = compute(process_data);
    process_data->data[QString(this->metaObject()->className())] = node_data;

    timings[frame_counter++] = (double)(clock() - start) / CLOCKS_PER_SEC;
    average_time = calc_average_time();

    emit next_node(process_data);
}

double CVKernel::CVProcessingNode::get_average_time()
{
    return average_time;
}

void CVKernel::CVProcessingNode::reset_average_time()
{
    average_time = 0.;
}
