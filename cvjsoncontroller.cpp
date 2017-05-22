#include "cvjsoncontroller.h"       // Подключение файла cvnetworkmanager.h для определения методов класса CVJsonController
#include "cvnetworkmanager.h"       // Подключение файла cvnetworkmanager.h для определения методов методов сериализации и десериализации JSON спецификаций
#include "cvprocessmanager.h"       // Подключение файла cvprocessmanager.h для определения методов методов сериализации и десериализации JSON спецификаций
#include "cvapplication.h"          // Подключение файла cvapplication.h для определения методов методов сериализации и десериализации JSON спецификаций
#include "cvgraphnode.h"            // Подключение файла cvgraphnode.h для определения методов методов сериализации и десериализации JSON спецификаций
#include "cvconnector.h"            // Подключение файла cvconnector.h для определения методов методов сериализации и десериализации JSON спецификаций
#include "implementation/cvclient.h"       // Подключение файла cvclient.h для определения методов методов сериализации и десериализации JSON спецификаций
#include "implementation/cvsupervisor.h"   // Подключение файла cvsupervisor.h для определения методов методов сериализации и десериализации JSON спецификаций
#include <QFile>            // Подключение файла QFile для использования одноименного класса
#include <QTextStream>      // Подключение файла QTextStream для использования одноименного класса
#include <QJsonDocument>    // Подключение файла QJsonDocument для использования одноименного класса
#include <QJsonObject>      // Подключение файла QJsonObject для использования одноименного класса
#include <QJsonArray>       // Подключение файла QJsonArray для использования одноименного класса

// Определение нумерации типов REST пакетов
enum class packet_type {
    client_status = 0,      // Изменение статуса клиента ядра
    supervisor_status = 1,  // Изменение статуса супервайзера
    supervisor_startup = 2, // Синхронизация статуса ядра на старте работы супервайзера
    supervision_info = 3    // Текущий статус ядра
};

// Определение конструктора класса CVNetworkSettings
// Входящий параметр json_object - ссылка на структуру JSON спецификации для разбора
CVKernel::CVNetworkSettings::CVNetworkSettings(QJsonObject& json_object)
{
    QJsonObject::iterator iter; // Объявление итератора по JSON структуре
    mac_address = (iter = json_object.find("mac_address")) == json_object.end() ? "" : iter.value().toString(); // Устанавливаем значение mac_address по найденному ключу словаря JSON, если ключ не найден то инициализируем значением ""
    tcp_server_port = (iter = json_object.find("tcp_server_port")) == json_object.end() ? 0 : iter.value().toInt(); // Устанавливаем значение tcp_server_port по найденному ключу словаря JSON, если ключ не найден то инициализируем значением 0
}

// Определение конструктора класса CVConnectorType
// Входящий параметр json_object - ссылка на структуру JSON спецификации для разбора
CVKernel::CVConnectorType::CVConnectorType(QJsonObject& json_obj)
{
    QJsonObject::iterator iter = json_obj.find("type"); // Определение итератора по JSON структуре адресом нахождения элемента type

    if (iter != json_obj.end()) {   // Элемент type найден в спецификации
        QString val = iter.value().toString();  // Берем значение поля type
        type = val == "client" ? CVConnectorType::client : CVConnectorType::supervisor; // Устанавливаем значение типа коннектора в зависимости от его строкового представления
    } else { // Элемент не найден
        type = CVConnectorType::supervisor; // То тип коннектора - супервайзер
    }
}

