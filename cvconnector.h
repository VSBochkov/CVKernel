#ifndef CVCONNECTOR_H
#define CVCONNECTOR_H

#include <QSharedPointer>   // Подключение файла QSharedPointer для использования одноименного класса
#include <QJsonObject>      // Подключение файла QJsonObject для использования одноименного класса
#include <QByteArray>       // Подключение файла QByteArray для использования одноименного класса
#include <QTcpSocket>       // Подключение файла QTcpSocket для использования одноименного класса
#include <QUdpSocket>       // Подключение файла QUdpSocket для использования одноименного класса
#include <QTimer>           // Подключение файла QTimer для использования одноименного класса
#include <QMap>             // Подключение файла QMap для использования одноименного класса
#include <memory>           // Подключение файла memory для использования класса unique_ptr


namespace CVKernel      // Определение области видимости
{
    class CVIONode;             // Предварительное объявление класса CVIONode
    class CVProcessForest;      // Предварительное объявление класса CVProcessForest
    class CVProcessingNode;     // Предварительное объявление класса CVProcessingNode
    class CVNetworkManager;     // Предварительное объявление класса CVNetworkManager
    class CVProcessManager;     // Предварительное объявление класса CVProcessManager
    class CVConnector;          // Предварительное объявление класса CVConnector

    struct CVConnectorType      // Определение структуры типа коннектора полученной по сети
    {
        enum type_value {client = false, supervisor = true} type;   // Целочисленное перечисление типа коннектора
        CVConnectorType(QJsonObject& json_obj);     // Объявление конструктора структуры
    };

    struct CVInputProcessor     // Определение структуры процессора приема входящих сообщений
    {
        enum class recv_state {GET_SIZE, GET_BUFFER} state = recv_state::GET_SIZE;  // Определение состояний конечного автомата приема данных
        uint64_t buffer_size = 0;       // Определение размера входящего буфера
        uint64_t bytes_received = 0;    // Определение количества принятых байт
        QByteArray buffer;              // Определение массива байт входящих данных
        bool receive_message(QTcpSocket& tcp_socket);   // Объявление метода принятия входящего сообщения
        void clear_buffer();                            // Объявление метода очистки буфера
    };

    struct CVConnectorCommand   // Определение абстрактной структуры команды коннектора - (Паттерн ООП "Команда")
    {
        virtual ~CVConnectorCommand() {}    // Определение виртуального деструктора класса
        virtual void execute() = 0;         // Объявление чисто виртуального метода выполнения команды
    };

    struct CVConnectorState     // Определение структуры состояния коннектора - (Паттерн ООП "Стратегия")
    {
        enum val {closed = 0, ready = 1, run = 2} value;    // Определение значения состояния
        std::unique_ptr<CVConnectorCommand> command;        // Определение указателя на объект команды

        CVConnectorState(CVConnectorState::val st, CVConnector& conn)   // Определение конструктора класса
            : value(st),                // Присваиваем значение состояния
              connector(conn) {}        // Присваиваем объекта-собственника состояния

        virtual ~CVConnectorState() = default;      // Определение виртуального деструктора класса
        virtual void handle_incomming_message(QByteArray& buffer) = 0;  // Объявление чисто виртуального метода обработки входящего сообщения
        QJsonObject pack_to_json();                 // Объявление метода упаковки структуры в формат JSON

        CVConnector& connector;         // Определение ссылки на объект-собственника состояния
    };

    struct CVConnectorClosed : CVConnectorState // Определение структуры состояния "Не активен" - (Паттерн ООП "Стратегия")
    {
        CVConnectorClosed(CVConnector& conn)                        // Определение конструктора класса
            : CVConnectorState(CVConnectorState::closed, conn) {}   // Инициализация базового класса

        virtual void handle_incomming_message(QByteArray& buffer) = 0;  // Объявление чисто виртуального метода обработки входящего сообщения
    };

    struct CVConnectorReady : CVConnectorState // Определение структуры состояния "Остановлен" - (Паттерн ООП "Стратегия")
    {
        CVConnectorReady(CVConnector& conn)                        // Определение конструктора класса
            : CVConnectorState(CVConnectorState::ready, conn) {}   // Инициализация базового класса

        virtual void handle_incomming_message(QByteArray& buffer) override;  // Объявление виртуального метода обработки входящего сообщения
    };

    struct CVConnectorRun : CVConnectorState // Определение структуры состояния "В процессе" - (Паттерн ООП "Стратегия")
    {
        CVConnectorRun(CVConnector& conn)                        // Определение конструктора класса
            : CVConnectorState(CVConnectorState::run, conn) {}   // Инициализация базового класса

        virtual void handle_incomming_message(QByteArray& buffer) override;  // Объявление виртуального метода обработки входящего сообщения
    };

