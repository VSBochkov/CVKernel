#include "cvprocessmanager.h"       // Подключение файла cvprocessmanager.h для определения методов класса CVProcessManager
#include "cvnetworkmanager.h"       // Подключение файла cvnetworkmanager.h для использования класса CVNetworkManager
#include "cvgraphnode.h"            // Подключение файла cvgraphnode.h для использования классов CVIONode, CVProcessingNode
#include "cvfactorycontroller.h"    // Подключение файла cvfactorycontroller.h для использования класса CVFacroryController
#include <QObject>                  // Подключение файла QObject для использования одноименного класса
#include <QTimer>                   // Подключение файла QTimer для использования одноименного класса
#include <QThread>                  // Подключение файла QThread для использования одноименного класса

// Определение конструктора структуры CVProcessTree
// Входящий параметр root_node - указатель на корневой узел дерева процессов
CVKernel::CVProcessTree::CVProcessTree(CVProcessTree::Node* root_node)
{
    root = root_node; // присваивание корневого узла переменной root
}

// Определение метода set_network_manager класса CVProcessManager
// Входящий параметр manager - указатель на сетевого менеджера
void CVKernel::CVProcessManager::set_network_manager(CVNetworkManager* manager)
{
    network_manager_wp = manager; // присваивание указателя на сетевого менеджера переменной network_manager_wp
}

// Определение метода create_new_thread класса CVProcessManager
// Входящий параметр node - указатель на объект созданного процессорного узла
void CVKernel::CVProcessManager::create_new_thread(CVProcessingNode* node)
{
    QThread* cv_thread = new QThread;  // Создание нового потока
    node->moveToThread(cv_thread);     // Перемещение процессорного узла в поток cv_thread
    processing_nodes.insert(node);     // Добавление узла в множество процессорных узлов processing_nodes
    QObject::connect(cv_thread, SIGNAL(finished()), node, SLOT(deleteLater())); // Установка метода удаления процессорного узла в качестве обработчика сигнала завершения работы потока
    QObject::connect(network_manager_wp, SIGNAL(all_clients_closed()), node, SLOT(reset_average_time())); // Установка метода reset_average_time() процессорного узла в качетсве обработчика сигнала all_clients_closed сетевого менеджера
    cv_thread->start();     // Запуск потока cv_thread
    qDebug() << "Added new processing node: " << node->metaObject()->className(); // Вывод сообщения в интерфейс командной строки о добавлении нового процессорного узла с указанием имени класса, его реализующий
}

