  #ifndef CVGRAPHNODE_H
#define CVGRAPHNODE_H

#include <QMap>
#include <QObject>
#include <QSharedPointer>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>


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
        QMap<QString, QSharedPointer<CVNodeData>> data;
        CVProcessData() {}
        CVProcessData(QString video_name, double fps);
        CVProcessData(QString video_name, cv::Mat frame, int fnum, double fps);
    };

    class CVIONode : public QObject {
        Q_OBJECT
    public:
        explicit CVIONode(QObject *parent = 0, QString video_name = "", QString overlay_name = "");
        explicit CVIONode(QObject *parent = 0, int device_id = 0);
        double get_fps() { return fps; }
        useconds_t get_delay(unsigned int micros_spent) {
            unsigned int delay = (unsigned int)(1000000. / fps);
            delay += delay * fps >= 1000000. ? 0 : 1;
            return (useconds_t) (((int)delay - (int)micros_spent) > 0 ? delay - micros_spent : 0);
        }

    signals:
        void save_log(QString video_name, int frame_num);
        void nextNode(CVProcessData process_data);
        void EOS();

    public slots:
        void process();

    private:
        cv::VideoCapture in_stream;
        QString video_name;
        cv::VideoWriter out_stream;
        QString overlay_name;
        int frame_number;
        double fps;
        CVProcessData process_data;
    };

    class CVProcessingNode : public QObject {
        Q_OBJECT
    public:
        explicit CVProcessingNode(QObject *parent = 0);
        virtual QSharedPointer<CVNodeData> compute(CVProcessData &process_data) = 0;

        double averageTime() { return average_time; }
        QString make_log(QSharedPointer<CVNodeData> data) { return data->get_log(); }
    signals:
        void nextNode(CVProcessData process_data);
        void pushLog(QString video_name, QString log);

    public slots:
        virtual void process(CVProcessData process_data);

    private:
        void calcAverageTime();
    private:
        double timings[SIZE_BUF];
        double average_time;
        int counter;
        bool fill_buf;
    };

    class CVLoggerNode : public QObject {
        Q_OBJECT
    public:
        explicit CVLoggerNode(QObject *parent = 0, int frames_to_store = 0);

    public slots:
        void add_log(QString video_name, QString log);
        void save_log(QString video_name, int frame_num);

    private:
        int frames_cnt;
        QMap<QString, QString> log_map;
    };
}
Q_DECLARE_METATYPE(CVKernel::CVProcessData)

#endif // CVGRAPHNODE_H
