#ifndef CVGRAPHNODE_H
#define CVGRAPHNODE_H

#include <QMap>                         // Подключение файла QMap для использования одноименного класса
#include <QString>                      // Подключение файла QString для использования одноименного класса
#include <QSharedPointer>               // Подключение файла QSharedPointer для использования одноименного класса
#include <QUdpSocket>                   // Подключение файла QUdpSocket для использования одноименного класса
#include <QTcpServer>                   // Подключение файла QTcpServer для использования одноименного класса
#include <QMutex>                       // Подключение файла QMutex для использования одноименного класса
#include <QWaitCondition>               // Подключение файла QWaitCondition для использования одноименного класса
#include <QJsonObject>                  // Подключение файла QJsonObject для использования одноименного класса
#include <opencv2/core/core.hpp>        // Подключение файла core.hpp для использования класса Mat
#include <opencv2/highgui/highgui.hpp>  // Подключение файла highgui.hpp для использования классов VideoCapture и VideoWriter

#include <memory>                       // Подключение файла memory для использования одноименного класса

namespace CVKernel    // Определение области видимости
{
    struct VideoData {      // Определение структуры данных обрабатываемого видео
        cv::Mat frame;      // Кадр входящего видео
        cv::Mat overlay;    // Кадр исходящего видео
    };

    extern QMap<QString, VideoData> video_data; // Объявление словаря видеоданных

    struct CVNodeData {             // Определение структуры данных процессорного узла
        virtual ~CVNodeData() {}    // Определение виртуального деструктора
        virtual QJsonObject pack_to_json(); // Определение виртуального метода сериализации объекта в JSON формат
    };

    struct CVNodeParams {           // Определение структуры параметров процессорного узла
        CVNodeParams(QJsonObject&); // Объявление конструктора
        virtual ~CVNodeParams() {}  // Определение виртуального деструктора
        bool draw_overlay;          // Флаг отрисовки оверлея
    };

    struct CVNodeHistory {              // Определение структуры истории процессорного узла
        virtual void clear_history() {} // Определение виртуального метода очистки истории
        virtual ~CVNodeHistory() {}     // Определение виртуального деструктора
    };

    struct CVProcessData {              // Определение структуры данных видеоаналитики
        QString video_name;             // Имя входящего видео
        int frame_num;                  // Номер текущего кадра
        double fps;                     // Количество кадров в секунду
        QMap<QString, QSharedPointer<CVNodeData>> data; // Словарь хранения данных процессорных узлов
        QMap<QString, QSharedPointer<CVNodeParams>> params; // Словарь хранения параметров процессорных узлов
        QMap<QString, QSharedPointer<CVNodeHistory>> history;   // Словарь хранения истории процессорных узлов

        CVProcessData() {}  // Объявление конструктора класса
        CVProcessData(QString video_name, cv::Mat frame, int fnum, double fps, bool draw_overlay,   // Объявление
                      QMap<QString, QSharedPointer<CVNodeParams>>& params,                          // конструктора
                      QMap<QString, QSharedPointer<CVNodeHistory>>& history);                       // класса
        QJsonObject pack_to_json();     // Объявление метода сериализации объекта в JSON формат
    };

    enum class ionode_state { closed = 0, run, stopped };   // Определение перечисления состояний узла ввода-вывода

    class CVIONode;     // Предварительное объявление класса CVIONode

    struct CVIONodeState    // Определение абстрактной структуры состояния узла ввода-вывода - (Паттерн ООП "Стратегия")
    {
        CVIONodeState(CVIONode& node) : io_node(node) {}    // Определение конструктора класса
        virtual ~CVIONodeState() = default;                 // Определение виртуального деструктора
        virtual void process() = 0;                         // Объявление чисто-виртуальной функции работы состояния
        CVIONode& io_node;                                  // Определение ссылки на объект собственник состояния
    };

    struct CVIONodeRun : CVIONodeState      // Определение структуры состояния "На исполнении"
    {
        CVIONodeRun(CVIONode& node) : CVIONodeState(node) {}    // Определение конструктора класса
        virtual void process() override;    // Объявление виртуальной функции работы состояния
    };

    struct CVIONodeStopped : CVIONodeState      // Определение структуры состояния "Остановлен"
    {
        CVIONodeStopped(CVIONode& node) : CVIONodeState(node) {}    // Определение конструктора класса
        virtual void process() override;    // Объявление виртуальной функции работы состояния
    };

    struct CVIONodeClosed : CVIONodeState      // Определение структуры состояния "Деактивирован"
    {
        CVIONodeClosed(CVIONode& node) : CVIONodeState(node) {}    // Определение конструктора класса
        virtual void process() override;    // Объявления виртуальной функции работы состояния
    };