// Определение конструктора класса CVProcessForest
// Входящий параметр json_object - ссылка на структуру JSON спецификации для разбора
CVKernel::CVProcessForest::CVProcessForest(QJsonObject& json_obj)
{
    QJsonObject::iterator iter; // Объявление итератора по JSON структуре
    if ((iter = json_obj.find("input_path")) != json_obj.end()) { // Если найден элемент input_path в спецификации
        video_in = QString(iter.value().toString());              // Инициализируем video_in строковым значением элемента input_path
    } else if ((iter = json_obj.find("input_device")) != json_obj.end()) { // Если найден элемент input_device в спецификации
        device_in = iter.value().toInt(); // Инициализируем device_in целочисленным значением элемента input_device
    } else { // Если не найдено ни одного целевого элемента
        return; // То выход из функции
    }

    iter = json_obj.find("output_path");  // Поиск элемента output_path
    if (iter != json_obj.end()) {         // Если элемент найден
        video_out = QString(iter.value().toString()); // Инициализируем video_out строковым значением элемента output_path
    }

    iter = json_obj.find("proc_resolution"); // Поиск элемента proc_resolution
    if (iter != json_obj.end()) {            // Если элемент найден
        QStringList resol = iter.value().toString().split('x'); // Разбиваем строковое значение на список строк, разделенный символом 'x'
        frame_width = resol[0].toInt();                         // Инициализируем ширину кадра первым значением списка
        frame_height = resol[1].toInt();                        // Инициализируем высоту кадра вторым значением списка
    } else {                                 // Если элемент не найден
        frame_width = 320;                   // То ширина кадра устанавливается по-умолчанию в значение 320
        frame_height = 240;                  // Высота кадра устанавливается по-умолчанию в значение 240
    }

    fps = (iter = json_obj.find("fps")) == json_obj.end() ? 25. : iter.value().toDouble(); // Устанавливаем значение fps по найденному ключу словаря JSON, если ключ не найден то инициализируем значением 25.0

    if ((iter = json_obj.find("meta_udp_port")) != json_obj.end()) { // Если найден элемент meta_udp_port в спецификации
        meta_udp_port = iter.value().toInt();                        // Инициализируем meta_udp_port числовым значением элемента
    }

    if ((iter = json_obj.find("process")) == json_obj.end())        // Если не найден элемент описания процесса видеоаналитики
        return;     // То выходим из функции

    QJsonArray forest = iter.value().toArray();     // Инициализируем значение forest приведением найденного элемента к типу массива структур JSON
    proc_forest.reserve(forest.size());             // Резервируем память под список деревьев
    for (QJsonValueRef tree_ref : forest) {         // Организуем цикл по массиву forest
        QJsonObject tree = tree_ref.toObject();     // Переводим значение элемента массива к типу объекта JSON
        CVProcessTree::Node* root = create_node(tree); // Создаем описатель корневого узла дерева
        if (root != nullptr) {                        // Если создан объект описателя
            recursive_parse(tree, root);                    // То вызываем метод рекурсивного разбора дерева процессов
            proc_forest.push_back(new CVProcessTree(root)); // Вставляем дерево в конец списка process_forest
        }
    }
}

// Определение метода recursive_parse класса CVProcessForest
// Входящие параметры
//   json_node - родительский JSON узел дерева
//   node - описатель родительского процессорного узла дерева
void CVKernel::CVProcessForest::recursive_parse(QJsonObject json_node, CVProcessTree::Node* node)
{
    QJsonObject::iterator iter; // Объявление итератора по JSON структуре
    if ((iter = json_node.find("children")) == json_node.end()) // Если не найден элемент с ключом chindlren для JSON узла дерева
        return; // То выход из функции

    QJsonArray json_children = iter.value().toArray(); // Инициализируем значение json_children приведением найденного элемента к типу массива структур JSON
    for (int i = 0; i < json_children.size(); ++i) {   // Организуем цикл по всем элементам массива json_children
        QJsonObject child_jobj = json_children[i].toObject();   // Переводим значение элемента массива к типу объекта JSON
        CVProcessTree::Node* child_node = create_node(child_jobj, node); // Создаем объект описателя процессорного узла передавая в аргументах значение текущего JSON объекта и родительский описатель процессорного узла
        if (child_node != nullptr) {                       // Если создан объект описателя
            recursive_parse(child_jobj, child_node);       // То вызываем метод рекурсивного разбора для текущего процессорного узла
        }
    }
}

