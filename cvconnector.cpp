#include "cvconnector.h"        // Подключение файла cvconnector.h для определения методов классов
#include "cvgraphnode.h"        // Подключение файла cvgraphnode.h для использования классов
#include "cvprocessmanager.h"   // Подключение файла cvprocessmanager.h для использования класса CVProcessManager
#include "cvnetworkmanager.h"   // Подключение файла cvprocessmanager.h для использования класса CVProcessManager
#include "cvjsoncontroller.h"   // Подключение файла cvjsoncontroller.h для использования класса CVJsonController


// Определение конструктора класса CVConnector
// Входящие параметры
//   index - номер коннектора
//   f - фабричный класс коннектора для создания объектов его состояния
//   pm - ссылка на менеджер процессорных узлов
//   nm - ссылка на сетевой менеджер
//   sock - TCP соединение
CVKernel::CVConnector::CVConnector(unsigned index, CVConnectorFactory& f, CVProcessManager& pm, CVNetworkManager& nm, QTcpSocket& sock)
    : QObject(&sock),       // Создаем новый объект иерархии Qt зависимый от объекта сокета (работающий в одном потоке с ним)
      id(index),            // инициализируем индекс
      factory(f),           // инициализируем ссылку на фабрику
      running(false),       // задаем статус неактивности
      tcp_state(sock),      // инициализируем сокет
      network_manager(nm),  // инициализируем ссылку на сетевой менеджер
      process_manager(pm)   // инициализируем ссылку на менеджер процессов
{
    disconnect(&sock, SIGNAL(readyRead()), &network_manager, SLOT(init_new_conector())); // деактивируем предыдущий обработчик сигнала готовности к принятию данных от сокета
    connect(&sock, SIGNAL(readyRead()), this, SLOT(process_incoming_message()));         // устанавливаем метод process_incoming_message в качестве обработчика сигнала готовности к принятию данных от сокета
}

// Определение деструктора класса CVConnector
CVKernel::CVConnector::~CVConnector()
{
    disconnect(&tcp_state, SIGNAL(readyRead()), this, SLOT(process_incoming_message())); // деактивируем предыдущий обработчик сигнала готовности к принятию данных от сокета
    connect(&tcp_state, SIGNAL(readyRead()), &network_manager, SLOT(init_new_conector())); // устанавливаем метод init_new_conector в качестве обработчика сигнала готовности к принятию данных от сокета
}

// Определение метода process_incoming_message класса CVConnector
void CVKernel::CVConnector::process_incoming_message()
{
    if (not receiver.receive_message(tcp_state)) // Если процессор приема данных не принял полный пакет
        return; // То выход из функции

    state->handle_incomming_message(receiver.buffer);  // Вызываем метод обработки входящих данный объекту статуса коннектора, метод производит инициализацию команды для выполнения (объект статуса может меняться во время выполнения программы, Паттерн ООП "Стратегия"))
    if (state->command.get())                        // Если команда определена (объект команды может меняться во время выполнения программы, Паттерн ООП "Стратегия")
        state->command->execute();                   // Выполняем команду

    receiver.clear_buffer();                         // Очищаем буфер процессора приема данных
    state_changed();                                 // Вызываем метод изменения статуса
}

// Определение метода state_changed класса CVConnector
void CVKernel::CVConnector::state_changed()
{
    state.reset(factory.set_state(*this));          // Устанавливаем статус объекта с помощью фабричного метода set_state
    send_buffer(CVJsonController::pack_to_json<CVConnectorState>(state.get()), tcp_state);  // Отправляем данные о смене статуса по TCP
    emit notify_supervisors(*state);                // Генерируем сигнал оповещения супервайзеров о смене статуса коннектора
}

// Определение метода get_ip_address класса CVConnector
// Возвращаемое значение - структура IP-адреса входящего TCP соединения
QHostAddress CVKernel::CVConnector::get_ip_address()
{
    return tcp_state.peerAddress(); // Возвращаем адрес входящего TCP соединения
}

// Определение метода handle_incomming_message класса CVConnectorRun - наследника класса CVConnectorState (Паттерн ООП "Стратегия")
// Входящий параметр buffer - массив байт входящего сообщения
void CVKernel::CVConnectorRun::handle_incomming_message(QByteArray &buffer)
{
    QSharedPointer<CVCommandType> com = CVJsonController::get_from_json_buffer<CVCommandType>(buffer); // Определение типа команды из массива в формате JSON
    if (*com == CVCommandType::stop)    // Если принята команда остановки коннектора
    {
        command.reset(new CVStopCommand(connector)); // То переопределяем объект command типом CVStopCommand
    }
}

// Определение метода handle_incomming_message класса CVConnectorReady - наследника класса CVConnectorState (Паттерн ООП "Стратегия")
// Входящий параметр buffer - массив байт входящего сообщения
void CVKernel::CVConnectorReady::handle_incomming_message(QByteArray &buffer)
{
    QSharedPointer<CVCommandType> com = CVJsonController::get_from_json_buffer<CVCommandType>(buffer); // Определение типа команды из массива в формате JSON
    switch (com->command)       // Проверяем тип команды
    {
        case CVCommandType::run:   // Если принята команда на запуск коннектора
            command.reset(new CVRunCommand(connector)); // То переопределяем объект command типом CVRunCommand
            break;  // Выход из условия
        case CVCommandType::close: // Если принята команда на остановку коннектора
            command.reset(new CVCloseCommand(connector));  // То переопределяем объект command типом CVCloseCommand
            break;  // Выход из условия
        default:    // Если любой другой тип команды
            break;  // То выход из условия
    }
}

