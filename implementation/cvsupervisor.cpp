#include "cvsupervisor.h"       // Подключаем файл cvsupervisor.h для определения методов классов
#include "cvclient.h"           // Подключаем файл cvclient.h для использования класса CVClient
#include "cvconnector.h"        // Подключаем файл cvconnector.h для использования методов классов-родителей
#include "cvjsoncontroller.h"   // Подключаем файл cvjsoncontroller.h для использования класса CVJsonController
#include "cvnetworkmanager.h"   // Подключаем файл cvnetworkmanager.h для использования класса CVNetworkManager
#include "cvprocessmanager.h"   // Подключаем файл cvprocessmanager.h для использования класса CVProcessManager


// Определение метода set_state класса CVSupervisorFactory
// Входящий параметр connector - ссылка на коннектор
// Возвращаемое значение - объект состояния коннектора унаследованный от класса CVConnectorState
CVKernel::CVConnectorState* CVKernel::CVSupervisorFactory::set_state(CVConnector& connector)
{
    CVSupervisor& sup = static_cast<CVSupervisor&>(connector);  // приводим ссылку на коннектора к ссылке на супервайзера
    if (sup.supervision_port == 0)              // Если TCP порт передачи информации не инициализирован
    {
        return new CVSupervisorClosed(sup);     // То возвращаем объект состояния "не активен"
    }
    else if (sup.running)                       // Если супервайзер активирован
    {
        return new CVConnectorRun(sup);         // То возвращаем объект состояния "в процессе"
    }
    else
    {
        return new CVConnectorReady(sup);       // Возвращаем объект состояния "остановлен"
    }
}

// Определение конструктора класса CVSupervisor
// Входящие параметры
//   index - номер коннектора
//   pm - ссылка на менеджер процессорных узлов
//   nm - ссылка на сетевой менеджер
//   sock - TCP соединение
CVKernel::CVSupervisor::CVSupervisor(unsigned int id, CVProcessManager& pm, CVNetworkManager& nm, QTcpSocket& sock)
    : CVConnector(id, *CVSupervisorFactory::instance(), pm, nm, sock),  // Инициализируем объект коннектора передавая входящие параметры и ссылку на конкретную фабрику супервайзера
      supervision_port(0)   // Установка порта передачи информации в значение 0
{
    state_changed();    // Вызов метода изменения состояния супервайзера
    updateSupInfoTimer = new QTimer(&sock);     // Инициализация таймера в потоке родительского объекта sock
    tcp_supervision = new QTcpSocket(&sock);    // Инициализация TCP сокета для передачи информации в потоке родительского объекта sock
    connect(updateSupInfoTimer, SIGNAL(timeout()), this, SLOT(send_supervision_info()));   // Устанавливаем метод send_supervision_info как обработчик сигнала истечения времени таймера
    for (QSharedPointer<CVClient> client : network_manager.get_clients())   // Организуем цикл по всем клиентам ядра
    {
        connect(client.data(), SIGNAL(notify_supervisors(CVConnectorState&)), this, SLOT(send_connector_state_change(CVConnectorState&))); // Устанавливаем метод send_connector_state_change как обработчик сигнала оповещения о смене статуса клиента
    }
    qDebug() << "added CVSupervisor id:" << id << "ip_address:" << sock.peerAddress();  // Вывод информации о добавлении нового супервайзера в интерфейс командной строки
}

// Определение деструктора класса CVSupervisor
CVKernel::CVSupervisor::~CVSupervisor()
{
    for (QSharedPointer<CVClient> client : network_manager.get_clients())   // Организуем цикл по всем клиентам ядра
    {
        disconnect(client.data(), SIGNAL(notify_supervisors(CVConnectorState&)), this, SLOT(send_connector_state_change(CVConnectorState&))); // Деактивируем обработчик сигнала оповещения о смене статуса клиента
    }
    qDebug() << "deleted CVSupervisor id: " << id;  // Вывод информации об удалении супервайзера в интерфейс командной строки
}