// Определение метода create_node класса CVProcessForest
// Входящие параметры:
//   json_node - JSON объект описания текущего узла
//   parent - описатель родительского процессорного узла
// Возвращаемое значение - указатель на новый описатель процессорного узла
CVKernel::CVProcessTree::Node* CVKernel::CVProcessForest::create_node(QJsonObject& json_node, CVProcessTree::Node* parent)
{
    QJsonObject::iterator iter; // Объявление итератора по JSON структуре
    if ((iter = json_node.find("node")) == json_node.end()) // Если не найден элемент с ключом node для JSON узла дерева
        return nullptr;        // Возврат нулевого указателя

    QString class_name(iter.value().toString()); // Инициализируем переменную имени класса реализующего процессорный узел значением по ключу node
    QSharedPointer<CVNodeParams> params(CVFactoryController::get_instance().create_node_params(json_node)); // Инициализируем структуру параметров процессорного узла
    if (params != nullptr) {             // Если структура была создана
        params_map[class_name] = params; // Записываем ее в словарь params_map по ключу имени класса
    }
    return new CVProcessTree::Node(parent, class_name); // возврат нового описателя процессорного узла
}

// Определение конструктора класса CVCommand
// Входящий параметр json_object - ссылка на структуру JSON спецификации для разбора
CVKernel::CVCommandType::CVCommandType(QJsonObject& json_object)
{
    QJsonObject::iterator iter = json_object.find("command");   // Инициализация итератора по JSON структуре адресом элемента с ключом command
    command = iter == json_object.end() ? command_value::stop : (command_value) iter.value().toInt(); // Приведения целочисленного значения найденного элемента к типу command_value, если элемент не найден то инициализации команды значением stop
}

// Определение конструктора класса CVNodeParams
// Входящий параметр json_object - ссылка на структуру JSON спецификации для разбора
CVKernel::CVNodeParams::CVNodeParams(QJsonObject& json_obj)
{
    QJsonObject::iterator iter = json_obj.find("draw_overlay");  // Инициализация итератора по JSON структуре адресом элемента с ключом draw_overlay
    draw_overlay = iter == json_obj.end() ? false : iter.value().toBool(); // Инициализация draw_overlay булевым значением найденного элемента, если элемент не найден то инициализация значением false
}

// Определение конструктора класса CVSupervisionSettings
// Входящий параметр settings_json - ссылка на структуру JSON спецификации для разбора
CVKernel::CVSupervisionSettings::CVSupervisionSettings(QJsonObject& settings_json)
{
    QJsonObject::iterator iter = settings_json.find("port");       // Инициализация итератора по JSON структуре адресом элемента с ключом port
    port = iter == settings_json.end() ? 0 : iter.value().toInt(); // Инициализация port целочисленным значением найденного элемента, если элемент не найден то инициализация значением 0
    iter = settings_json.find("update_timer_value");               // Инициализация итератора по JSON структуре адресом элемента с ключом update_timer_value
    update_timer_value = iter == settings_json.end() ? 0 : iter.value().toInt(); // Инициализация update_timer_value целочисленным значением найденного элемента, если элемент не найден то инициализация значением 0
}

// Определение метода pack_to_json класса CVProcessData
// Возвращаемое значение - JSON объект содержащий данные структуры CVProcessData
QJsonObject CVKernel::CVProcessData::pack_to_json()
{
    QJsonObject metadata_json; // Объявление JSON объекта metadata_json
    for (auto it = data.begin(); it != data.end(); it++) {  // Организация цикла по всем парам ключ, значение словаря data
        QJsonObject node_obj = it.value()->pack_to_json();  // Вызов метода перевода объекта CVNodeData в формат JSON
        if (not node_obj.empty()) {                         // Если node_obj не пуст
            metadata_json[it.key()] = node_obj;             // То сохранение его в JSON объекте metadata_json
        }
    }
    return metadata_json;                                   // Возврат JSON объекта metadata_json
}

// Определение метода pack_to_json класса CVNodeData
// Метод является виртуальным и переопределяемым в классах потомках CVNodeData
QJsonObject CVKernel::CVNodeData::pack_to_json()
{
    return QJsonObject(); // Возврат пустого JSON объекта
}

