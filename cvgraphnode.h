#ifndef CVGRAPHNODE_H
#define CVGRAPHNODE_H

#include <QMap>
#include <QString>
#include <QSharedPointer>
#include <QUdpSocket>
#include <QTcpServer>
#include <QMutex>
#include <QWaitCondition>
#include <QJsonObject>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <iostream>


#define SIZE_BUF 20

namespace CVKernel {
    struct VideoData {
        QString video_name;
        cv::Mat frame;
        cv::Mat overlay;
        cv::Mat debug_overlay;
    };
    extern QMap<QString, VideoData> video_data;

    struct CVNodeData {
        CVNodeData() {}
        virtual ~CVNodeData() {}
        virtual QJsonObject pack_to_json();
    };

    struct CVNodeParams {
        CVNodeParams(QJsonObject&);
        virtual ~CVNodeParams() {}
        bool draw_overlay;
    };

    struct CVNodeHistory {
        CVNodeHistory() {}
        virtual void clear_history() {}
        virtual ~CVNodeHistory() {}
    };

    struct CVProcessData {
        QString video_name;
        int frame_num;
        double fps;
        QMap<QString, QSharedPointer<CVNodeData>> data;
        QMap<QString, QSharedPointer<CVNodeParams>> params;
        QMap<QString, QSharedPointer<CVNodeHistory>> history;

        CVProcessData() {}
        CVProcessData(QString video_name, cv::Mat frame, int fnum, double fps, bool draw_overlay,
                      QMap<QString, QSharedPointer<CVNodeParams>>& params,
                      QMap<QString, QSharedPointer<CVNodeHistory>>& history);
        QJsonObject pack_to_json();
    };

    class CVIONode : public QObject {
        Q_OBJECT
    public:
        explicit CVIONode(QString input_path, QString overlay_path, int frame_width, int frame_height, double fps,
                          QMap<QString, QSharedPointer<CVNodeParams>>& pars);
        explicit CVIONode(int device_id, QString overlay_path, int frame_width, int frame_height, double fps,
                          QMap<QString, QSharedPointer<CVNodeParams>>& pars);
        virtual ~CVIONode();

        double get_fps() { return fps; }

        useconds_t get_delay(unsigned int micros_spent) {
            unsigned int delay = (unsigned int)(1000000. / fps);
            delay += delay * fps >= 1000000. ? 0 : 1;
            return (useconds_t) (((int)delay - (int)micros_spent) > 0 ? delay - micros_spent : 0);
        }

        QString get_overlay_path() { return overlay_path; }

    public:
        void start();
        void stop();
        void close();
        void udp_closed();

    signals:
        void nextNode(QSharedPointer<CVProcessData> process_data, CVIONode *stream_node);

        void send_metadata(QByteArray);

        void node_started();
        void node_stopped();
        void node_closed();
        void close_udp();

    public slots:
        void process();

    private:
        cv::VideoCapture in_stream;
        QString video_path;
        cv::VideoWriter out_stream;
        QString overlay_path;
        int frame_number;
        int frame_width;
        int frame_height;
        QMap<QString, QSharedPointer<CVNodeParams>> params;
        QMutex guard_lock;
        QWaitCondition run_cv;
        double fps;
        double proc_frame_scale;
        enum class process_state {closed = 0, run, stopped} state;

    public:
        QMap<int, std::pair<clock_t,int>> video_timings;
        int nodes_number;
    };

    class CVProcessingNode : public QObject {
        Q_OBJECT
    public:
        explicit CVProcessingNode();
        virtual ~CVProcessingNode() = default;

        virtual QSharedPointer<CVNodeData> compute(QSharedPointer<CVProcessData> process_data) = 0;

        double averageTime();

    signals:
        void nextNode(QSharedPointer<CVProcessData> process_data, CVIONode *stream_node);
        void nextStat();

    public slots:
        virtual void process(QSharedPointer<CVProcessData> process_data, CVIONode *stream_node);

    public:
        QSharedPointer<QMutex> aver_time_mutex;

    private:
        void calcAverageTime();
    private:
        double timings[SIZE_BUF];
        double average_time;
        double average_delay = 0.0;
        int frame_processed = 0;
        int frame_counter;
        bool fill_buf;
    };
}
Q_DECLARE_METATYPE(CVKernel::CVProcessData)

#endif // CVGRAPHNODE_H
