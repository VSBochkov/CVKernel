#include "cvnetworkmanager.h"   // Подключение файла cvnetworkmanager.h для определения методов класса CVNetworkManager
#include "cvjsoncontroller.h"   // Подключение файла cvjsoncontroller.h для
#include "cvprocessmanager.h"   // Подключение файла cvprocessmanager.h для
#include "cvgraphnode.h"        // Подключение файла cvgraphnode.h для
#include "cvconnector.h"        // Подключение файла cvconnector.h для
#include "implementation/cvclient.h"        // Подключение файла cvclient.h для
#include "implementation/cvsupervisor.h"    // Подключение файла cvsupervisor.h для


// Определение конструктора класса CVNetworkManager
// Входящие параметры:
//   parent - указатель на родительский объект в модели Qt
//   settings - структура сетевых настроек ядра
//   pm - ссылка на объект менеджера процессов
CVKernel::CVNetworkManager::CVNetworkManager(QObject* parent, CVNetworkSettings& settings, CVProcessManager& pm) :
    QObject(parent), process_manager(pm) // Инициализация родительского класса QObject и ссылки на менеджера процесса
{
    receiver = new CVInputProcessor; // Инициализация процессора приема данных
    setup_tcp_server(settings);      // Вызов метода установки TCP сервера
}

// Определение деструктора класса CVNetworkManager
CVKernel::CVNetworkManager::~CVNetworkManager()
{
    delete receiver;                 // Удаление процессора приема данных
}

// Определение метода setup_tcp_server класса CVNetworkManager
// Входящий параметр settings - структура сетевых настроек ядра
void CVKernel::CVNetworkManager::setup_tcp_server(CVNetworkSettings& settings) {
    tcp_server = new QTcpServer(this);  // Создание TCP сервера библиотеки Qt
    tcp_server->listen(QHostAddress::Any, settings.tcp_server_port); // Установка прослушивания TCP соединений на порт указанный в структуре сетевых настроек
    connect(tcp_server, SIGNAL(newConnection()), this, SLOT(add_tcp_connection())); // Установка метода add_tcp_connection как обработчика сигнала о принятии нового соединения от TCP сервера
}

// Определение метода add_tcp_connection класса CVNetworkManager
void CVKernel::CVNetworkManager::add_tcp_connection() {
    QTcpSocket *new_connection = tcp_server->nextPendingConnection();  // Инициализация указателя на новое соединение
    if (not new_connection->isOpen()) {                 // Если новое соединение не открыто
        qDebug() << "New tcp connector is not opened";  // Выдача сообщения об ошибке открытия нового соединения в интерфейс командной строки
        return;                                         // Выход из функции
    }
    if (not new_connection->isValid()) {                // Если новое соединение не установлено
        qDebug() << "New tcp connector is invalid";     // Выдача сообщения об ошибке установления нового соединения в интерфейс командной строки
        return;                                         // Выход из функции
    }
    connect(new_connection, SIGNAL(readyRead()), this, SLOT(init_new_conector())); // Установка метода init_new_connector как обработчика сигнала на готовность к чтению данных от сокета
    connect(new_connection, SIGNAL(disconnected()), this, SLOT(delete_connector())); // Установка метода delete_connector как обработчика сигнала прекращения TCP соединения
}

// Определение метода init_new_connector класса CVNetworkManager
void CVKernel::CVNetworkManager::init_new_conector()
{
    QTcpSocket* connection = (QTcpSocket*) sender(); // Получение адреса сокета
    if (clients.find(connection) != clients.end() or supervisors.find(connection) != supervisors.end()) // Если TCP соединение уже инициализировано как клиент ядра или супервайзер
    {
        return; // То выход из функции
    }

    if (receiver->receive_message(*connection)) // Если процессор приема данных получил полное сообщение
    {
        add_new_connector(connection);  // Вызов метода добавления нового коннектора
        receiver->clear_buffer();       // Очистка входящего буфера процессора приема данных
    }
}

// Определение метода delete_connector класса CVNetworkManager
void CVKernel::CVNetworkManager::delete_connector()
{
    QTcpSocket* connection = (QTcpSocket*) sender();  // Получение адреса сокета
    delete_connector(*connection); // Вызов метода удаления коннектора с передачей ссылки на сокет в качестве аргумента
}

// Определение метода delete_connector класса CVNetworkManager
// Входящий параметр sock - ссылка на объект сокета библиотеки Qt
void CVKernel::CVNetworkManager::delete_connector(QTcpSocket& sock)
{
    auto client_it = clients.find(&sock);           // Поиск клиента ядра по сокету
    auto supervisor_it = supervisors.find(&sock);   // Поиск супервайзера по сокету
    if (client_it != clients.end())                 // Если клиент ядра найден
    {
        client_it.value().clear();                  // То разрушаем объект клиента
        clients.erase(client_it);                   // Удаляем указатель на клиента из списка подключенных клиентов
    }
    else if(supervisor_it != supervisors.end())     // Иначе если найден супервайзер
    {
        supervisor_it.value().clear();              // То разрушаем объект супервайзера
        supervisors.erase(supervisor_it);           // Удаляем указатель на супервайзера из списка подключенных супервайзеров
    }
    sock.close();                                   // закрываем TCP сокет

    if (clients.empty())                            // Если список клиентов пуст
        emit all_clients_closed();                  // То генерируем сигнал о закрытии всех клиентов
}

