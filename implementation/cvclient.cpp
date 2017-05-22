#include "cvclient.h"           // Подключаем файл cvclient.h для определения методов классов
#include "cvsupervisor.h"       // Подключаем файл cvsupervisor.h для использования класса CVSupervisor
#include "cvconnector.h"        // Подключаем файл cvconnector.h для использования методов классов-родителей
#include "cvnetworkmanager.h"   // Подключаем файл cvnetworkmanager.h для использования класса CVNetworkManager
#include "cvprocessmanager.h"   // Подключаем файл cvprocessmanager.h для использования класса CVProcessManager
#include "cvgraphnode.h"        // Подключаем файл cvgraphnode.h для использования класса CVIONode
#include "cvjsoncontroller.h"   // Подключаем файл cvjsoncontroller.h для использования класса CVJsonController

// Определение метода set_state класса CVClientFactory
// Входящий параметр connector - ссылка на коннектор
// Возвращаемое значение - объект состояния коннектора унаследованный от класса CVConnectorState
CVKernel::CVConnectorState* CVKernel::CVClientFactory::set_state(CVConnector& connector)
{
    CVClient& client = static_cast<CVClient&>(connector);   // приводим ссылку на коннектора к ссылке на клиента ядра
    if (client.video_io == nullptr)                         // Если узел ввода-вывода не инициализирован
    {
        return new CVClientClosed(client);                  // То возвращаем объект состояния "не активен"
    }
    else if (client.running)                                // Если клиент активирован
    {
        return new CVConnectorRun(connector);               // То возвращаем объект состояния "в процессе"
    }
    else                                                    // Иначе
    {
        return new CVConnectorReady(connector);             // Возвращаем объект состояния "остановлен"
    }
}

// Определение конструктора класса CVClient
// Входящие параметры
//   index - номер коннектора
//   pm - ссылка на менеджер процессорных узлов
//   nm - ссылка на сетевой менеджер
//   sock - TCP соединение
CVKernel::CVClient::CVClient(unsigned index, CVProcessManager& pm, CVNetworkManager& nm, QTcpSocket& sock)
    : CVConnector(index, *CVClientFactory::instance(), pm, nm, sock)    // Инициализируем объект коннектора передавая входящие параметры и ссылку на конкретную фабрику клиента ядра
{
    video_io = nullptr; // Сбрасываем указатель на узел ввода-вывода
    meta_udp_client.reset(new QUdpSocket);  // Инициализируем UDP сокет передачи метаданных
    for (QSharedPointer<CVSupervisor> sup : network_manager.get_supervisors()) // Организуем цикл по всем супервайзерам ядра
    {
        connect(this, SIGNAL(notify_supervisors(CVConnectorState&)), sup.data(), SLOT(send_connector_state_change(CVConnectorState&))); // Устанавливаем метод send_connector_state_change в качестве обработчика сигнала notify_supervisors от клиента ядра
    }

    state_changed();    // Вызываем метод изменения состояния коннектора
    qDebug() << "added CVClient id:" << id << "ip_address:" << sock.peerAddress();  // Выводим в интерфейс командной строки сообщение о добавлении нового клиента ядра
}

// Определение деструктора класса CVClient
CVKernel::CVClient::~CVClient()
{
    for (QSharedPointer<CVSupervisor> sup : network_manager.get_supervisors()) // Организуем цикл по всем супервайзерам ядра
    {
        disconnect(this, SIGNAL(notify_supervisors(CVConnectorState&)), sup.data(), SLOT(send_connector_state_change(CVConnectorState&))); // Деактивируем оповещение супервайзеров
    }
    qDebug() << "deleted CVClient id: " << id;  // Выводим в интерфейс командной строки сообщение об удалении клиента ядра
}

// Определение метода get_overlay_path класса CVClient
// Возвращаемое значение - путь до видео с отображением результатов анализа
QString CVKernel::CVClient::get_overlay_path()
{
    return video_io->get_overlay_path();    // Возврат пути до видео.
}

// Определение метода get_proc_nodes_number класса CVClient
// Возвращаемое значение - количество пррцессорных узлов ядра задействованных клиентом
int CVKernel::CVClient::get_proc_nodes_number()
{
    return video_io->nodes_number;  // Возврат значения nodes_number
}

// Определение метода close класса CVClient - (Паттерн ООП "Команда")
void CVKernel::CVClient::close()
{
    video_io->close();  // Закрытие узла ввода-вывода
}

// Определение метода run класса CVClient - (Паттерн ООП "Команда")
void CVKernel::CVClient::run()
{
    video_io->start();  // Активация работы узла ввода-вывода
    CVConnector::run(); // Вызов одноименного метода базового класса
}

