#include "cvjsoncontroller.h"
#include "cvnetworkmanager.h"
#include "cvprocessmanager.h"
#include "cvapplication.h"
#include "cvgraphnode.h"
#include "cvconnector.h"

#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>


CVKernel::CVNetworkManagerSettings::CVNetworkManagerSettings(QJsonObject& json_object) {
    QJsonObject::iterator iter = json_object.find("tcp_server_port");
    tcp_server_port = iter == json_object.end() ? 0 : iter.value().toInt();
}

CVKernel::CVConnectorType::CVConnectorType(QJsonObject& json_obj) {
    QJsonObject::iterator iter = json_obj.find("type");

    if (iter != json_obj.end()) {
        QString val = iter.value().toString();
        type = val == "client" ? CVConnectorType::client : CVConnectorType::supervisor;
    } else {
        type = CVConnectorType::supervisor;
    }
}

CVKernel::CVProcessForest::CVProcessForest(QJsonObject& json_obj) {
    QJsonObject::iterator iter;
    if ((iter = json_obj.find("input_path")) != json_obj.end()) {
        video_in = QString(iter.value().toString());
    } else if ((iter = json_obj.find("input_device")) != json_obj.end()) {
        device_in = iter.value().toInt();
    } else {
        return;
    }

    if ((iter = json_obj.find("output_path")) != json_obj.end()) {
        video_out = QString(iter.value().toString());
    }

    iter = json_obj.find("proc_resolution");
    if (iter != json_obj.end()) {
        QStringList resol = iter.value().toString().split('x');
        frame_width = resol[0].toInt();
        frame_height = resol[1].toInt();
    } else {
        frame_width = 320;
        frame_height = 240;
    }

    fps = (iter = json_obj.find("fps")) == json_obj.end() ? 25. : iter.value().toDouble();

    if ((iter = json_obj.find("meta_udp_path")) != json_obj.end()) {
        QStringList meta_udp_path = QString(iter.value().toString()).split(':');
        if (meta_udp_path.size() == 2) {
            meta_udp_addr = meta_udp_path[0];
            meta_udp_port = meta_udp_path[1].toInt();
        }
    }

    if ((iter = json_obj.find("process")) == json_obj.end())
        return;

    QJsonArray forest = iter.value().toArray();
    proc_forest.reserve(forest.size());
    for (QJsonValueRef tree_ref : forest) {
        QJsonObject tree = tree_ref.toObject();
        CVProcessTree::Node* root = create_node(tree);
        recursive_parse(tree, root);
        proc_forest.push_back(new CVProcessTree(root));
    }
}

void CVKernel::CVProcessForest::recursive_parse(QJsonObject json_node, CVProcessTree::Node* node) {
    QJsonObject::iterator iter;
    if ((iter = json_node.find("children")) == json_node.end())
        return;

    QJsonArray json_children = iter.value().toArray();
    for (int i = 0; i < json_children.size(); ++i) {
        QJsonObject child_jobj = json_children[i].toObject();
        CVProcessTree::Node* child_node = create_node(child_jobj, node);
        recursive_parse(child_jobj, child_node);
    }
}

CVKernel::CVProcessTree::Node* CVKernel::CVProcessForest::create_node(QJsonObject& json_node, CVProcessTree::Node* parent) {
    QJsonObject::iterator iter;
    if ((iter = json_node.find("node")) == json_node.end())
        return NULL;

    QString class_name(iter.value().toString());
    QSharedPointer<CVNodeParams> params(CVFactoryController::get_instance().create_node_params(json_node));
    if (params != nullptr) {
        params_map[class_name] = params;
    }
    return new CVProcessTree::Node(parent, class_name);
}

CVKernel::CVCommand::CVCommand(QJsonObject& json_object) {
    QJsonObject::iterator iter = json_object.find("command");
    command = iter == json_object.end() ? command_value::stop : (command_value) iter.value().toInt();
}

CVKernel::CVNodeParams::CVNodeParams(QJsonObject& json_obj) {
    QJsonObject::iterator iter = json_obj.find("draw_overlay");
    draw_overlay = iter == json_obj.end() ? false : iter.value().toBool();
}

QJsonObject CVKernel::CVProcessData::pack_to_json() {
    QJsonObject metadata_json;
    for (auto it = data.begin(); it != data.end(); it++) {
        QJsonObject node_obj = it.value()->pack_to_json();
        if (not node_obj.empty()) {
            metadata_json[it.key()] = node_obj;
        }
    }
    return metadata_json;
}

QJsonObject CVKernel::CVNodeData::pack_to_json() {
    return QJsonObject();
}

QJsonObject CVKernel::CVConnectorState::pack_to_json() {
    QJsonObject state_json;
    state_json["state"] = state;
    return state_json;
}


QJsonObject CVKernel::CVSupervisionInfo::pack_to_json() {
    QJsonObject sup_json;
    sup_json["connected_clients"] = connected_clients;
    sup_json["used_process_nodes"] = used_process_nodes;
    QJsonObject timings_json;
    for (auto iter = average_timings.begin(); iter != average_timings.end(); iter++)
    {
        timings_json[iter.key()] = iter.value();
    }
    sup_json["average_timings"] = timings_json;
    return sup_json;
}

QJsonObject CVKernel::CVClientChangedParams::pack_to_json() {
    QJsonObject client_json;
    client_json["id"] = (int) id;
    client_json["state"] = state.state;
    client_json["overlay_path"] = overlay_path;
    return client_json;
}

QJsonObject CVKernel::CVSupervisorChangedParams::pack_to_json() {
    QJsonObject sup_json;
    sup_json["id"] = (int) id;
    sup_json["state"] = state.state;
    return sup_json;
}

QJsonObject CVKernel::CVSupervisorStartup::pack_to_json() {
    QJsonObject clients_json;
    QJsonArray clients_json_array;
    for (QSharedPointer<CVClient>& client : client_list)
    {
        QJsonObject client_json;
        client_json["id"] = (int) client->id;
        client_json["state"] = CVClientState(client.data()).state;
        client_json["used_proc_nodes"] = client->video_io->nodes_number;
        client_json["overlay_path"] = client->video_io->get_overlay_path();
        clients_json_array.push_back(client_json);
    }
    clients_json["clients"] = clients_json_array;
    return clients_json;
}
