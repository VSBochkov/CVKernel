#include "cvapplication.h"      // Подключение файла cvapplication.h для определения методов класса CVApplication
#include "cvjsoncontroller.h"   // Подключение файла cvjsoncontroller.h для использования класса CVJsonController
#include <QThread>              // Подключение файла QThread для использоания одноименного класса
#include <QNetworkInterface>    // Подключение файла QNetworkInterface для использования одноименного класса
#include <array>                // Подключение файла array для использования структуры std::array
#include <errno.h>              // Подключение файла errno.h для использования переменной номера ошибки errno
#include <signal.h>             // Подключение файла signal.h для инициализации обработчиков UNIX сигналов с помощью метода signal()
#include <unistd.h>             // Подключение файла unistd.h для использования методов ::read() и ::write()
#include <sys/socket.h>         // Подключение файла socket.h для использования метода ::socketpair, значений AF_UNIX, SOCK_STREAM

static int sig_usr1_fd[2];      // Определение статической переменной массива файловых дескрипторов для сигнала USR1
static int sig_term_fd[2];      // Определение статической переменной массива файловых дескрипторов для сигнала TERM

QString CVKernel::get_ip_address()  // Определение функции get_ip_address для возврата строки IP-адреса ПК на котором запущено ядро видеоаналитики
{
    QString ip_address;                                                         // Определение пустой строки ip_address
    foreach (const QHostAddress &address, QNetworkInterface::allAddresses())    // Организация цикла по массиву структур QHostAddress полученного в результате вызова метода QNetworkInterface::allAddresses()
    {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && address != QHostAddress(QHostAddress::LocalHost)) // Если текущий элемент является IPv4 адресом и не является адресом локального хоста, т.е. 127.0.0.1
        {
            ip_address = address.toString(); // То инициализируем строку ip_address текущим элементом массива преобразованным в тип строки
            break;                           // Выход из цикла
        }
    }
    return ip_address;  // Возврат значения переменной ip_address
}

// Определение конструктора класса CVApplication
// Входящий параметр - app_settings_json - путь до файла сетевых настроек ядра видеоаналитики
CVKernel::CVApplication::CVApplication(QString app_settings_json)
    : QObject(NULL) // Инициализация класса родителя QObject с передачей нулевого указателя в качестве параметра,
                    // для указания что у объекта нет родителя в рамках объектной модели Qt
{
    net_settings = CVJsonController::get_from_json_file<CVNetworkSettings>(app_settings_json); // Инициализация сетевых настроек с помощью разбора файла JSON
    network_manager = new CVNetworkManager(this, *net_settings, process_manager);              // Создание объекта класса CVNetworkManager
    process_manager.set_network_manager(network_manager);                                      // Передача ссылки на сетевой менеджер для использования его из класса менеджера процессов

    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sig_usr1_fd)) { // Инициализация UNIX сокета файлового дескриптора sig_usr1_fd
       qFatal("Couldn't create USR1 socketpair");             // Если произошла ошибка при инициализации сокета то высветить сообщение об ошибке
    }
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sig_term_fd)) { // Инициализация UNIX сокета файлового дескриптора sig_term_fd
       qFatal("Couldn't create TERM socketpair");             // Если произошла ошибка при инициализации сокета то высветить сообщение об ошибке
    }

    sn_usr1 = new QSocketNotifier(sig_usr1_fd[1], QSocketNotifier::Read, this); // Создание экземпляра QSocketNotifier для отслеживания файлового дескриптора для чтения sig_usr1_fd[1]
    connect(sn_usr1, SIGNAL(activated(int)), this, SLOT(shutdown()));           // Установка метода shutdown() в качестве обработчика сигнала activated() от объекта sn_usr1
    sn_term = new QSocketNotifier(sig_term_fd[1], QSocketNotifier::Read, this); // Создание экземпляра QSocketNotifier для отслеживания файлового дескриптора для чтения sig_usr1_fd[1]
    connect(sn_term, SIGNAL(activated(int)), this, SLOT(reboot()));             // Установка метода reboot() в качестве обработчика сигнала activated() от объекта sn_term
}

// Определение функции setup_unix_signal_handlers() класса CVApplication
// Возвращаемое значение:
//      0 - установка обработчиков успешно завершено
//      1 - ошибка в установке обработчика для сигнала USR1
//      2 - ошибка в установке обработчика для сигнала TERM
int CVKernel::CVApplication::setup_unix_signal_handlers()
{
    if (signal(SIGUSR1, CVKernel::CVApplication::usr1_handler) == SIG_ERR) // Вызов метода signal для установки обработчика сигнала USR1
        return 1;   // Если произошла ошибка то возвращаем 1
    if (signal(SIGTERM, CVKernel::CVApplication::term_handler) == SIG_ERR) // Вызов метода signal для установки обработчика сигнала TERM
        return 2;   // Если произошла ошибка то возвращаем 2
    return 0; // Установка обработчиков сигнала успешно завершена - возвращаем 0
}