// Определение метода init_supervision класса CVSupervisor
// Входящий параметр settings - настройки режима супервайзера
void CVKernel::CVSupervisor::init_supervision(QSharedPointer<CVSupervisionSettings> settings)
{
    supervision_port = settings->port;      // Инициализация TCP порта передачи информации
    update_timer_value = settings->update_timer_value;  // Инициализация периода времени посылки информации в секундах
    tcp_supervision->connectToHost(tcp_state.peerAddress(), supervision_port);  // Присоединение к TCP серверу приема информации
    tcp_supervision->waitForConnected(-1);              // Ожидание установление соединения
    send_buffer(                                                    // Отправка буфера
        CVJsonController::pack_to_json<CVSupervisorStartup>(  // Синхронизации статуса ядра
            CVSupervisorStartup(network_manager.get_clients())      // В формате JSON
        ),
        *tcp_supervision                                            // По сокету передачи информации
    );
}

// Определение метода run класса CVSupervisor - (Паттерн ООП "Команда")
void CVKernel::CVSupervisor::run()
{
    updateSupInfoTimer->start(update_timer_value * 1000);   // Заводим таймер по истечении которого передаем информацию о работе процессорных модулей
    CVConnector::run();     // Вызов одноименного метода из класса-родителя
}

// Определение метода stop класса CVSupervisor - (Паттерн ООП "Команда")
void CVKernel::CVSupervisor::stop()
{
    updateSupInfoTimer->stop(); // Останавливаем таймер передачи информации о работе процессорных модулей
    CVConnector::stop();        // Вызов одноименного метода из класса-родителя
}

// Определение метода close класса CVSupervisor - (Паттерн ООП "Команда")
void CVKernel::CVSupervisor::close()
{
    if (running)    // Если супервайзер запущен
    {
        stop();     // То останавливаем его
    }
    send_buffer(QByteArray("{}"), *tcp_supervision);    // Отправка буфера пустой спецификации JSON через сокет передачи информации
    tcp_supervision->close();                           // Закрываем сокет передачи информации
    supervision_port = 0;                               // Сбрасываем значение порта
    state_changed();                                    // Вызов метода изменения состояния супервайзера
}

// Определение метода handle_incomming_message класса CVSupervisorClosed (паттерн ООП "Стратегия")
// Входящий параметр buffer - массив байт входящего сообщения
void CVKernel::CVSupervisorClosed::handle_incomming_message(QByteArray &buffer)
{
    QSharedPointer<CVSupervisionSettings> sup_settings = CVJsonController::get_from_json_buffer<CVSupervisionSettings>(buffer); // Разбор настроек процесса супервайзера в формате JSON
    if (sup_settings->port != 0)    // Если настройки корректные
    {
        command.reset(new CVInitSupervisionCommand(static_cast<CVSupervisor&>(connector), sup_settings));   // То устанавливаем объект команды типом инициализации параметров супервизора
    }
}

// Определение метода execute класса CVInitSupervisionCommand (паттерн ООП "Стратегия")
void CVKernel::CVInitSupervisionCommand::execute()
{
    supervisor.init_supervision(settings);  // Вызов функции инициализации параметров супервизора для объекта собственника
}

// Определение метода send_supervision_info класса CVSupervisor
void CVKernel::CVSupervisor::send_supervision_info()
{
    send_buffer(                                                    // Отправка
        CVJsonController::pack_to_json<CVSupervisionInfo>(          // буффера информации
            CVSupervisionInfo(                                      // о состоянии ядра
                network_manager.get_clients().size(),               // и процессорных
                process_manager.get_average_timings()               // модулей
            )
        ),
        *tcp_supervision                                            // через сокет супервайзера
    );
}

// Определение метода send_connector_state_change класса CVSupervisor
// Входящий параметр state - ссылка на объект состояния коннектора
void CVKernel::CVSupervisor::send_connector_state_change(CVConnectorState& state)
{
    send_buffer(CVJsonController::pack_to_json<CVConnectorState>(&state), *tcp_supervision);  // Отправка буфера состояния коннектора в формате JSON
}

// Определение конструктора класса CVSupervisionInfo
// Входящие параметры:
//   clients - количество подключенных клиентов
//   timings - словарь значений среднего времени выполнения работы всех процессорных узлов
CVKernel::CVSupervisionInfo::CVSupervisionInfo(int clients, QMap<QString, double> timings)
    : connected_clients(clients)    // Присваивание количества подключенных клиентов
{
    used_process_nodes = timings.size();    // Вычисление количества процессорных узлов
    average_timings = timings;              // Присвоение словаря среднего времени работы узлов
}

