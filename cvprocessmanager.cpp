#include "cvprocessmanager.h"
#include "cvnetworkmanager.h"
#include "cvgraphnode.h"
#include "cvfactorycontroller.h"

#include <QObject>
#include <QTimer>
#include <QThread>

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include <iostream>


CVKernel::CVProcessManager::CVProcessManager(QObject* parent, CVNetworkManager* net_manager) : QObject(parent), network_manager(net_manager)
{
    connect(network_manager, SIGNAL(got_process_forest(QTcpSocket*,QSharedPointer<CVProcessForest>)), this, SLOT(add_new_stream(QTcpSocket*,QSharedPointer<CVProcessForest>)));
    connect(network_manager, SIGNAL(run_io_node(CVIONode*)), this, SLOT(start_stream(CVIONode*)));
    connect(network_manager, SIGNAL(stop_io_node(CVIONode*)), this, SLOT(stop_stream(CVIONode*)));
    connect(network_manager, SIGNAL(close_io_node(CVIONode*)), this, SLOT(close_stream(CVIONode*)));
    connect(network_manager, SIGNAL(udp_closed(CVIONode*)), this, SLOT(on_udp_closed(CVIONode*)));
}

void CVKernel::CVProcessManager::close_threads() {}

CVKernel::CVProcessManager::~CVProcessManager() {
    close_threads();
}

CVKernel::CVProcessTree::CVProcessTree(CVProcessTree::Node* root_node) {
    root = root_node;
}

void CVKernel::CVProcessManager::create_new_thread(CVProcessingNode* node) {
    QThread* cv_thread = new QThread;
    node->moveToThread(cv_thread);
    processing_nodes.insert(node);
    connect(node, SIGNAL(destroyed()), cv_thread, SLOT(quit()));
    cv_thread->start();
}

void CVKernel::CVProcessManager::purpose(CVProcessTree::Node* par, CVProcessTree::Node* curr) {
    CVProcessingNode* parent_node = cv_processor[par->value].last();
    CVProcessingNode* curr_node;
    connection conn(parent_node, curr_node);

    if (cv_processor.find(curr->value) != cv_processor.end())
    {
        curr_node = cv_processor[curr->value].last();
        if (parent_node == curr_node || connections.contains(conn.get_reverse()))
            return;

        if (curr_node->averageTime() > 0.5 / max_fps) {
            curr_node = CVFactoryController::get_instance().create_processing_node(curr->value);
        }
    } else {
        curr_node = CVFactoryController::get_instance().create_processing_node(curr->value);
    }

    if (curr_node != nullptr) {
        cv_processor[curr->value].push_back(curr_node);
        create_new_thread(curr_node);
    } else {
        curr_node = cv_processor[curr->value].last();
    }

    connect(parent_node, SIGNAL(nextNode(QSharedPointer<CVProcessData>, CVIONode*)), curr_node, SLOT(process(QSharedPointer<CVProcessData>, CVIONode*)), Qt::UniqueConnection);
    connect(parent_node, SIGNAL(destroyed()), curr_node, SLOT(deleteLater()));

    connections.push_back(conn);
}

void CVKernel::CVProcessManager::purpose(CVIONode* video_io, CVProcessTree::Node* root) {
    CVProcessingNode* root_node;

    if (cv_processor.find(root->value) != cv_processor.end()) {
        root_node = cv_processor[root->value].last();
        if (root_node->averageTime() > 0.5 / max_fps) {
            root_node = CVFactoryController::get_instance().create_processing_node(root->value);
        }
    } else {
        root_node = CVFactoryController::get_instance().create_processing_node(root->value);
    }

    if (root_node != nullptr) {
        cv_processor[root->value].push_back(root_node);
        create_new_thread(root_node);
    } else {
        root_node = cv_processor[root->value].last();
    }

    connect(video_io, SIGNAL(nextNode(QSharedPointer<CVProcessData>, CVIONode*)), root_node, SLOT(process(QSharedPointer<CVProcessData>, CVIONode*)));
    connect(video_io, SIGNAL(destroyed()), root_node, SLOT(deleteLater()));
}

void CVKernel::CVProcessManager::purpose_tree(CVIONode* video_io, CVProcessTree::Node* node) {
    for (CVProcessTree::Node* child : node->children) {
        purpose(node, child);
        purpose_tree(video_io, child);
        video_io->nodes_number++;
    }
}

void CVKernel::CVProcessManager::purpose_processes(
    QList<CVProcessTree*> processForest,
    CVIONode* video_io)
{
    for (CVProcessTree* processTree : processForest) {
        purpose(video_io, processTree->root);
        purpose_tree(video_io, processTree->root);
        video_io->nodes_number++;
    }
}

void CVKernel::CVProcessManager::add_new_stream(QTcpSocket* socket, QSharedPointer<CVProcessForest> forest) {
    CVIONode* io(forest->video_in.isEmpty() ?
        new CVIONode(forest->device_in, forest->video_out, forest->frame_width, forest->frame_height, forest->fps, forest->params_map) :
        new CVIONode(forest->video_in, forest->video_out, forest->frame_width, forest->frame_height, forest->fps, forest->params_map)
    );

    QThread* io_thread = new QThread(this);

    io->moveToThread(io_thread);

    connect(io_thread, SIGNAL(started()), io, SLOT(process()));
    connect(io, SIGNAL(node_closed()), io_thread, SLOT(quit()));
    connect(io_thread, SIGNAL(finished()), io, SLOT(deleteLater()));

    connect(io, SIGNAL(node_started()), this, SLOT(on_stream_started()));
    connect(io, SIGNAL(node_stopped()), this, SLOT(on_stream_stopped()));
    connect(io, SIGNAL(node_closed()), this, SLOT(on_stream_closed()));

    max_fps = max_fps >= io->get_fps() ? max_fps : io->get_fps();

    purpose_processes(forest->proc_forest, io);

    io_thread->start();

    emit io_node_created(socket, io);
}

QMap<QString, double> CVKernel::CVProcessManager::get_average_timings()
{
    QMap<QString, double> timings;
    for (CVProcessingNode* node: processing_nodes)
    {
        timings[node->metaObject()->className()] = node->averageTime();
    }
    return timings;
}

void CVKernel::CVProcessManager::start_stream(CVIONode* io_node)
{
    io_node->start();
}

void CVKernel::CVProcessManager::on_stream_started()
{
    CVIONode* io_node = (CVIONode*) sender();
    emit io_node_started(io_node);
}

void CVKernel::CVProcessManager::stop_stream(CVIONode* io_node)
{
    io_node->stop();
}

void CVKernel::CVProcessManager::on_stream_stopped()
{
    CVIONode* io_node = (CVIONode*) sender();
    emit io_node_stopped(io_node);
}

void CVKernel::CVProcessManager::close_stream(CVIONode* io_node)
{
    io_node->close();
}

void CVKernel::CVProcessManager::on_udp_closed(CVIONode* io_node)
{
    io_node->udp_closed();
}

void CVKernel::CVProcessManager::on_stream_closed()
{
    CVIONode* io_node = (CVIONode*) sender();
    emit io_node_closed(io_node);
    io_node->thread()->exit();
}
