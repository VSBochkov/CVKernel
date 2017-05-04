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

CVKernel::CVProcessTree::CVProcessTree(CVProcessTree::Node* root_node) {
    root = root_node;
}

void CVKernel::CVProcessManager::create_new_thread(CVProcessingNode* node) {
    QThread* cv_thread = new QThread;
    node->moveToThread(cv_thread);
    processing_nodes.insert(node);
    QObject::connect(cv_thread, SIGNAL(finished()), node, SLOT(deleteLater()));
    cv_thread->start();
}

void CVKernel::CVProcessManager::purpose(CVProcessTree::Node* par, CVProcessTree::Node* curr)
{
    CVProcessingNode* parent_node = cv_processor[par->value].last();
    CVProcessingNode* curr_node;
    connection conn(parent_node, curr_node);

    if (cv_processor.find(curr->value) != cv_processor.end())
    {
        curr_node = cv_processor[curr->value].last();
        if (parent_node == curr_node || connections.contains(conn.get_reverse()))
            return;

        if (curr_node->get_average_time() > 0.5 / max_fps) {
            curr_node = CVFactoryController::get_instance().create_processing_node(curr->value);
            create_new_thread(curr_node);
        }
    } else {
        curr_node = CVFactoryController::get_instance().create_processing_node(curr->value);
        create_new_thread(curr_node);
    }

    if (curr_node != nullptr) {
        cv_processor[curr->value].push_back(curr_node);
    } else {
        curr_node = cv_processor[curr->value].last();
    }

    QObject::connect(parent_node, SIGNAL(next_node(QSharedPointer<CVProcessData>)), curr_node, SLOT(process(QSharedPointer<CVProcessData>)), Qt::UniqueConnection);
    QObject::connect(parent_node, SIGNAL(destroyed()), curr_node, SLOT(deleteLater()));

    connections.push_back(conn);
}

void CVKernel::CVProcessManager::purpose(CVIONode* video_io, CVProcessTree::Node* root)
{
    CVProcessingNode* root_node;

    if (cv_processor.find(root->value) != cv_processor.end()) {
        root_node = cv_processor[root->value].last();
        if (root_node->get_average_time() > 0.5 / max_fps) {
            root_node = CVFactoryController::get_instance().create_processing_node(root->value);
            create_new_thread(root_node);
        }
    } else {
        root_node = CVFactoryController::get_instance().create_processing_node(root->value);
        create_new_thread(root_node);
    }

    if (root_node != nullptr) {
        cv_processor[root->value].push_back(root_node);
    } else {
        root_node = cv_processor[root->value].last();
    }

    QObject::connect(video_io, SIGNAL(next_node(QSharedPointer<CVProcessData>)), root_node, SLOT(process(QSharedPointer<CVProcessData>)));
    //connect(video_io, SIGNAL(destroyed()), root_node, SLOT(deleteLater()));
    // TODO: make intelligent thread supervisor: if no one io node uses root processing node then delete all pipeline
}

void CVKernel::CVProcessManager::purpose_tree(CVIONode* video_io, CVProcessTree::Node* node)
{
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

CVKernel::CVIONode* CVKernel::CVProcessManager::add_new_stream(QSharedPointer<CVProcessForest> forest)
{
    QThread* io_thread = new QThread;

    CVIONode* io(forest->video_in.isEmpty() ?
        new CVIONode(forest->device_in, forest->video_out, forest->frame_width, forest->frame_height, forest->fps, forest->params_map) :
        new CVIONode(forest->video_in, forest->video_out, forest->frame_width, forest->frame_height, forest->fps, forest->params_map)
    );

    io->moveToThread(io_thread);
    QObject::connect(io_thread, SIGNAL(started()), io, SLOT(process()));
    QObject::connect(io, SIGNAL(node_closed()), io_thread, SLOT(quit()));
    QObject::connect(io_thread, SIGNAL(finished()), io, SLOT(deleteLater()));
    max_fps = max_fps >= io->get_fps() ? max_fps : io->get_fps();

    purpose_processes(forest->proc_forest, io);

    io_thread->start();
    return io;
}

QMap<QString, double> CVKernel::CVProcessManager::get_average_timings()
{
    QMap<QString, double> timings;
    for (CVProcessingNode* node: processing_nodes)
    {
        timings[node->metaObject()->className()] = node->get_average_time();
    }
    return timings;
}
