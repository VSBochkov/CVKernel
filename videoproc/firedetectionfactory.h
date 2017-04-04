#ifndef FIREDETECTIONFACTORY_H
#define FIREDETECTIONFACTORY_H

#include "cvfactorycontroller.h"
#include "cvprocessmanager.h"
#include "cvgraphnode.h"
#include "rfiremaskingmodel.h"
#include "yfiremaskingmodel.h"
#include "firevalidation.h"
#include "firebbox.h"
#include "fireweight.h"
#include "flamesrcbbox.h"


struct FireDetectionFactory : public CVKernel::CVNodeFactory
{    
    FireDetectionFactory() {}
    virtual CVKernel::CVProcessingNode* create_processing_node(QString &node)
    {
        if      (node == "RFireMaskingModel")   return new RFireMaskingModel();
        else if (node == "YFireMaskingModel")   return new YFireMaskingModel();
        else if (node == "FireValidation")      return new FireValidation();
        else if (node == "FireWeightDistrib")   return new FireWeightDistrib();
        else if (node == "FireBBox")            return new FireBBox();
        else if (node == "FlameSrcBBox")        return new FlameSrcBBox();
        else                                    return nullptr;
    }

    virtual CVKernel::CVNodeParams* create_node_params(QJsonObject& json_obj)
    {
        QJsonObject::iterator iter = json_obj.find("node");
        if (iter == json_obj.end())
            return nullptr;

        QString class_name(iter.value().toString());

        if (class_name == "RFireMaskingModel") {
            return new RFireParams(json_obj);
        } else if (class_name == "YFireMaskingModel") {
            return new YFireParams(json_obj);
        } else if (class_name == "FireValidation") {
            return new FireValidationParams(json_obj);
        } else if (class_name == "FireWeightDistrib") {
            return new FireWeightDistribParams(json_obj);
        } else if (class_name == "FireBBox") {
            return new FireBBoxParams(json_obj);
        } else if (class_name == "FlameSrcBBox") {
            return new FlameSrcBBoxParams(json_obj);
        } else {
            return nullptr;
        }
    }

    virtual CVKernel::CVNodeHistory* create_history(QString& node) {
        if      (node == "RFireMaskingModel")   return new RFireHistory();
        else if (node == "YFireMaskingModel")   return new YFireHistory();
        else if (node == "FireValidation")      return new FireValidationHistory();
        else if (node == "FireWeightDistrib")   return new FireWeightDistribHistory();
        else if (node == "FireBBox")            return new FireBBoxHistory();
        else if (node == "FlameSrcBBox")        return new FlameSrcBBoxHistory();
        else                                    return nullptr;
    }
};

#endif // FIREDETECTIONFACTORY_H

