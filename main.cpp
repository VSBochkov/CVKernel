#include <QCoreApplication>                 // подключение файла QCoreApplication для использования одноименного класса
#include <cstdlib>                          // подключение файла cstdlib для использования функции std::getenv()
#include <unistd.h>                         // подключение файла unistd.h для использования функции ::getpid()
#include "cvapplication.h"                  // подключение файла cvapplication.h для использования класса CVKernel::CVApplication
#include "videoproc/firedetectionfactory.h" // подключение файла firedetectionfactory.h для использования класса FireDetectionFactory

int main(int argc, char *argv[])            // Функция - точка входа в программу
{
    QCoreApplication app(argc, argv);       // Создание объекта класса QCoreApplication, параметрами конструктора являются аргументы командной строки
    qRegisterMetaType<QSharedPointer<CVKernel::CVProcessData>>("QSharedPointer<CVProcessData>");     // Регистрация типа разделяемого указателя на структуры CVProcessData и CVProcessForest из области видимости
    qRegisterMetaType<QSharedPointer<CVKernel::CVProcessForest>>("QSharedPointer<CVProcessForest>"); // CVKernel для возможности передавать объекты типов в аргументах сигналов/cлотов библиотеки Qt 5.
    QList<CVKernel::CVNodeFactory*> factories = { new FireDetectionFactory }; // Определение списка фабричных классов для конструирования процессорных модулей алгоритмов обнаружения пламени
    CVKernel::CVFactoryController::get_instance().set_factories(factories);   // Инициализация списка фабричных классов класса фабричного контроллера
    qDebug() << "CVKERNEL pid = " << ::getpid();                              // Вывод номера процесса приложения в терминал
    CVKernel::CVApplication::setup_unix_signal_handlers();                    // Установка обработчиков для UNIX сигналов SIGUSR1 и SIGTERM, для возможности управлять программой из интерфейса командной строки
    auto cv_app = new CVKernel::CVApplication(std::getenv("CVKERNEL_PATH") + QString("/cv_kernel_settings.json")); // Создание объекта класса CVApplication с передачей пути до файла сетевых настроек ядра
    QObject::connect(cv_app, SIGNAL(destroyed()), &app, SLOT(quit()));        // Установка пары сигнал/слот для сообщения "разрушен" от указателя на объект класса CVApplication. В качестве обработчика приложение завершает свою работу
    return app.exec();                                                        // Запуск основного событийного потока приложения
}
