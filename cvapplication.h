#ifndef CVAPPLICATION_H
#define CVAPPLICATION_H

#include <QObject>                  // Подключение файла QObject для использования одноименного класса
#include <QJsonObject>              // Подключение файла QJsonObject для использования одноименного класса
#include <QSocketNotifier>          // Подключение файла QSocketNotifier для использования одноименного класса
#include "cvgraphnode.h"            // Подключение файла cvgraphnode.h для использования классов
#include "cvnetworkmanager.h"       // Подключение файла cvnetworkmanager.h для использования класса CVNetworkManager
#include "cvprocessmanager.h"       // Подключение файла cvprocessmanager.h для использования класса CVProcessManager
#include "cvjsoncontroller.h"       // Подключение файла cvjsoncontroller.h для использования класса CVJsonController
#include "cvfactorycontroller.h"    // Подключение файла cvfactorycontroller.h для использования класса CVFactoryController

namespace CVKernel    // Определение области видимости
{
    class CVApplication : public QObject    // Определение класса CVApplication - наследника класса QObject
    {
        Q_OBJECT        // Макроопределение спецификации класса объектной модели Qt
    public:             // Открытая секция класса
        explicit CVApplication(QString app_settings_json);  // Объявление конструктора класса
        virtual ~CVApplication() {}                         // Определение виртуального деструктора класса

    // Unix signal handlers.
        static int setup_unix_signal_handlers();    // Подключение обработчиков Unix сигналов
        static void usr1_handler(int unused);       // Обработчик сигнала SIGUSR1
        static void term_handler(int unused);       // Обработчик сигнала SIGTERM

    public slots:           // Секция Qt слотов
        void shutdown();    // Завершение работы приложения
        void reboot();      // Отсоединение всех клиентов и супервайзеров

    private:            // Закрытая секция класса
        CVNetworkManager* network_manager;  // Определение указателя на сетевой менеджер
        CVProcessManager process_manager;   // Определение объекта менеджера процессов
        QSharedPointer<CVNetworkSettings> net_settings; // Определение указателя на сетевые настройки ядра
        QSocketNotifier *sn_usr1;           // Определение указателя на подписчика Unix сигнала USR1
        QSocketNotifier *sn_term;           // Определение указателя на подписчика Unix сигнала TERM
    };

    QString get_ip_address();       // Объявление метода получения собственного IP-адреса
}

#endif // CVAPPLICATION_H   // Выход из области подключения
