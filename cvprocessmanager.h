#ifndef CVPROCESSMANAGER_H
#define CVPROCESSMANAGER_H

#include <QObject>          // Подключение файла QObject для использования одноименного класса
#include <QMap>             // Подключение файла QMap для использования одноименного класса
#include <QSet>             // Подключение файла QSet для использования одноименного класса
#include <QList>            // Подключение файла QList для использования одноименного класса
#include <QTcpSocket>       // Подключение файла QTcpSocket для использования одноименного класса
#include <QString>          // Подключение файла QString для использования одноименного класса
#include <QHostAddress>     // Подключение файла QHostAddress для использования одноименного класса
#include <QSharedPointer>   // Подключение файла QSharedPointer для использования одноименного класса

namespace CVKernel {        // Определяем область видимости CVKernel
    class CVIONode;         // Предварительное объявление класса CVIONode
    class CVNodeParams;     // Предварительное объявление класса CVNodeParams
    class CVProcessingNode; // Предварительное объявление класса CVProcessingNode
    class CVNetworkManager; // Предварительное объявление класса CVNetworkManager

    struct CVProcessTree {  // Определение класса CVProcessTree описания спецификации дерева видеоаналитики
        struct Node {               // Определение структуры узла
            Node* parent;           // Определение указателя на родительский узел дерева
            QString value;          // Определение строки типа процессорного узла
            QList<Node*> children;  // Определение списка указателей на дочерние узлы дерева
            Node(Node* par = NULL, QString val = "") // Определение конструктора структуры
                : parent(par), value(val)   // Список инициализации
            {
                children = QList<Node*>();  // Создаем пусттой список узлов
                if (parent != NULL)         // Если узел имеет родительский
                    parent->children.push_back(this);   // То добавляем в список дочерних узлов родителя собственный узел
            }
        }* root;    // Определение корневого узла

        CVProcessTree(Node* root_node); // Объявление конструктора класса спецификации дерева видеоаналитики
        void delete_tree(Node* node) {  // Определение метода удаления дерева
            for (CVProcessTree::Node* child : node->children)   // Для всех дочерних узлов
                delete_tree(child);     // Рекурсивно вызываем удаление дерева
            delete node;    // Удаление собственного узла
        }
        ~CVProcessTree() { delete_tree(root); } // Определение деструктора класса
    };

    class CVProcessForest { // Определение класса спецификации леса видеоаналитики
    public: // Открытая секция класса
        CVProcessForest(QJsonObject& json_obj); // Объявление конструктора
        ~CVProcessForest() {                    // Определение деструктора
            for (CVKernel::CVProcessTree* tree : proc_forest) // Для всех деревьев леса
                delete tree;        // Удаляем дерево
            proc_forest.clear();    // Очищаем список деревьев
        }

    private: // Закрытая секция класса
        void recursive_parse(QJsonObject json_node, CVProcessTree::Node* node); // Объявление метода рекурсивного разбора JSON спецификации
        CVProcessTree::Node* create_node(QJsonObject& json_node, CVProcessTree::Node* parent = NULL);   // Объявление метода создания узла дерева

    public: // Открытая секция класса
        QList<CVProcessTree*> proc_forest;  // Определение списка деревьев леса видеоаналитики
        QMap<QString, QSharedPointer<CVNodeParams>> params_map; // Определение словаря параметров процессорных узлов
        int     device_in = 0;      // Определение номера USB камеры
        QString video_in = "";      // Определение пути до входного видеопотока
        QString video_out = "";     // Определение пути до исходящего видеопотока
        int     meta_udp_port;      // Определение порта передачи метаданных
        int     frame_width;        // Определение ширины входящего кадра
        int     frame_height;       // Определение высоты входящего кадра
        double  fps;                // Определение количества кадров в секунду
    };

    class CVProcessManager  // Определение класса CVProcessManager
    {
    public:     // Открытая секция класса
        QMap<QString, double> get_average_timings();                            // Объявление метода получения среднего времени для всех процессорных узлов
        void purpose_processes(QList<CVProcessTree*> processForest, CVIONode* video_io);    // Объявление метода формирования связей процессорных узлов с узлом ввода-вывода
        void set_network_manager(CVNetworkManager* manager);                    // Объявление метода присвоения указателя на сетевой менеджер
        CVIONode* add_new_stream(QSharedPointer<CVProcessForest> proc_forest);  // Объявление метода создания нового узла ввода-вывода

    private:    // Закрытая секция класса
        void create_new_thread(CVProcessingNode* node);                         // Объявление метода создания потока для нового процессорного узла
        void purpose(CVProcessTree::Node* parent, CVProcessTree::Node* curr);   // Объявление метода установки соединения между родительским и текущим процессорными узлами
        void purpose(CVIONode* video_io, CVProcessTree::Node* root);    // Объявление метода установки соединения между узлом ввода-вывода и корневым процессорными узлами
        void purpose_tree(CVIONode* video_io, CVProcessTree::Node* node); // Объявление метода установки соединений дерева видеопроцессинга

        QMap<QString, QList<CVProcessingNode*>> cv_processor;       // Определение словаря списков процессорных узлов по имени класса в качестве ключа
        CVNetworkManager* network_manager_wp;                       // Объявление слабого указателя на сетевой менеджер
        double max_fps;                                             // Определение максимального количества кадров в секунду среди всех входящих видеопотоков

    public:     // Открытая секция класса
        QSet<CVProcessingNode*> processing_nodes;   // Определение множества процессорных узлов
    };

}


#endif // CVPROCESSMANAGER_H   // Выход из области подключения