// Определение метода execute класса CVRunCommand - наследника класса CVConnectorCommand - (Паттерн ООП "Команда")
void CVKernel::CVRunCommand::execute()
{
    connector.run();    // Вызываем метод run у собственника структуры команды
}

// Определение метода execute класса CVStopCommand - наследника класса CVConnectorCommand - (Паттерн ООП "Команда")
void CVKernel::CVStopCommand::execute()
{
    connector.stop();   // Вызываем метод stop у собственника структуры команды
}

// Определение метода execute класса CVCloseCommand - наследника класса CVConnectorCommand - (Паттерн ООП "Команда")
void CVKernel::CVCloseCommand::execute()
{
    connector.close();  // Вызываем метод close у собственника структуры команды
}

// Определение метода run класса CVConnector - (Паттерн ООП "Команда")
void CVKernel::CVConnector::run()
{
    running = true;     // Переводим статус активности в значение True
    state_changed();    // Вызов метода изменения состояния коннектора
}

// Определение метода stop класса CVConnector - (Паттерн ООП "Команда")
void CVKernel::CVConnector::stop()
{
    running = false;
    state_changed();
}

// Определение метода get_id класса CVConnector
// Возвращаемое значение - номер коннектора в ядре
unsigned CVKernel::CVConnector::get_id()
{
    return id;  // Возвращаем номер коннектора в ядре
}

// Определение метода send_buffer класса CVConnector
// Входящие параметры:
//   byte_arr - массив байт необходимый для отправки в сеть
//   sock - ссылка на соединение, через которое нужно отправить данные
void CVKernel::CVConnector::send_buffer(QByteArray byte_arr, QTcpSocket& sock)
{
    if (not sock.isOpen())  // Если сокет не открыт
        return;             // То выходим из функции

    quint64 arr_size = (quint64) byte_arr.size();           // Инициализация количества байт исходящего массива
    char size[sizeof(quint64)];                             // Создание массива преамбулы из 8 байт для указания размера исходящего массива
    for (uint i = 0; i < sizeof(quint64); ++i) {            // Организация цикла от 0 до 8
        size[i] = (char)((arr_size >> (i * 8)) & 0xff);     // Упаковывание размера исходящего массива в 8-байтовую строку
    }
    QByteArray packet = QByteArray::fromRawData(size, sizeof(quint64)) + byte_arr;  // Инициализация исходящего пакета в виде преамбулы + массива
    quint64 bytes_to_send = packet.size();  // Инициализация размера пакета
    quint64 bytes_sent = 0;                 // Инициализация количества отправленых байт нулем
    while (bytes_sent < bytes_to_send) {    // Организуем цикл до тех пор пока количество отправленых байт не станет равно размеру пакета
        qint64 bytes = sock.write(packet);  // Отправляем пакет через TCP сокет и возвращаем количество отправленых байт
        sock.waitForBytesWritten(-1);       // Ожидаем пока пакет не будет послан в сеть
        packet.mid((int) bytes);            // Отсекаем от пакета часть посланных байт
        bytes_sent += bytes;                // Увеличиваем количество отправленных байт
    }
}

// Определение метода receive_message класса CVInputProcessor
// Входящий параметр tcp_socket - ссылка на сокет из которого будет приниматься сообщение
// Возвращаемое значение - флаг завершения приема входящего пакета
bool CVKernel::CVInputProcessor::receive_message(QTcpSocket& tcp_socket)
{
    if (state == recv_state::GET_SIZE) {    // Если текущее состояние конечного автомата процесса приема сообщения - прием преамбулы
        buffer += tcp_socket.readAll();     // Заполняем буфер прочитанными данными из сокета
        if ((uint) buffer.size() < sizeof(quint64)) // Если размер прочитанных данных меньше размера преамбулы (8 байт)
            return false;       // Выходим из функции со статусом "не завершен прием пакета"
        buffer_size = 0;                                    // Сбрасываем значение размера входящего буфера
        for (uint i = 0; i < sizeof(quint64); ++i) {        // Организуем цикл от 0 до 8
            buffer_size += ((uchar) buffer[i]) << (i * 8);  // Инициализируем значение размера входящего буфера из байтового массива преамбулы
        }
        buffer = buffer.mid(sizeof(quint64));       // Отсекаем преамбулу из принятого буфера
        bytes_received = buffer.size();             // Инициализируем количество принятых байт
        if (bytes_received < buffer_size) {         // Если количество принятых байт меньше значения количества байт пакета из преамбулы
            state = recv_state::GET_BUFFER;         // То устанавливаем состояние конечного автомата в значение приема буфера
        }
    } else if (state == recv_state::GET_BUFFER) {   // Если текущее состояние конечного автомата процесса приема сообщения - прием буфера
        QByteArray bytes_read = tcp_socket.readAll();   // Прочитываем данные из сокета
        bytes_received += bytes_read.size();            // Увеличиваем счетчик принятых байт
        buffer += bytes_read;                           // Добавляем принятые данные в конец сообщения
    }

    return (bytes_received == buffer_size); // Возвращаем true если количество принятых байт равно значению из преамбулы, иначе false
}

// Определение метода clear_buffer класса CVInputProcessor
void CVKernel::CVInputProcessor::clear_buffer()
{
    buffer.clear();     // Очищаем буфер данных
    buffer_size = 0;    // Сбрасываем счетчик количества принятых байт
    state = recv_state::GET_SIZE;   // устанавливаем состояние конечного автомата процесса приема сообщений в значение - прием преамбулы
}