// Определение метода purpose класса CVProcessManager
// Входящие параметры:
//   par - указатель на описатель родителя процессорного узла
//   curr - указатель на текущий описатель процессорного узла
void CVKernel::CVProcessManager::purpose(CVProcessTree::Node* par, CVProcessTree::Node* curr)
{
    CVProcessingNode* parent_node = cv_processor[par->value].last(); // Получаем последний процессорный узел из словаря cv_processor по значению описателя родительского узла
    CVProcessingNode* curr_node; // Объявляем указатель на объект текущего процессорного узла

    auto iter = cv_processor.find(curr->value); // Определяем итератор с помощью метода поиска списка узлов по значению описателя текущего процессорного узла
    if (iter != cv_processor.end()) // Если находим список узлов по значению описателя текущего процессорного узла
    {
        curr_node = iter->last(); // Инициализируем указатель текущего процессорного узла адресом последнего используемого узла из найденного списка
        if (curr_node->average_time > 0.5 / max_fps) {    // Если среднее время работы текущего узла больше половины от минимального времени жизни кадра (Узел является тяжеловесным)
            curr_node = CVFactoryController::get_instance().create_processing_node(curr->value); // То создаем новый процессорный узел по значению описателя через фабричный контроллер
            if (curr_node != nullptr) { // Если процессорный узел был создан
                create_new_thread(curr_node);  // То создаем новый поток для процессорного узла current_node
            }
        }
    } else { // Если нет списка узлов по значению описателя
        curr_node = CVFactoryController::get_instance().create_processing_node(curr->value); // То создаем новый процессорный узел по значению описателя через фабричный контроллер
        if (curr_node != nullptr) { // Если процессорный узел был создан
            create_new_thread(curr_node);  // То создаем новый поток для процессорного узла current_node
        }
    }

    if (curr_node != nullptr) { // Если указатель на текущий процессорный узел не является нулевым
        cv_processor[curr->value].push_back(curr_node); // То помещаем его в конец списка узлов словаря cv_processor по значению описателя процессорного узла
    } else if (iter != cv_processor.end()) { // Иначе если находим список узлов по значению описателя текущего процессорного узла
        curr_node = iter->last(); // Инициализируем указатель текущего процессорного узла адресом последнего используемого узла найденного списка
    }

    if (curr_node != nullptr) { // Если указатель на текущий процессорный узел не является нулевым
        QObject::connect(parent_node, SIGNAL(next_node(QSharedPointer<CVProcessData>)), curr_node, SLOT(process(QSharedPointer<CVProcessData>)), Qt::UniqueConnection); // Устанавливаем метод process текущего узла в качестве обработчика сигнала next_node от родительского узла
        QObject::connect(parent_node, SIGNAL(destroyed()), curr_node, SLOT(deleteLater())); // Устанавливаем метод разрушения объекта текущего процессорного узла в качестве обработчика сигнала destroyed от родительского узла
    }
}

// Определение метода purpose класса CVProcessManager
// Входящие параметры:
//   video_io - указатель на узел ввода-вывода
//   root - указатель на описатель корневого узла дерева видеоаналитики
void CVKernel::CVProcessManager::purpose(CVIONode* video_io, CVProcessTree::Node* root)
{
    CVProcessingNode* root_node; // Объявляем указатель на объект корневого процессорного узла

    auto iter = cv_processor.find(root->value); // Определяем итератор с помощью метода поиска списка узлов по значению описателя корневого процессорного узла
    if (iter != cv_processor.end()) {  // Если находим список узлов по значению описателя корневого процессорного узла
        root_node = iter->last();  // Инициализируем указатель корневого процессорного узла адресом последнего используемого узла из найденного списка
        if (root_node->average_time > 0.5 / max_fps) {  // Если среднее время работы корневого узла больше половины от минимального времени жизни кадра (Узел является тяжеловесным)
            root_node = CVFactoryController::get_instance().create_processing_node(root->value); // То создаем новый процессорный узел по значению описателя через фабричный контроллер
            if (root_node != nullptr) {  // Если процессорный узел был создан
                create_new_thread(root_node); // То создаем новый поток для процессорного узла root_node
            }
        }
    } else { // Если нет списка узлов по значению описателя
        root_node = CVFactoryController::get_instance().create_processing_node(root->value); // То создаем новый процессорный узел по значению описателя через фабричный контроллер
        if (root_node != nullptr) {  // Если процессорный узел был создан
            create_new_thread(root_node); // То создаем новый поток для процессорного узла root_node
        }
    }

    if (root_node != nullptr) { // Если указатель на корневого процессорный узел не является нулевым
        cv_processor[root->value].push_back(root_node); // То помещаем его в конец списка узлов словаря cv_processor по значению описателя процессорного узла
    } else if(iter != cv_processor.end()) { // Иначе если находим список узлов по значению описателя корневого процессорного узла
        root_node = iter->last(); // Инициализируем указатель корневого процессорного узла адресом последнего используемого узла найденного списка
    }

    if (root_node != nullptr) { // Если указатель на корневого процессорный узел не является нулевым
        QObject::connect(video_io, SIGNAL(next_node(QSharedPointer<CVProcessData>)), root_node, SLOT(process(QSharedPointer<CVProcessData>))); // Устанавливаем метод process текущего узла в качестве обработчика сигнала next_node от узла ввода-вывода
    }
}

