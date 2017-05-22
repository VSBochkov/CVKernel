#ifndef CVNETWORKMANAGER_H
#define CVNETWORKMANAGER_H

#include <QObject>      // Подключение файла QObject для использования одноименного класса
#include <QTimer>       // Подключение файла QTimer для использования одноименного класса
#include <QTcpSocket>   // Подключение файла QTcpSocket для использования одноименного класса
#include <QTcpServer>   // Подключение файла QTcpServer для использования одноименного класса
#include <QUdpSocket>   // Подключение файла QUdpSocket для использования одноименного класса

namespace CVKernel      // Определение области видимости
{
    class CVClient;         // Предварительное объявление класса CVClient
    class CVSupervisor;     // Предварительное объявление класса CVSupervisor
    class CVIONode;         // Предварительное объявление класса CVIONode
    class CVConnector;      // Предварительное объявление класса CVConnector
    class CVInputProcessor; // Предварительное объявление класса CVInputProcessor
    class CVProcessManager; // Предварительное объявление класса CVProcessManager
    class CVProcessForest;  // Предварительное объявление класса CVProcessForest
    class CVProcessData;    // Предварительное объявление класса CVProcessData
    class CVProcessingNode; // Предварительное объявление класса CVProcessingNode

    struct CVNetworkSettings        // Определение структуры сетевых настроек ядра
    {
        QString mac_address = "";   // Определение MAC-адреса ядра
        int tcp_server_port = 0;    // Определение серверного порта TCP для входящих соединений коннекторов
        CVNetworkSettings(QJsonObject& json_object);    // Определение конструктора класса
    };

    class CVNetworkManager : public QObject     // Определение класса CVNetworkManager - наследника QObject
    {
        Q_OBJECT    // Макроопределение спецификации Qt объекта

    public:         // Открытая секция класса
        explicit CVNetworkManager(QObject* parent, CVNetworkSettings& settings, CVProcessManager& pm);  // Определение конструктора класса
        virtual ~CVNetworkManager();    // Объявление виртуального деструктора класса

        QList<QSharedPointer<CVClient>> get_clients();          // Объявление метода получения списка клиентов ядра
        QList<QSharedPointer<CVSupervisor>> get_supervisors();  // Объявление метода получения списка супервайзеров
        void close_all_clients();                               // Объявление метода деактивации всех клиентов ядра

    private:        // Закрытая секция класса
        void setup_tcp_server(CVNetworkSettings& settings);     // Объявление метода запуска TCP сервера
        void add_new_connector(QTcpSocket* socket);             // Объявление метода добавления нового коннектора

        QTcpServer* tcp_server;             // Определение указателя на Qt TCP сервер
        CVInputProcessor* receiver;         // Определение указателя на процессор входящих сообщений
        CVProcessManager& process_manager;  // Определение ссылки на менеджер процессов
        QMap<QTcpSocket*, QSharedPointer<CVClient>> clients;            // Словарь отображений указателя входящего TCP соединения на указатель клиента
        QMap<QTcpSocket*, QSharedPointer<CVSupervisor>> supervisors;    // Словарь отображений указателя входящего TCP соединения на указатель супервайзера
        unsigned connector_id = 0;                                      // Номер последнего коннектора

    signals:        // Секция сигналов Qt
        void all_clients_closed();      // Определение сигнала о закрытии всех клиентских сессий ядра
        void all_supervisors_closed();  // Определение сигнала о закрытии всех сессий супервайзеров

    public slots:   // Секция слотов Qt
        void close_all_supervisors();   // Объявление метода деактивации всех супервайзеров
        void add_tcp_connection();      // Объявление метода добавления нового TCP соединения
        void init_new_conector();       // Объявление метода инициализации нового коннектора
        void delete_connector();        // Объявление метода деактивации коннектора
        void delete_connector(QTcpSocket& sock);    // Объявление метода деактивации коннектора
    };
}

#endif // CVNETWORKMANAGER_H   // Выход из области подключения