    struct CVRunCommand : public CVConnectorCommand  // Определение стркутуры команды "Активировать" - (Паттерн ООП "Команда")
    {
        CVRunCommand(CVConnector& conn)     // Определение конструктора класса
            : connector(conn) {}            // Инициализация ссылки на объекта-собственника

        virtual void execute() override;    // Объявление виртуального метода выполнения команды

        CVConnector& connector;             // Определение ссылки на объекта-собственника команды
    };

    struct CVStopCommand : public CVConnectorCommand // Определение стркутуры команды "Остановить" - (Паттерн ООП "Команда")
    {
        CVStopCommand(CVConnector& conn)    // Определение конструктора класса
            : connector(conn) {}            // Инициализация ссылки на объекта-собственника

        virtual void execute() override;    // Объявление виртуального метода выполнения команды

        CVConnector& connector;             // Определение ссылки на объекта-собственника команды
    };

    struct CVCloseCommand : public CVConnectorCommand // Определение стркутуры команды "Закрыть" - (Паттерн ООП "Команда")
    {
        CVCloseCommand(CVConnector& conn)   // Определение конструктора класса
            : connector(conn) {}            // Инициализация ссылки на объекта-собственника

        virtual void execute() override;    // Объявление виртуального метода выполнения команды

        CVConnector& connector;             // Определение ссылки на объекта-собственника команды
    };

    struct CVConnectorFactory       // Определение структуры абстрактной фабрики коннектора - (Паттерн ООП "Абстрактная фабрика")
    {
        virtual ~CVConnectorFactory() = default;    // Определение виртуального деструктора класса
        virtual CVConnectorState* set_state(CVConnector& connector) = 0;    // Определение чисто-виртуальной функции создания состояния коннектора
    };

    struct CVCommandType            // Определение структуры типа входящей команды коннектора полученной по сети
    {
        enum command_value {run = 0, stop = 1, close = 2} command;          // Определение номера команды
        CVCommandType(QJsonObject& json_object);                            // Объявление конструктора класса
        inline bool operator== (const CVCommandType::command_value& desired_command) {  // Определение оператора равенства между командами
            return command == desired_command;  // Возврат равенства значений команд
        }
    };

    class CVConnector : public QObject      // Определение класса CVConnector - наследника класса QObject
    {
    Q_OBJECT        // Макроопределение предоставления спецификации класса модели Qt
    public:         // Открытая секция класса
        CVConnector(unsigned index, CVConnectorFactory& f, CVProcessManager& process_manager, CVNetworkManager& nm, QTcpSocket& sock);  // Объявление конструктора класса
        virtual ~CVConnector(); // Объявление виртуального деструктора

        virtual void run();         // Объявление виртуального метода выполнения команды "Активировать" - (Паттерн ООП "Команда")
        virtual void stop();        // Объявление виртуального метода выполнения команды "Остановить" - (Паттерн ООП "Команда")
        virtual void close() = 0;   // Объявление виртуального метода выполнения команды "Закрыть" - (Паттерн ООП "Команда")

        void send_buffer(QByteArray byte_arr, QTcpSocket& sock);    // Объявление метода отправки буфера через TCP сокет
        void state_changed();       // Объявление метода изменения состояния коннектора
        unsigned get_id();          // Объявление метода получения номера коннектора
        QHostAddress get_ip_address();  // Объявление метода получения IP-адреса присоединенного коннектора

    signals:    // Секция сигналов Qt
        void notify_supervisors(CVConnectorState&); // Определение сигнала оповещения супервайзеров о изменении состояния коннектора

    public slots:   // Секция слотов Qt
        void process_incoming_message();    // Определение слота принятия входящего сообщения

    protected:      // Защищенная секция класса
        unsigned id;    // Номер коннектора в системе

    private:    // Закрытая секция класса
        CVInputProcessor receiver;                  // Процессор приема входящих сообщений
        std::unique_ptr<CVConnectorState> state;    // Указатель на объект состояния коннектора

    public:     // Открытая секция класса
        CVConnectorFactory& factory;                // Ссылка на тип абстрактной фабрики создания объектов состояния коннектора

    protected:  // Защищенная секция класса
        bool running;                       // Флаг активности коннектора
        QTcpSocket& tcp_state;              // Сокет обмена конфигурационными данными и состоянием коннектора
        CVNetworkManager& network_manager;  // Сетевой менеджер
        CVProcessManager& process_manager;  // Менеджер процессов

    public:     // Открытая секция класса
        friend class CVClientFactory;       // Определение класса-друга CVClientFactory
        friend class CVSupervisorFactory;   // Определение класса-друга CVSupervisorFactory
    };
}

#endif // CVCONNECTOR_H   // Выход из области подключения
