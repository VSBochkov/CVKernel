#ifndef CVPROCESSMANAGER_H
#define CVPROCESSMANAGER_H

#include <QObject>
#include <QMap>
#include <QSet>
#include <QList>

#include <QTcpSocket>
#include <QString>
#include <QHostAddress>
#include <QSharedPointer>

namespace CVKernel {
    class CVIONode;
    class CVNodeParams;
    class CVProcessingNode;
    class CVNetworkManager;

    struct CVProcessTree {
        struct Node {
            Node* parent;
            QString value;
            bool draw_overlay;
            bool ip_deliver;
            QList<Node*> children;
            Node(Node* par = NULL, QString val = "", bool draw = false, bool ip_deliver = false):
                parent(par), value(val), draw_overlay(draw), ip_deliver(ip_deliver) {
                children = QList<Node*>();
                if (parent != NULL)
                    parent->children.push_back(this);
            }
        }* root;

        CVProcessTree(Node* root_node);
        void delete_tree(Node* node) {
            for (CVProcessTree::Node* child : node->children)
                delete_tree(child);
            delete node;
        }
        ~CVProcessTree() { delete_tree(root); }
    };

    class CVProcessForest {
    public:
        CVProcessForest(QJsonObject& json_obj);
        ~CVProcessForest() {
            for (CVKernel::CVProcessTree* tree : proc_forest) {
                delete tree;
            }
            proc_forest.clear();
        }

    private:
        void recursive_parse(QJsonObject json_node, CVProcessTree::Node* node);
        CVProcessTree::Node* create_node(QJsonObject& json_node, CVProcessTree::Node* parent = NULL);

    public:
        QList<CVProcessTree*> proc_forest;
        QMap<QString, QSharedPointer<CVNodeParams>> params_map;
        int     device_in;
        QString video_in;
        QString video_out;
        QHostAddress meta_udp_addr;
        int     meta_udp_port;
        bool    show_overlay;
        bool    store_output;
        int     frame_width;
        int     frame_height;
        double  fps;
    };

    class CVProcessManager
    {
    public:
        QMap<QString, double> get_average_timings();

        void purpose_processes(
            QList<CVProcessTree*> processForest,
            CVIONode* video_io
        );

        CVIONode* add_new_stream(QSharedPointer<CVProcessForest> proc_forest);

    private:
        void parse_cv_kernel_settings_json(QString json_fname);
        void create_new_thread(CVProcessingNode* node);
        void purpose(CVProcessTree::Node* parent, CVProcessTree::Node* curr);
        void purpose(CVIONode* video_io, CVProcessTree::Node* root);
        void purpose_tree(CVIONode* video_io, CVProcessTree::Node* node);

    private:
        struct connection {
            CVProcessingNode* reciever;
            CVProcessingNode* sender;
            connection(CVProcessingNode* r, CVProcessingNode* s) : reciever(r), sender(s) {}
            connection get_reverse() { return connection(sender, reciever); }

            friend bool operator==(const connection& left, const connection& right) {
                return left.reciever == right.reciever && left.sender == right.sender;
            }
        };
        QList<connection> connections;
        QMap<QString, QList<CVProcessingNode*>> cv_processor;

    public:
        QSet<CVProcessingNode*> processing_nodes;
        double max_fps;
    };

}


#endif // CVPROCESSMANAGER_H