// Определение метода purpose_tree класса CVProcessManager
// Входящие параметры:
//   video_io - узел ввода-вывода
//   node - родительский описатель узла дерева процессов
void CVKernel::CVProcessManager::purpose_tree(CVIONode* video_io, CVProcessTree::Node* node)
{
    for (CVProcessTree::Node* child : node->children) { // Организация цикла по всем дочерним узлам родительского
        purpose(node, child);           // Назначение соединения между родительским и дочерним узлами
        purpose_tree(video_io, child);  // Рекурсивный метод назначения дерева для дочернего узла
        video_io->nodes_number++;       // Увеличение количества процессорных узлов для узла ввода-вывода на единицу
    }
}

// Определение метода purpose_processes класса CVProcessManager
// Входящие параметры:
//   processForest - список процессорных деревьев (процессорный лес) в спецификации видеоаналитики
//   video_io - узел ввода-вывода
void CVKernel::CVProcessManager::purpose_processes(
    QList<CVProcessTree*> processForest,
    CVIONode* video_io)
{
    for (CVProcessTree* processTree : processForest) {  // Организация цикла по всем деревьям леса
        purpose(video_io, processTree->root);           // Назначение соединения между узлом ввода-вывода и корневым процессорным узлом
        purpose_tree(video_io, processTree->root);      // Вызов метода назначения соединений узлов дерева начиная с корневого узла
        video_io->nodes_number++;                       // Увеличение количества процессорных узлов для узла ввода-вывода на единицу
    }
}

// Определение метода add_new_stream класса CVProcessManager
// Входящий параметр forest - указатель на лес процесса видеоаналитики
// Возвращаемое значение - указатель на созданный узел ввода-вывода
CVKernel::CVIONode* CVKernel::CVProcessManager::add_new_stream(QSharedPointer<CVProcessForest> forest)
{
    QThread* io_thread = new QThread;   // Создание нового потока
    CVIONode* io = new CVIONode(forest->device_in, forest->video_in, forest->video_out, forest->frame_width, forest->frame_height, forest->fps, forest->params_map); // Создаем новый узел ввода-вывода
    io->moveToThread(io_thread);        // Перемещение узла ввода-вывода в новый поток io_thread
    QObject::connect(io_thread, SIGNAL(started()), io, SLOT(process()));      // Установка метода process в качестве обработчика сигнала о старте нового потока
    QObject::connect(io, SIGNAL(node_closed()), io_thread, SLOT(quit()));     // Установка метода закрытия потока io_thread в качестве обработчика сигнала node_closed() от узла ввода-вывода
    QObject::connect(io_thread, SIGNAL(finished()), io, SLOT(deleteLater())); // Установка метода удаления узла ввода-вывода как обработчика сигнала о завершении потока io_thread
    max_fps = max_fps >= io->get_fps() ? max_fps : io->get_fps();             // Инициализация максимального значения кадров в секунду
    purpose_processes(forest->proc_forest, io);                               // Создание дерева видеоаналитики для ядра ввода-вывода исходя из спецификации
    io_thread->start();                                                       // Запуск нового потока
    return io;                                                                // Возврат указателя на созданный узел ввода-вывода
}

// Определение метода get_average_timings класса CVProcessManager
// Возвращаемое значение - словарь в котором ключом является имя класса процессорного узла и значение адреса памяти
//                         значением является среднее время выполнения обработки 1 кадра выраженное в секундах
QMap<QString, double> CVKernel::CVProcessManager::get_average_timings()
{
    QMap<QString, double> timings; // Объявление словаря характеристик среднего времени работы узлов
    for (CVProcessingNode* node: processing_nodes) // Организация цикла по всем процессорным узлам
    {
        timings[QString(node->metaObject()->className()) + "[" + QString::number((unsigned long)node, 16) + "]"] = node->average_time; // Присваивание характеристики среднего времени для узла
    }
    return timings; // Возврат словаря характеристик среднего времени работы узлов
}
