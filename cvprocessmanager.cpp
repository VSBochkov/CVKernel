#include "cvprocessmanager.h"
#include "videoproc/rfiremaskingmodel.h"
#include "videoproc/yfiremaskingmodel.h"
#include "videoproc/firevalidation.h"
#include "videoproc/firebbox.h"
#include "videoproc/fireweight.h"
#include "videoproc/flamesrcbbox.h"
#include "cvgraphnode.h"

#include <QTimer>
#include <QThread>

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include <iostream>

using namespace CVKernel;

CVProcessManager::CVProcessManager(QObject *parent) :
    QObject(parent) {
}

void CVProcessManager::close_threads() {}

CVProcessManager::~CVProcessManager() {
    close_threads();
}

CVProcessTree::CVProcessTree(CVProcessTree::Node* root_node) {
    root = root_node;
}

void CVProcessManager::createNewThread(CVProcessingNode* node) {
    QThread* cv_thread = new QThread;
    node->moveToThread(cv_thread);
    processing_nodes.insert(node);
    connect(node, SIGNAL(destroyed()), cv_thread, SLOT(quit()));
    cv_thread->start();
}

void CVProcessManager::addProcessingNode(CVProcessTree::Node* node) {
    switch(node->value) {
        case r_firemm:      cv_processor[node->value].append(new RFireMaskingModel(node->ip_deliver, node->draw_overlay)); break;
        case y_firemm:      cv_processor[node->value].append(new YFireMaskingModel(node->ip_deliver, node->draw_overlay)); break;
        case fire_valid:    cv_processor[node->value].append(new FireValidation(node->ip_deliver, node->draw_overlay)); break;
        case fire_bbox:     cv_processor[node->value].append(new FireBBox(node->ip_deliver, node->draw_overlay)); break;
        case fire_weights:  cv_processor[node->value].append(new FireWeightDistrib(node->ip_deliver, node->draw_overlay)); break;
        case flame_src_bbox:cv_processor[node->value].append(new FlameSrcBBox(node->ip_deliver, node->draw_overlay)); break;
        case nothing: return;
    }
    createNewThread(cv_processor[node->value].last());
}

void CVProcessManager::purpose(CVProcessTree::Node* par, CVProcessTree::Node* curr) {
    CVProcessingNode* parent_node = cv_processor[par->value].last();
    CVProcessingNode* curr_node;
    connection conn(parent_node, curr_node);

    if (cv_processor.find(curr->value) != cv_processor.end())
    {
        curr_node = cv_processor[curr->value].last();
        if (parent_node == curr_node || connections.contains(conn.get_reverse()))
            return;
        if (
            curr_node->draw_overlay != curr->draw_overlay || curr_node->ip_deliever != curr->ip_deliver ||
            curr_node->averageTime() > 0.5 / max_fps
        ) {
            addProcessingNode(curr);
        }
    } else {
        addProcessingNode(curr);
    }

    curr_node = cv_processor[curr->value].last();
    connect(parent_node, SIGNAL(nextNode(CVProcessData, CVIONode*)), curr_node, SLOT(process(CVProcessData, CVIONode*)), Qt::UniqueConnection);
    connect(parent_node, SIGNAL(destroyed()), curr_node, SLOT(deleteLater()));
    connect(parent_node, SIGNAL(nextStat()), curr_node, SLOT(printStat()), Qt::UniqueConnection);
    connections.push_back(conn);
}

void CVProcessManager::purpose(CVIONode* video_io, CVProcessTree::Node* root) {
    CVProcessingNode* root_node;

    if (cv_processor.find(root->value) != cv_processor.end()) {
        root_node = cv_processor[root->value].last();
        if (
            root_node->draw_overlay != root->draw_overlay || root_node->ip_deliever != root->ip_deliver ||
            root_node->averageTime() > 0.5 / max_fps
        ) {
            addProcessingNode(root);
        }
    } else {
        addProcessingNode(root);
    }

    root_node = cv_processor[root->value].last();
    connect(video_io, SIGNAL(nextNode(CVProcessData, CVIONode*)), root_node, SLOT(process(CVProcessData, CVIONode*)));
    connect(video_io, SIGNAL(destroyed()), root_node, SLOT(deleteLater()));
    connect(video_io, SIGNAL(print_stat()), root_node, SLOT(printStat()), Qt::UniqueConnection);
}