// Определение метода stop класса CVClient - (Паттерн ООП "Команда")
void CVKernel::CVClient::stop()
{
    video_io->stop();    // Остановка работы узла ввода-вывода
    CVConnector::stop(); // Вызов одноименного метода базового класса
}

// Определение метода init_process_forest класса CVClient
// Входящий параметр proc_forest - указатель на спецификацию леса видеоаналитики
void CVKernel::CVClient::init_process_forest(QSharedPointer<CVProcessForest> proc_forest)
{
    video_io = process_manager.add_new_stream(proc_forest); // Вызов метода инициализации новозо узла ввода-вывода
    udp_address = get_ip_address();                         // Взятие адреса клиента для передачи метаданных по UDP
    udp_port = proc_forest->meta_udp_port;                  // Присвоение порта передачи метаданных по UDP
    connect(video_io, SIGNAL(node_closed()), this, SLOT(on_stream_closed()));   // Установка метода on_stream_closed как обработчика сигнала node_closed от узла ввода-вывода
    connect(video_io, SIGNAL(send_metadata(QByteArray)), this, SLOT(send_datagramm(QByteArray))); // Установка метода send_datagram как обработчика сигнала send_metadata от узла ввода-вывода
    connect(video_io, SIGNAL(close_udp()), this, SLOT(do_close_udp())); // Установка метода do_close_udp как обработчика сигнала close_udp от узла ввода-вывода
}

// Определение метода on_stream_closed класса CVClient
void CVKernel::CVClient::on_stream_closed()
{
    if (running == true)    // Если клиент активирован
    {
        running = false;    // То деактивируем клиента
        state_changed();    // Вызываем метод изменения состояния клиента
    }

    video_io = nullptr;     // Сбрасываем указатель на узел ввода-вывода
    state_changed();        // Вызываем метод изменения состояния клиента
    emit client_disabled(tcp_state); // Генерируем сигнал неактивности клиента
}

// Определение метода send_datagram класса CVClient
// Входящий параметр byte_arr - массив байт для отправки по UDP
void CVKernel::CVClient::send_datagramm(QByteArray byte_arr)
{
    if (byte_arr.isEmpty()) {   // Если массив пустой
        return;                 // То выходим из функции
    }
    quint64 arr_size = (quint64) byte_arr.size();   // Вычисляем размер массива
    char size[sizeof(quint64)];                     // Создаем массив преамбулы из 8 байт для указания размера датаграммы
    for (uint i = 0; i < sizeof(quint64); ++i) {    // Организуем цикл от 0 до 8
        size[i] = (char)((arr_size >> (i * 8)) & 0xff); // Инициализируем массив преамбулы размером датаграммы
    }
    meta_udp_client->writeDatagram(QByteArray::fromRawData(size, sizeof(quint64)), udp_address, udp_port); // Передаем преамбулу по UDP
    quint64 bytes_to_send = byte_arr.size(), bytes_sent = 0; // Инициализируем количество байт для передачи и переданных байт датаграммы
    while (bytes_sent < bytes_to_send) {            // Организуем цикл пока количество переданных байт не сравняется с размером датаграммы
        meta_udp_client->waitForBytesWritten(-1);   // Ожидаем пока не запишется предыдущая порция данных
        qint64 bytes = meta_udp_client->writeDatagram(byte_arr, udp_address, udp_port); // Отправить новую порцию данных
        byte_arr.mid((int) bytes);      // Урезаем массив датаграммы на величину переданных байт
        bytes_sent += bytes;            // Увеличиваем счетчик переданных байт
    }
}

// Определение метода do_close_udp класса CVClient
void CVKernel::CVClient::do_close_udp()
{
    send_datagramm(QByteArray("{}"));   // Отправка датаграммы пустой спецификации JSON
    video_io->udp_closed();             // Вызов метода udp_closed у узла ввода-вывода
}

// Определение метода handle_incomming_message класса CVClientClosed (паттерн ООП "Стратегия")
// Входящий параметр buffer - входящий буфер данных
void CVKernel::CVClientClosed::handle_incomming_message(QByteArray &buffer)
{
    QSharedPointer<CVProcessForest> proc_forest = CVJsonController::get_from_json_buffer<CVProcessForest>(buffer); // Разбор спецификации видеоаналитики в формате JSON
    if (not proc_forest->proc_forest.empty())                   // Если принята спецификация видеоаналитики
    {
        command.reset(new CVInitProcTreeCommand(static_cast<CVClient&>(connector), proc_forest));   // То устанавливаем команду в значение "установка параметров видеоаналитики"
    }
}

// Определение метода execute класса CVInitProcessTreeComand (паттерн ООП "Стратегия")
void CVKernel::CVInitProcTreeCommand::execute()
{
    client.init_process_forest(proc_forest);  // Вызов метода инициализации параметров видеоаналитики у клиента ядра
}