// Определение метода pack_to_json класса CVConnectorState
// Возвращаемое значение - JSON объект содержащий данные структуры CVConnectorState
QJsonObject CVKernel::CVConnectorState::pack_to_json()
{
    QJsonObject conn_st_json;       // Объявление JSON объекта
    conn_st_json["id"] = (int) connector.get_id();  // Присвоение номера коннектора по ключу "id" JSON объекта
    conn_st_json["state"] = (int) value;            // Присвоение целочисленного значения состояния коннектора по ключу "state" JSON объекта

    if (typeid(connector) == typeid(CVClient&))     // Если тип коннектора - клиент ядра
    {
        CVClient& client = static_cast<CVClient&>(connector);           // То приводим объект к типу CVClient
        if (value != CVConnectorState::closed)                          // Если значение состояния клиента не равно "Не активен"
            conn_st_json["overlay_path"] = client.get_overlay_path();   // То записываем значение пути до видео с наложением результатов анализа по ключу overlay_path
        conn_st_json["type"] = (int) packet_type::client_status;        // Записываем целочисленное значение типа транзакции "статус клиента" по ключу type JSON объекта
    }
    else        // Иначе если тип коннектора - супервайзер
    {
        conn_st_json["type"] = (int) packet_type::supervisor_status;    // Записываем целочисленное значение типа транзакции "статус супервайзера" по ключу type JSON объекта
    }

    return conn_st_json; // Возврат JSON объекта
}

// Определение метода pack_to_json класса CVSupervisorStartup
// Возвращаемое значение - JSON объект содержащий данные структуры CVSupervisorStartup
QJsonObject CVKernel::CVSupervisorStartup::pack_to_json()
{
    QJsonObject clients_json;                // Объявление JSON объекта
    QJsonArray clients_json_array;           // Объявление JSON массива
    std::unique_ptr<CVConnectorState> state; // Объявление неразделяемого указателя на структуру CVConnectorState

    clients_json["type"] = (int) packet_type::supervisor_startup;   // Записываем целочисленное значение типа транзакции "синхронизация супервизора" по ключу type JSON объекта
    for (QSharedPointer<CVClient>& client : client_list)            // Организуем цикл по всем клиентам ядра
    {
        QJsonObject client_json;                                    // Объявление JSON объекта
        state.reset(CVClientFactory::instance()->set_state(*client)); // Инициализация состояния клиента
        client_json["id"] = (int) client->get_id();                 // Записываем целочисленное значение номера клиента по ключу id JSON объекта client_json
        client_json["state"] = (int) state->value;                  // Записываем целочисленное значение состояния клиента по ключу state JSON объекта client_json
        client_json["used_proc_nodes"] = client->get_proc_nodes_number(); // Записываем целочисленное значение количества используемых процессорных ядер текущим клиентом по ключу used_proc_nodes JSON объекта client_json
        client_json["overlay_path"] = client->get_overlay_path();  // Записываем строковое значение пути до выходного видео по ключу overlay_path JSON объекта client_json
        clients_json_array.push_back(client_json);                 // Помещение объекта client_json в JSON массив clients_json_array
    }
    clients_json["clients"] = clients_json_array;                  // Присваивание массива clients_json_array по ключу "clients" объекту clients_json
    return clients_json;        // Возврат JSON объекта clients_json
}

// Определение метода pack_to_json класса CVSupervisionInfo
// Возвращаемое значение - JSON объект содержащий данные структуры CVSupervisionInfo
QJsonObject CVKernel::CVSupervisionInfo::pack_to_json()
{
    QJsonObject sup_json;   // Объявление JSON объекта
    sup_json["type"] = (int) packet_type::supervision_info; // Записываем целочисленное значение типа транзакции "статус ядра" по ключу type JSON объекта
    sup_json["connected_clients"] = connected_clients;      // Записываем целочисленное значение количества подключенных клиентов по ключу connected_clients JSON объекта
    sup_json["used_process_nodes"] = used_process_nodes;    // Записываем целочисленное значение количества процессорных модулей по ключу used_process_nodes JSON объекта
    QJsonObject timings_json;                               // Объявление JSON объекта
    for (auto iter = average_timings.begin(); iter != average_timings.end(); iter++) // Организуем цикл по всем парам словаря average_timings
    {
        timings_json[iter.key()] = iter.value();    // Записываем по ключу имени класса процессорного узла численное значение среднего времени его работы над кадром
    }
    sup_json["average_timings"] = timings_json;     // Записываем структуру JSON объекта timings_json по ключу "average_timings"
    return sup_json;     // Возврат JSON объекта sup_json
}
