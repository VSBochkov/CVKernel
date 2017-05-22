#ifndef CVCLIENT_H
#define CVCLIENT_H

#include <QJsonObject>  // Подключение файла QJsonObject для использования одноименного класса
#include <QByteArray>   // Подключение файла QByteArray для использования одноименного класса
#include <QTcpSocket>   // Подключение файла QTcpSocket для использования одноименного класса
#include <QUdpSocket>   // Подключение файла QUdpSocket для использования одноименного класса
#include <QTimer>       // Подключение файла QTimer для использования одноименного класса
#include <QMap>         // Подключение файла QMap для использования одноименного класса
#include <memory>       // Подключение файла memory для использования класса unique_ptr
#include "cvconnector.h" // Подключение файла cvconnector.h для использования базовых классов

namespace CVKernel {            // Определяем область видимости CVKernel
    struct CVIONode;            // Предварительное объявление структуры CVIONode
    struct CVProcessForest;     // Предварительное объявление структуры CVProcessForest
    struct CVProcessManager;    // Предварительное объявление структуры CVProcessManager
    struct CVNetworkManager;    // Предварительное объявление структуры CVNetworkManager


    struct CVClientFactory : public CVConnectorFactory  // Определение класса CVClientFactory, наследника класса CVConnectorFactory (Паттерн "Абстрактная фабрика")
    {
        CVClientFactory(const CVClientFactory&) = delete;               // Предотвращаем копирование
        CVClientFactory& operator==(const CVClientFactory&) = delete;   // объекта класса
        virtual ~CVClientFactory() = default;                           // Определение виртуального деструктора по-умолчанию
        virtual CVConnectorState* set_state(CVConnector& connector) override;   // Виртуальный метод создания объекта производного от CVConnectorState класса
        static CVClientFactory* instance()  // Статический метод взятия единственного объекта (Паттерн "Одиночка)
        {
            static CVClientFactory inst;    // Создаем статический объект фабрики (создается один раз, далее переиспользуется)
            return &inst;                   // Возвращаем адрес объекта
        }
    private:    // Закрытая секция класса
        CVClientFactory() {}    // Предотвращаем построение объекта из внешнего кода
    };

    class CVClient : public CVConnector // Определение класса CVClient - наследника CVConnector
    {
    Q_OBJECT    // Макроопределение спецификации Qt класса
    public:
        explicit CVClient(unsigned index, CVProcessManager& pm, CVNetworkManager& nm, QTcpSocket& tcp); // Объявление конструктора класса
        virtual ~CVClient();                    // Объявление виртуального деструктора класса
        void set_io_node(CVIONode& io_node);    // Объявление метода инициализации ссылки на узел ввода-вывода
        QString get_overlay_path();             // Объявление метода получения пути до исходящего видео
        int get_proc_nodes_number();            // Объявление метода получения количества процессорных узлов используемых клиентом
        void init_process_forest(QSharedPointer<CVProcessForest> proc_forest);    // Объявление метода инициализации леса видеоаналитики
        virtual void run() override;            // Объявление виртуального метода выполнения команды run - (Паттерн ООП "Команда")
        virtual void stop() override;           // Объявление виртуального метода выполнения команды stop - (Паттерн ООП "Команда")
        virtual void close() override;          // Объявление виртуального метода выполнения команды close - (Паттерн ООП "Команда")

    signals:    // Секция Qt сигналов
        void client_disabled(QTcpSocket& tcp_state); // Определение Qt-сигнала неактивности клиента

    public slots:    // Секция обработчиков сигналов Qt (слотов)
        void send_datagramm(QByteArray byte_arr);    // Объявление Qt-слота посылки датаграммы через UDP
        void do_close_udp();                         // Объявление Qt-слота закрытия UDP сессии
        void on_stream_closed();                     // Объявление Qt-слота на закрытие узла ввода-вывода

    private:    // Закрытая секция
        std::unique_ptr<QUdpSocket> meta_udp_client; // Определение уникального указателя на клиентский UDP сокет посылки метаданных
        QHostAddress udp_address;                    // Определение IP-адреса подключенного клиента для передачи данных по UDP
        qint16 udp_port;                             // Определение порта передачи метаданных через UDP
        CVIONode* video_io;                          // Объявление указателя на узел ввода-вывода используемый клиентом
        friend class CVClientFactory;                // Определение дружественного класса CVClientFactory (имеет доступ к закрытой секции)
    };

    struct CVInitProcTreeCommand : public CVConnectorCommand // Определение класса CVInitProcTreeComand наследника класса "команды" - CVConnectorCommand - (Паттерн ООП "Команда")
    {
        CVInitProcTreeCommand(CVClient& cl, QSharedPointer<CVProcessForest> pf) // Определение конструктора класса
            : client(cl),        // Присвоение ссылки на клиента
              proc_forest(pf) {} // Присвоение указателя на спецификацию видеоаналитики

        virtual void execute() override; // Объявление виртуальной функции выполнения команды

        CVClient& client;   // Объявление ссылки на клиента который будет исполнять команду
        QSharedPointer<CVProcessForest> proc_forest;    // Объявление указателя на спецификацию видеопроцессинга
    };

    struct CVClientClosed : public CVConnectorClosed    // Определение класса CVClientClosed наследника класса "состояния" CVConnectorClosed - (Паттерн ООП "Стратегия")
    {
        CVClientClosed(CVClient& c)     // Определение конструктора класса
            : CVConnectorClosed(c) {}   // Инициализация базового класса

        virtual void handle_incomming_message(QByteArray& buffer) override; // Объявление виртуального метода обработки входящего сообщения
    };
}

#endif // CVCLIENT_H    // Выход из области подключения
