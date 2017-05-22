#ifndef CVSUPERVISOR_H
#define CVSUPERVISOR_H

#include <QJsonObject>      // Подключение файла QJsonObject для использования одноименного класса
#include <QByteArray>       // Подключение файла QByteArray для использования одноименного класса
#include <QTcpSocket>       // Подключение файла QTcpSocket для использования одноименного класса
#include <QUdpSocket>       // Подключение файла QUdpSocket для использования одноименного класса
#include <QTimer>           // Подключение файла QTimer для использования одноименного класса
#include <QMap>             // Подключение файла QMap для использования одноименного класса
#include "cvconnector.h"    // Подключение файла cvconnector.h для использования базовых классов

namespace CVKernel      // Определение области видимости
{
    struct CVProcessForest;     // Предварительное объявление структуры CVProcessForest
    struct CVProcessManager;    // Предварительное объявление структуры CVProcessManager
    struct CVNetworkManager;    // Предварительное объявление структуры CVNetworkManager
    struct CVClient;            // Предварительное объявление структуры CVClient
    struct CVIONode;            // Предварительное объявление структуры CVIONode

    struct CVSupervisorFactory : CVConnectorFactory  // Определение конкретной фабрики создания состояний супервайзера - (Паттерн ООП "Абстрактная фабрика")
    {
        CVSupervisorFactory(const CVSupervisorFactory&) = delete;               // Предотвращение возможности
        CVSupervisorFactory& operator==(const CVSupervisorFactory&) = delete;   // копирования объекта
        virtual ~CVSupervisorFactory() = default;       // Определение виртуального деструктора

        virtual CVConnectorState* set_state(CVConnector& connector) override;   // Объявление виртуального метода создания объектов состояния супервайзера
        static CVSupervisorFactory* instance()      // Определение метода взятия единственного объекта класса фабрики (Паттерн ООП "Одиночка")
        {
            static CVSupervisorFactory inst;        // Создание статического объекта фабрики
            return &inst;                           // Возврат указателя на объект
        }

    private:  // Закрытая секция
        CVSupervisorFactory() {}    // Определение конструктора в закрытой секции класса для предотвращения конструирования объекта извне
    };

    struct CVSupervisionInfo    // Определение структуры информации о статусе ядра
    {
        int connected_clients;  // Количество подключенных клиентов
        int used_process_nodes; // Количество используемых процессорных узлов
        QMap<QString, double> average_timings;  // Словарь среднего времени работы узлов

        CVSupervisionInfo(): connected_clients(0), used_process_nodes(0) {} // Определение конструктора
        CVSupervisionInfo(int clients, QMap<QString, double> average_timings); // Объявление конструктора
        QJsonObject pack_to_json(); // Объявление метода сериализации объекта в формат JSON
    };

    struct CVSupervisionSettings    // Определение структуры вхоящего пакета настроек супервайзера
    {
        CVSupervisionSettings(QJsonObject& json_obj);   // Конструктор структуры
        int port;                   // TCP порт передачи информации
        int update_timer_value;     // Период времени передачи информации в секундах
    };

    class CVSupervisor : public CVConnector {   // Определение структуры супервайзера
    Q_OBJECT    // Макроопределение спецификации Qt класса
    public:     // Открытая секция класса
        explicit CVSupervisor(unsigned index, CVProcessManager& process_manager, CVNetworkManager& network_manager, QTcpSocket& sock);  // Объявление конструктора
        virtual ~CVSupervisor();    // Объявление виртуального деструктора

        void init_supervision(QSharedPointer<CVSupervisionSettings> settings);  // Объявление метода инициализации настроек супервайзера
        virtual void run() override;    // Объявление виртуального метода выполнения состояния "На исполнении" (Паттерн ООП "Стратегия")
        virtual void stop() override;   // Объявление виртуального метода выполнения состояния "Остановлен"    (Паттерн ООП "Стратегия")
        virtual void close() override;  // Объявление виртуального метода выполнения состояния "Деактивирован" (Паттерн ООП "Стратегия")

        QTimer* updateSupInfoTimer;     // Таймер передачи информации о статусе ядра
        QTcpSocket* tcp_supervision;    // TCP соединение передачи информации
        int supervision_port;           // TCP порт передачи информации
        int update_timer_value;         // Период времени передачи информации в секундах

    private slots:  // Секция слотов Qt
        void send_supervision_info();   // Объявление метода отправки информацию о состоянии ядра супервайзеру
        void send_connector_state_change(CVConnectorState&);    // Объявление метода отсылки изменения состояния коннектора супервайзеру
    };

    struct CVSupervisorClosed : CVConnectorClosed { // Определение структуры состояния "Деактивирован" супервайзера - (Паттерн ООП "Стратегия")
        CVSupervisorClosed(CVSupervisor& sup): CVConnectorClosed(sup) {} // Определение конструктора структуры
        virtual void handle_incomming_message(QByteArray&) override;    // Объявление метода обработки входящего сообщения
    };

    struct CVInitSupervisionCommand : public CVConnectorCommand // Определение структуры команды инициализации настроек супервайзера - (Паттерн ООП "Команда")
    {
        CVInitSupervisionCommand(CVSupervisor& sup, QSharedPointer<CVSupervisionSettings> params) : supervisor(sup), settings(params) {} // Определение конструктора класса
        virtual void execute() override;    // Определение виртуального метода выполнения команды
        CVSupervisor& supervisor;           // Ссылка на супервайзера
        QSharedPointer<CVSupervisionSettings> settings; // Структура настроек супервайзера
    };

    struct CVSupervisorStartup {    // Определение структуры синхронизации супервайзера на старте
        QList<QSharedPointer<CVClient>> client_list;        // Список клиентов ядра
        CVSupervisorStartup(QList<QSharedPointer<CVClient>> clients) : client_list(clients) {}  // Определение конструктора класса
        QJsonObject pack_to_json(); // Объявление метода сериализации объекта в формате JSON
    };
}
#endif // CVSUPERVISOR_H   // Выход из области подключения