// Определение метода add_new_connector класса CVNetworkManager
// Входящий параметр socket - новое TCP соединение
void CVKernel::CVNetworkManager::add_new_connector(QTcpSocket* socket)
{
    QSharedPointer<CVConnectorType> connector = CVJsonController::get_from_json_buffer<CVConnectorType>(receiver->buffer); // Инициализируем указатель на тип коннектора с помощью разбора спецификации JSON принятого массива данных
    if (connector->type == CVConnectorType::client) {   // Если типом коннетора является "клиент ядра"
        QSharedPointer<CVClient> new_client(new CVClient(connector_id++, process_manager, *this, *socket)); // Создаем нового клиента ядра
        clients[socket] = new_client;                   // Сохраняем в словарь указатель на клиента по ключу адреса TCP сокета
    } else if (connector->type == CVConnectorType::supervisor) {    // Если типом коннектора является "супервайзер"
        QSharedPointer<CVSupervisor> new_supervisor(new CVSupervisor(connector_id++, process_manager, *this, *socket));  // Создаем нового супервайзера
        supervisors[socket] = new_supervisor;           // Сохраняем в словарь указатель на супервайзера по ключу адреса TCP сокета
    }
}

// Определение метода get_clients класса CVNetworkManager
// Возвращаемое значение - список указателей на "клиентов ядра"
QList<QSharedPointer<CVKernel::CVClient>> CVKernel::CVNetworkManager::get_clients()
{
    return clients.values();     // Возвращаем список значений словаря clients
}

// Определение метода get_supervisors класса CVNetworkManager
// Возвращаемое значение - список указателей на "супервайзеров"
QList<QSharedPointer<CVKernel::CVSupervisor>> CVKernel::CVNetworkManager::get_supervisors()
{
    return supervisors.values(); // Возвращаем список значений словаря supervisors
}

// Определение метода close_all_supervisors класса CVNetworkManager
void CVKernel::CVNetworkManager::close_all_supervisors()
{
    for (auto supervisor_it = supervisors.begin(); supervisor_it != supervisors.end(); supervisor_it++) // Организуем цикл по всем парам "ключ, значение" словаря supervisors
    {
        QTcpSocket* tcp_state = supervisor_it.key();                // Присваивание ключа пары указателю tcp_state
        QSharedPointer<CVSupervisor> supervisor = supervisor_it.value();               // Присвоение адреса на значение пары указателю supervisor
        disconnect(tcp_state, SIGNAL(disconnected()), this, SLOT(delete_connector())); // Деактивируем обработчик сигнала TCP разрыва соединения
        CVConnectorState* state = supervisor->factory.set_state(*supervisor);          // Инициализируем состояние коннектора с помощью фабричного метода в который передается ссылка на супервайзера
        if (state->value != CVConnectorState::closed)                                  // Если текущее состояние не является значением "Не активен"
        {
            supervisor->close();                                                       // То завершаем текущую сессию супервайзера
        }
        tcp_state->close();                                                            // Закрываем TCP соединение супервайзера
        supervisor.clear();                                                            // Удаление объекта супервайзера
    }
    supervisors.clear();                                            // Очистка словаря супервайзеров
    emit all_supervisors_closed();                                  // Генерация сигнала all_supervisors_closed
}

// Определение метода close_all_clients класса CVNetworkManager
void CVKernel::CVNetworkManager::close_all_clients()
{
    for (auto client_it = clients.begin(); client_it != clients.end(); client_it++) // Организуем цикл по всем парам "ключ, значение" словаря clients
    {
        QTcpSocket* tcp_state = client_it.key();        // Присваивание ключа пары указателю tcp_state
        CVClient* client = client_it.value().data();    // Присвоение адреса на значение пары указателю client
        disconnect(tcp_state, SIGNAL(disconnected()), this, SLOT(delete_connector())); // Деактивируем обработчик сигнала TCP разрыва соединения
        connect(client, SIGNAL(client_disabled(QTcpSocket&)), this, SLOT(delete_connector(QTcpSocket&))); // Устанавливаем метод delete_connector в качестве обработчика сигнала client_disabled от клиента ядра
        CVConnectorState* state = client->factory.set_state(*client);   // Инициализируем состояние коннектора с помощью фабричного метода в который передается ссылка на клиента
        if (state->value != CVConnectorState::closed)   // Если текущее состояние не является значением "Не активен"
        {
            client->close();                            // То завершаем текущую сессию клиента
        }
    }
}