// Определение статической функции usr1_handler(int) класса CVApplication
void CVKernel::CVApplication::usr1_handler(int)
{
    qDebug() << "SIGUSR1 handler";                      // Вывод "SIGUSR1 handler" в интерфейс командной строки
    char a = 1;                                         // Инициализация байта для записи в файловый дескриптор sig_usr1_fd[0]
    if (::write(sig_usr1_fd[0], &a, sizeof(a)) < 0)     // Запись байта в файловый дескриптор sig_usr1_fd[0]
    {
        qDebug() << "SIGUSR1 handler error: " << errno; // Вывод сообщения об ошибке если запись байта прошла неудачно
    }
}

// Определение статической функции term_handler(int) класса CVApplication
void CVKernel::CVApplication::term_handler(int)
{
    qDebug() << "SIGTERM handler";                      // Вывод "SIGTERM handler" в интерфейс командной строки
    char a = 1;                                         // Инициализация байта для записи в файловый дескриптор sig_term_fd[0]
    if (::write(sig_term_fd[0], &a, sizeof(a)) < 0)     // Запись байта в файловый дескриптор sig_term_fd[0]
    {
        qDebug() << "SIGTERM handler error: " << errno; // Вывод сообщения об ошибке если запись байта прошла неудачно
    }
}

// Определение функции reboot() класса CVApplication
void CVKernel::CVApplication::reboot()
{
    sn_term->setEnabled(false);     // временное отключение sn_term, для предотвращения повторного вызова до завершения обработчика
    char tmp;                       // определение байта данных
    ::read(sig_term_fd[1], &tmp, sizeof(tmp));  // прочитывание байта данных из файлового дескриптора sig_term_fd[1]

    if (not network_manager->get_clients().empty()) // Если список клиентов ядра не является пустым
    {
        connect(network_manager, SIGNAL(all_clients_closed()), network_manager, SLOT(close_all_supervisors())); // То устанавливаем метод close_all_supervisors() как обработчик для сигнала all_clients_closed() объекта network_manager
        network_manager->close_all_clients();     // Вызов метода close_all_clients() объекта network_manager
    }
    else
    {
        network_manager->close_all_supervisors(); // Иначе вызываем метод close_all_supervisors() объекта network_manager
    }

    sn_term->setEnabled(true);     // включение UNIX socket оповестителя для возможности повторной обработки сигнала TERM
}

void CVKernel::CVApplication::shutdown()
{
    sn_usr1->setEnabled(false);     // временное отключение sn_usr1, для предотвращения повторного вызова до завершения обработчика
    char tmp;                       // определение байта данных
    ::read(sig_usr1_fd[1], &tmp, sizeof(tmp));  // прочитывание байта данных из файлового дескриптора sig_usr1_fd[1]

    for (CVProcessingNode* node: process_manager.processing_nodes)  // Организация цикла по множеству процессорных узлов менеджера процессов
    {
        if (not network_manager->get_clients().empty()) // Если список клиентов ядра не является пустым
        {
            disconnect(network_manager, SIGNAL(all_clients_closed()), node, SLOT(reset_average_time())); // То деактивируем ранее установленный обработчик сигнала all_clients_closed от объекта network manager
            connect(network_manager, SIGNAL(all_clients_closed()), network_manager, SLOT(close_all_supervisors())); // Уставливаем метод close_all_supervisors объекта network_manager в качестве нового обработчика сигнала all_clients_closed
        }
        connect(network_manager, SIGNAL(all_supervisors_closed()), node->thread(), SLOT(quit())); // Устанавливаем метод завершения потока процессорного узла в качестве обработчика сигнала all_supervisors_closed от объекта сетевого менеджера
    }

    connect(network_manager, SIGNAL(all_supervisors_closed()), this, SLOT(deleteLater())); // Устанавливаем метод удаления объекта класса CVApplication как обработчика сигнала all_supervisors_closed от сетевого менеджера
    if (network_manager->get_clients().empty())   // Если список клиентов ядра пуст
        network_manager->close_all_supervisors(); // То вызываем метод close_all_supervisors() сетевого менеджера
    else
        network_manager->close_all_clients(); // Иначе вызываем метод close_all_clients сетевого менеджера

    sn_usr1->setEnabled(true);     // включение UNIX socket оповестителя для возможности повторной обработки сигнала USR1
}