void CVProcessManager::purpose_tree(CVIONode* video_io, CVProcessTree::Node* node) {
    for (CVProcessTree::Node* child : node->children) {
        purpose(node, child);
        purpose_tree(video_io, child);
        video_io->nodes_number++;
    }
    if (node->parent != NULL)
        purpose(node->parent, node);
}

void CVProcessManager::purposeProcesses(
    QList<CVProcessTree*> processForest,
    CVIONode* video_io)
{
    for (CVProcessTree* processTree : processForest) {
        purpose(video_io, processTree->root);
        purpose_tree(video_io, processTree->root);
        video_io->nodes_number++;
    }
}

void CVProcessManager::addNewStream(CVForestParser &forest) {

    CVIONode* io(forest.video_in.isEmpty() ? new CVIONode(
                                                 forest.device_in,
                                                 !forest.video_out.isEmpty(),
                                                 forest.ip_addr,
                                                 forest.ip_port
                                             ) :
                                             new CVIONode(
                                                 forest.video_in,
                                                 !forest.video_out.isEmpty(),
                                                 forest.ip_addr,
                                                 forest.ip_port,
                                                 forest.video_out
                                                 )
                                             );
    QThread* io_thread = new QThread(this);
    io->moveToThread(io_thread);
    connect(io_thread, SIGNAL(started()), io, SLOT(process()));
    connect(io, SIGNAL(EOS()), io_thread, SLOT(quit()));
    connect(io_thread, SIGNAL(finished()), io, SLOT(deleteLater()));
    max_fps = max_fps >= io->get_fps() ? max_fps : io->get_fps();
    purposeProcesses(forest.proc_forest, io);
    io_thread->start();
}

void CVForestParser::recursive_parse(QJsonObject json_node, CVProcessTree::Node* node) {
    node->draw_overlay = json_node["draw_overlay"].toBool();
    node->ip_deliver = json_node["ip_deliver"].toBool();
    QJsonArray json_children = json_node["children"].toArray();
    node->children.reserve(json_children.size());
    for (int i = 0; i < json_children.size(); ++i) {
        node->children[i] = new CVProcessTree::Node(node, get_node(json_children[i].toObject()["node"].toString()));
        recursive_parse(json_children[i].toObject(), node->children[i]);
    }
}

CVForestParser& CVForestParser::parseJSON(QString json_fname) {
    proc_forest.clear();
    QFile json_file(json_fname);
    if (!json_file.open(QIODevice::ReadOnly | QIODevice::Text))
        return *this;

    QJsonDocument json = QJsonDocument::fromJson(json_file.readAll());
    json_file.close();
    QJsonObject json_obj = json.object();
    if (json_obj["file_input"].isUndefined())
        device_in = json_obj["device_input"].toInt();
    else
        video_in = QString(json_obj["file_input"].toString());
    if (!json_obj["file_output"].isUndefined())
        video_out = QString(json_obj["file_output"].toString());

    ip_addr = json_obj["ip_address"].isUndefined() ? "" : QString(json_obj["ip_address"].toString());
    ip_port = json_obj["ip_port"].isUndefined() ? 0 : json_obj["ip_port"].toInt();

    QJsonArray forest = json_obj["process"].toArray();
    proc_forest.reserve(forest.size());
    for (QJsonValueRef tree : forest) {
        CVProcessTree::Node* root = new CVProcessTree::Node(NULL, get_node(QJsonValue(tree.toObject()["node"]).toString()));
        recursive_parse(tree.toObject(), root);
        proc_forest.push_back(new CVProcessTree(root));
    }
    return *this;
}
