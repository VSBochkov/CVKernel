#include "cvgraphnode.h"
#include "cvjsoncontroller.h"
#include "cvfactorycontroller.h"

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

CVKernel::CVIONode::CVIONode(int device_id, QString over_path, int width, int height, double framespersecond,
                             QMap<QString, QSharedPointer<CVNodeParams>>& pars)
    : QObject(NULL),
      video_path(QString("video") + QString::number(device_id)),
      overlay_path(over_path),
      frame_number(1),
      frame_width(width),
      frame_height(height),
      params(pars)
{
    in_stream.open(device_id);

    if (in_stream.get(CV_CAP_PROP_FPS) > 1.0)
        fps = in_stream.get(CV_CAP_PROP_FPS);
    else
        fps = framespersecond;

    in_stream.set(CV_CAP_PROP_FRAME_WIDTH, frame_width);
    in_stream.set(CV_CAP_PROP_FRAME_HEIGHT, frame_height);
    in_stream.set(CV_CAP_PROP_FPS, fps);

    if (not overlay_path.isEmpty())
        out_stream.open(overlay_path.toStdString(), 1482049860, fps, cv::Size(frame_width, frame_height));

    state = process_state::closed;
}

CVKernel::CVIONode::CVIONode(QString input_path, QString over_path, int width, int height, double framespersecond,
                             QMap<QString, QSharedPointer<CVNodeParams>>& pars)
    : QObject(NULL),
      video_path(input_path),
      overlay_path(over_path),
      frame_number(1),
      frame_width(width),
      frame_height(height),
      params(pars)
{
    in_stream.open(video_path.toStdString());

    if (in_stream.get(CV_CAP_PROP_FPS) > 1.0)
        fps = in_stream.get(CV_CAP_PROP_FPS);
    else
        fps = framespersecond;

    in_stream.set(CV_CAP_PROP_FRAME_WIDTH, frame_width);
    in_stream.set(CV_CAP_PROP_FRAME_HEIGHT, frame_height);
    in_stream.set(CV_CAP_PROP_FPS, fps);

    if (not overlay_path.isEmpty())
        out_stream.open(overlay_path.toStdString(), 1482049860, fps, cv::Size(frame_width, frame_height));

    state = process_state::closed;
}

CVKernel::CVIONode::~CVIONode()
{
    if (in_stream.isOpened()) {
        in_stream.release();
    }
    if (out_stream.isOpened()) {
        out_stream.release();
    }
}

void CVKernel::CVIONode::start()
{
    guard_lock.lock();
        if(state != process_state::run) {
            state = process_state::run;
            run_cv.wakeOne();
        }
    guard_lock.unlock();
}

void CVKernel::CVIONode::stop()
{
    guard_lock.lock();
        if(state != process_state::stopped) {
            state = process_state::stopped;
        }
    guard_lock.unlock();
}

void CVKernel::CVIONode::close()
{
    guard_lock.lock();
        if(state != process_state::closed) {
            state = process_state::closed;
        }
        run_cv.wakeOne();
    guard_lock.unlock();
}

void CVKernel::CVIONode::process()
{
    guard_lock.lock();
        if (state != process_state::run)
            run_cv.wait(&guard_lock);

        if (state == process_state::run)
            emit node_started();
    guard_lock.unlock();

    cv::Mat frame;
    QMap<QString, QSharedPointer<CVNodeHistory>> history = CVKernel::CVFactoryController::get_instance().create_history(params.keys());

    clock_t t1 = clock();
    while(state != process_state::closed and in_stream.read(frame)) {
        switch (state) {
            case process_state::run: {
                video_timings[frame_number] = {clock(), nodes_number};
                QSharedPointer<CVProcessData> process_data(
                    new CVProcessData(
                        video_path,
                        frame,
                        frame_number,
                        get_fps(),
                        out_stream.isOpened(),
                        params,
                        history
                    )
                );
                emit nextNode(process_data, this);
                usleep(get_delay((unsigned int)((double) (clock() - t1) / CLOCKS_PER_SEC) * 1000000));
                out_stream << video_data[video_path].overlay;
                emit send_metadata(CVJsonController::pack_to_json_ascii(*process_data));
                t1 = clock();
                break;
            }
            default: {
                for (auto hist_item : history) {
                    hist_item->clear_history();
                }
                emit node_stopped();
                guard_lock.lock();
                    if (state == process_state::stopped)
                        run_cv.wait(&guard_lock);

                    if (state == process_state::run) {
                        t1 = clock();
                        emit node_started();
                    }
                guard_lock.unlock();
                break;
            }
        }
    }


    guard_lock.lock();
    emit close_udp();
    run_cv.wait(&guard_lock);
    guard_lock.unlock();
    emit node_closed();
}

void CVKernel::CVIONode::udp_closed() {
    guard_lock.lock();
        run_cv.wakeOne();
    guard_lock.unlock();
}

CVKernel::CVProcessingNode::CVProcessingNode() :
    QObject(nullptr), aver_time_mutex(new QMutex) {
    fill_buf = false;
    frame_counter = 0;
    average_time = 0.;
    frame_processed = 0;
}

void CVKernel::CVProcessingNode::calcAverageTime() {
    aver_time_mutex->lock();
    average_time = 0.;

    if (fill_buf) {
        frame_counter %= SIZE_BUF;
        for(int i = 0; i < SIZE_BUF; ++i) average_time += timings[i] / SIZE_BUF;
    } else {
        for(int i = 0; i < frame_counter; ++i) average_time += timings[i] / frame_counter;
        if (frame_counter == SIZE_BUF) { fill_buf = true; frame_counter %= SIZE_BUF; }
    }
    aver_time_mutex->unlock();
}

void CVKernel::CVProcessingNode::process(QSharedPointer<CVProcessData> process_data, CVIONode *stream_node) {
    clock_t start = clock();
    QSharedPointer<CVNodeData> node_data = compute(process_data);
    process_data->data[QString(this->metaObject()->className())] = node_data;
    timings[frame_counter++] = (double)(clock() - start) / CLOCKS_PER_SEC;
    auto clocks = stream_node->video_timings.find(process_data->frame_num);
    if (clocks != stream_node->video_timings.end())
        average_delay = ((double)average_delay * frame_processed + (double)(clock() - clocks->first) / CLOCKS_PER_SEC) / ++frame_processed;
    calcAverageTime();

    if (--clocks->second == 0)
        stream_node->video_timings.erase(clocks);

    emit nextNode(process_data, stream_node);
}

double CVKernel::CVProcessingNode::averageTime()
{
    aver_time_mutex->lock();
    double result = average_time;
    aver_time_mutex->unlock();
    return result;
}
