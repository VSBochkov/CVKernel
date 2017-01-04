#ifndef CVGRAPHNODE_H
#define CVGRAPHNODE_H

#include <QMap>
#include <QObject>
#include <QSharedPointer>
#include <QUdpSocket>
#include <QLocalSocket>
#include <QMutex>
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

    class CVNodeData {
    public:
        CVNodeData() {}
        virtual ~CVNodeData() {}
        virtual QString get_log() { return ""; }
    };

    struct CVProcessData {
        int frame_num;
        double fps;
        QString video_name;
        QByteArray data_serialized;
        QMap<QString, QSharedPointer<CVNodeData>> data;
        CVProcessData() {}
        CVProcessData(QString video_name, cv::Mat frame, int fnum, double fps, bool draw_overlay);
    };

    class CVIONode : public QObject {
        Q_OBJECT
    public:
        explicit CVIONode(QString video_name = "", bool draw_overlay = false, QString cli_udp_addr = "", int cli_udp_p = 0, int srv_unix_p = 0, QString overlay_name = "", bool show_overlay = false, bool store_output = false, int frame_width = 320, int frame_height = 240);
        explicit CVIONode(int device_id = 0, bool draw_overlay = false, QString cli_udp_addr = "", int cli_udp_p = 0, int srv_unix_p = 0, bool show_overlay = false, bool store_output = false, int frame_width = 320, int frame_height = 240);
        virtual ~CVIONode() {
            if (client_tx_meta_udp_addr == nullptr)
                return;
            delete client_tx_meta_udp_addr;
            client_udp_socket->close();
            delete client_udp_socket;
        }

        double get_fps() { return fps; }
        useconds_t get_delay(unsigned int micros_spent) {
            unsigned int delay = (unsigned int)(1000000. / fps);
            delay += delay * fps >= 1000000. ? 0 : 1;
            return (useconds_t) (((int)delay - (int)micros_spent) > 0 ? delay - micros_spent : 0);
        }

    signals:
        void nextNode(CVProcessData process_data, CVIONode *stream_node);
        void EOS();
        void print_stat();

    public slots:
        void process();
        void getState();

    private:
        void storeLog();

    private:
        cv::VideoCapture in_stream;
        QString video_name;
        cv::VideoWriter out_stream;
        QString overlay_name;
        double fps;
        QHostAddress* client_tx_meta_udp_addr;
        quint16 client_tx_meta_udp_port;
        CVProcessData process_data;
        QUdpSocket* client_udp_socket;
        QLocalSocket* server_unix_socket;
        quint16 server_rx_state_udp_port;
        bool show_overlay;
        double proc_frame_scale;
        enum class process_state {disable = 0, enable, conn_closed} proc_enabled;

    public:
        QMap<int, std::pair<clock_t,int>> video_timings;
        QMap<std::pair<int, QString>, QSharedPointer<CVNodeData>> stored_output;
        bool store_output;
        int nodes_number;
        int frame_number;
        int frame_width;
        int frame_height;
    };

    class CVProcessingNode : public QObject {
        Q_OBJECT
    public:
        explicit CVProcessingNode(bool ip_deliever_en = false, bool draw_overlay = false);
        virtual ~CVProcessingNode() = default;

        virtual QSharedPointer<CVNodeData> compute(CVProcessData &process_data) = 0;

        double averageTime() { return average_time; }
        QString make_log(QSharedPointer<CVNodeData> data) { return data->get_log(); }
    signals:
        void nextNode(CVProcessData process_data, CVIONode *stream_node);
        void pushLog(QString video_name, QString log);
        void nextStat();

    public slots:
        virtual void process(CVProcessData process_data, CVIONode *stream_node);
        virtual void printStat();

    public:
        bool draw_overlay;
        bool ip_deliever;
        QSharedPointer<QMutex> ip_mutex;

    private:
        void calcAverageTime();
    private:
        double timings[SIZE_BUF];
        double average_time;
        double average_delay = 0.0;
        int frame_processed = 0;
        int counter;
        bool fill_buf;
    };
}
Q_DECLARE_METATYPE(CVKernel::CVProcessData)

#endif // CVGRAPHNODE_H
