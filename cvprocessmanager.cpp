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
    cv_processor[r_firemm].append(new RFireMaskingModel);
    cv_processor[y_firemm].append(new YFireMaskingModel);
    cv_processor[fire_valid].append(new FireValidation);
    cv_processor[fire_bbox].append(new FireBBox);
    cv_processor[fire_weights].append(new FireWeightDistrib);
    cv_processor[flame_src_bbox].append(new FlameSrcBBox);
    logger_node = new CVLoggerNode(0, 100);
    logger_thread = new QThread;
    logger_node->moveToThread(logger_thread);
    logger_thread->start();
}

void CVProcessManager::close_threads() {
    delete logger_node;
    logger_thread->exit();
    delete logger_thread;
}

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

void CVProcessManager::purpose(cv_node par_val, cv_node curr_val) {
    CVProcessingNode* curr_node = cv_processor[curr_val].last();
    CVProcessingNode* parent_node = cv_processor[par_val].last();
    connection conn(parent_node, curr_node);
    if (parent_node == curr_node || connections.contains(conn.get_reverse()))
        return;
    if (!processing_nodes.contains(curr_node)) createNewThread(curr_node);
    else if (curr_node->averageTime() > 0.5 / max_fps) {
        if      (dynamic_cast<RFireMaskingModel*>(curr_node)) cv_processor[curr_val].append(new RFireMaskingModel);
        else if (dynamic_cast<YFireMaskingModel*>(curr_node)) cv_processor[curr_val].append(new YFireMaskingModel);
        else if (dynamic_cast<FireWeightDistrib*>(curr_node)) cv_processor[curr_val].append(new FireWeightDistrib);
        else if (dynamic_cast<FireValidation*>   (curr_node)) cv_processor[curr_val].append(new FireValidation);
        else if (dynamic_cast<FlameSrcBBox*>     (curr_node)) cv_processor[curr_val].append(new FlameSrcBBox);
        else if (dynamic_cast<FireBBox*>         (curr_node)) cv_processor[curr_val].append(new FireBBox);
        createNewThread(cv_processor[curr_val].last());
    }
    curr_node = cv_processor[curr_val].last();
    connect(parent_node, SIGNAL(nextNode(CVProcessData)),   curr_node,   SLOT(process(CVProcessData)),   Qt::UniqueConnection);
    connect(curr_node,   SIGNAL(pushLog(QString,QString)),  logger_node, SLOT(add_log(QString,QString)), Qt::UniqueConnection);
    connect(parent_node, SIGNAL(destroyed()),               curr_node,   SLOT(deleteLater()));
    connections.push_back(conn);
}

void CVProcessManager::purpose(CVIONode* video_io, cv_node root_value) {
    CVProcessingNode* root_node = cv_processor[root_value].last();
    if (!processing_nodes.contains(root_node)) createNewThread(root_node);
    else if (root_node->averageTime() > 0.5 / max_fps) {
        if      (dynamic_cast<RFireMaskingModel*>(root_node)) cv_processor[root_value].append(new RFireMaskingModel);
        else if (dynamic_cast<YFireMaskingModel*>(root_node)) cv_processor[root_value].append(new YFireMaskingModel);
        else if (dynamic_cast<FireWeightDistrib*>(root_node)) cv_processor[root_value].append(new FireWeightDistrib);
        else if (dynamic_cast<FireValidation*>   (root_node)) cv_processor[root_value].append(new FireValidation);
        else if (dynamic_cast<FlameSrcBBox*>     (root_node)) cv_processor[root_value].append(new FlameSrcBBox);
        else if (dynamic_cast<FireBBox*>         (root_node)) cv_processor[root_value].append(new FireBBox);
        createNewThread(cv_processor[root_value].last());
    }
    root_node = cv_processor[root_value].last();
    connect(video_io,   SIGNAL(nextNode(CVProcessData)),  root_node,    SLOT(process(CVProcessData)));
    connect(root_node,  SIGNAL(pushLog(QString,QString)), logger_node,  SLOT(add_log(QString,QString)), Qt::UniqueConnection);
    connect(video_io,   SIGNAL(destroyed()),              root_node,    SLOT(deleteLater()));
}

void CVProcessManager::purpose_tree(CVProcessTree::Node* node) {
    for (CVProcessTree::Node* child : node->children) {
        purpose(node->value, child->value);
        purpose_tree(child);
    }
    if (node->parent != NULL)
        purpose(node->parent->value, node->value);
}

void CVProcessManager::purposeProcesses(
    QList<CVProcessTree*> processForest,
    CVIONode* video_io)
{
    for (CVProcessTree* processTree : processForest) {
        purpose(video_io, processTree->root->value);
        purpose_tree(processTree->root);
    }
    connect(video_io, SIGNAL(save_log(QString, int)), logger_node, SLOT(save_log(QString, int)));
}

void CVProcessManager::addNewStream(int devId, QList<CVProcessTree*> processForest) {
    CVIONode* io(new CVIONode(this, QString("device_") + QString::number(devId)));
    QThread* io_thread = new QThread(this);
    io->moveToThread(io_thread);
    connect(io_thread, SIGNAL(started()), io, SLOT(process()));
    connect(io, SIGNAL(EOS()), io_thread, SLOT(quit()));
    connect(io_thread, SIGNAL(finished()), io, SLOT(deleteLater()));
    max_fps = max_fps >= io->get_fps() ? max_fps : io->get_fps();
    purposeProcesses(processForest, io);
    io_thread->start();
}

void CVProcessManager::addNewStream(QString video_input, QString video_output, QList<CVProcessTree*> processForest) {
    CVIONode* io(new CVIONode(NULL, video_input, video_output));
    QThread* io_thread = new QThread;
    io->moveToThread(io_thread);
    connect(io_thread, SIGNAL(started()), io, SLOT(process()));
    connect(io, SIGNAL(EOS()), io_thread, SLOT(quit()));
    connect(io_thread, SIGNAL(finished()), io, SLOT(deleteLater()));
    max_fps = max_fps >= io->get_fps() ? max_fps : io->get_fps();
    purposeProcesses(processForest, io);
    io_thread->start();
}

void CVForestParser::recursive_parse(QJsonObject json_node, CVProcessTree::Node* node) {
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
    video_in = QString(json_obj["input"].toString());
    video_out = QString(json_obj["output"].toString());
    ip_addr = QString(json_obj["ip_address"].toString());
    ip_port = json_obj["ip_address"].toInt();
    display_debug = json_obj["display_debug"].toBool();

    QJsonArray forest = json_obj["process"].toArray();
    proc_forest.reserve(forest.size());
    for (QJsonValueRef tree : forest) {
        CVProcessTree::Node* root = new CVProcessTree::Node(NULL, get_node(QJsonValue(tree.toObject()["node"]).toString()));
        recursive_parse(tree.toObject(), root);
        proc_forest.push_back(new CVProcessTree(root));
    }
    return *this;
}
