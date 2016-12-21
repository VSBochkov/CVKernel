#ifndef CVPROCESSMANAGER_H
#define CVPROCESSMANAGER_H

#include <QObject>
#include <QMap>
#include <QSet>

namespace CVKernel {
    class CVProcessingNode;
    class CVLoggerNode;
    class CVIONode;

    enum cv_node {
        nothing = -1,
        r_firemm,
        y_firemm,
        fire_valid,
        fire_bbox,
        fire_weights,
        flame_src_bbox
    };

    struct CVProcessTree {
        struct Node {
            Node* parent;
            cv_node value;
            bool draw_overlay;
            bool ip_deliver;
            QList<Node*> children;
            Node(Node* par = NULL, cv_node val = nothing, bool draw = false, bool ip_deliver = false):
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

    class CVForestParser {
    public:
        CVForestParser& parseJSON(QString json_fname);
        QList<CVProcessTree*> get_forest() { return proc_forest; }
        QString get_video_in() { return video_in; }
        QString get_video_out() { return video_out; }
    private:
        void recursive_parse(QJsonObject json_node, CVProcessTree::Node* node);
        cv_node get_node(QString node) {
            if (node == "r_firemm")             return r_firemm;
            else if (node == "y_firemm")        return y_firemm;
            else if (node == "fire_valid")      return fire_valid;
            else if (node == "fire_bbox")       return fire_bbox;
            else if (node == "fire_weights")    return fire_weights;
            else if (node == "flame_src_bbox")  return flame_src_bbox;
            else                                return nothing;
        }
    public:
        QList<CVProcessTree*> proc_forest;
        int     device_in;
        QString video_in;
        QString video_out;
        QString ip_addr;
        int     ip_port;
        bool    show_overlay;
        double  proc_frame_scale;
    };

    class CVProcessManager : QObject {
        Q_OBJECT
    public:
        explicit CVProcessManager(QObject *parent = 0);
        virtual ~CVProcessManager();
        void addNewStream(CVForestParser &forest);

        void purposeProcesses(
            QList<CVProcessTree*> processForest,
            CVIONode* video_io
        );

    public slots:
        void close_threads();

    private:
        void createNewThread(CVProcessingNode* node);
        void addProcessingNode(CVProcessTree::Node* node);
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
        QMap<cv_node, QList<CVProcessingNode*>> cv_processor;
        QSet<CVProcessingNode*> processing_nodes;
        double max_fps;
    };

}


#endif // CVPROCESSMANAGER_H