    struct CVStopwatch  // Определение структуры секундомера
    {
        void start() { t1 = clock(); }      // Определение метода начального отсчета
        useconds_t time() {     // Определение метода вычисления времени задержки
            unsigned int micros_spent = (unsigned int)((double) (clock() - t1) / CLOCKS_PER_SEC) * 1000000; // Количество потраченных микросекунд
            unsigned int frame_time = (unsigned int)(1000000. / fps);    // Время жизни кадра
            frame_time += frame_time * fps >= 1000000. ? 0 : 1;               // Инкремент времени жизни кадра
            return (useconds_t) (((int)frame_time - (int)micros_spent) > 0 ? frame_time - micros_spent : 0);  // Возврат разности между временем кадра и потраченного времени
        }

        clock_t t1; // Точка отсчета времени
        double fps; // Количество кадров в секунду
    };

    class CVIONode : public QObject // Определение класса узла ввода-вывода
    {
        Q_OBJECT    // Макроопределение спецификации Qt класса
    public: // Открытая секция класса
        explicit CVIONode(int device_id, QString input_path, QString overlay_path, int frame_width, int frame_height, double fps,   // Объявление
                          QMap<QString, QSharedPointer<CVNodeParams>>& pars);                                                       // конструктора класса
        virtual ~CVIONode();    // Объявление виртуального деструктора
        double get_fps() { return stopwatch.fps; }  // Определение метода взятия FPS
        QString get_overlay_path() { return generate_overlay_path(overlay_path); }  // Определение метода получения пути до оверлея
        void udp_closed();  // Объявление метода оповещения о закрытии сессии передачи метаданных через UDP

        void on_run();      // Объявление метода выполнения действий в состоянии "На исполнении" - (Паттерн ООП "Стратегия")
        void on_stopped();  // Объявление метода выполнения действий в состоянии "Остановлен" - (Паттерн ООП "Стратегия")
        void on_closed();   // Объявление метода выполнения действий в состоянии "Деактивирован" - (Паттерн ООП "Стратегия")

        void start();       // Объявление метода выполнения команды активации
        void stop();        // Объявление метода выполнения команды остановки
        void close();       // Объявление метода выполнения команды деактивации


    private: // Закрытая секция класса
        QString generate_overlay_path(QString output_path); // Объявление метода генерации пути до оверлея по шаблону

    signals: // Секция Qt-сигналов
        void next_node(QSharedPointer<CVProcessData> process_data); // Определение сигнала передачи вычислений корневому процессорному узлу
        void send_metadata(QByteArray); // Определение сигнала передачи метаданных
        void close_udp();   // Определение сигнала закрытия канала передачи метаданных
        void node_closed(); // Определение сигнала о закрытии узла ввода-вывода

    public slots:   // Секция слотов Qt
        void process(); // Объявление метода выполнения потока узла ввода-вывода

    private:  // Закрытая секция класса
        cv::VideoCapture in_stream;     // Структура входящего видео
        QString video_path;             // Имя входящего видео
        cv::VideoWriter out_stream;     // Структура исходящего видео
        QString overlay_path;           // Имя исходящего видео
        int frame_number;               // Номер кадра
        int frame_width;                // Ширина кадра
        int frame_height;               // Высота кадра
        QMap<QString, QSharedPointer<CVNodeParams>> params; // Словарь параметров видеоаналитики
        QMutex guard_lock;                  // Мьютекс
        QWaitCondition state_changed_cv;    // Условная переменная изменения статуса
        std::unique_ptr<CVIONodeState> state;   // Указатель на статус узла ввода-вывода
        QMap<QString, QSharedPointer<CVNodeHistory>> history;   // Словарь истории видеоаналитики
        CVStopwatch stopwatch;      // Секундомер
    public: // Открытая секция класса
        int nodes_number;   // Число процессорных узлов используемых узлом ввода-вывода
    };

    class CVProcessingNode : public QObject // Определение абстрактного класса процессорного узла
    {
        Q_OBJECT       // Макроопределение спецификации Qt класса
    public:     // Открытая секция класса
        explicit CVProcessingNode();    // Объявление конструктора класса процессорного узла
        virtual ~CVProcessingNode() = default;  // Определение дестркутора класса
        virtual QSharedPointer<CVNodeData> compute(QSharedPointer<CVProcessData> process_data) = 0; // Определение чисто-виртуальной функции вычислений над кадром
    signals:    // Секция сигналов Qt
        void next_node(QSharedPointer<CVProcessData> process_data); // Определение сигнала передачи вычислений дочерним узлам дерева
    public slots:   // Секция слотов Qt
        virtual void process(QSharedPointer<CVProcessData> process_data);  // Объявление метода проведения анализа над кадром
        void reset_average_time();  // Объявление метода сброса среднего времени итерации анализа
    private:        // Закрытая секция класса
        double calc_average_time(); // Объявление метода вычисления среднего времени итерации анализа
        static const int window_size = 20;  // Определение окна усреднения массива времени итераций потраченных на анализ
        double timings[window_size];        // Определение массива времени
        int frame_counter;                  // Определение счетчика кадров
    public:         // Открытая секция класса
        double average_time;                // Определение среднего времени выполнения итерации анализа
    };
}
Q_DECLARE_METATYPE(CVKernel::CVProcessData) // Декларация типа данных видеоаналитики для передачи в модели сигнал-слот библиотеки Qt

#endif // CVGRAPHNODE_H   // Выход из области подключения
