#pragma  once
#include "CADMesh.h"

class ModelInstance{
public:
    std::unordered_map<std::string, treeNode> nodePtr;        //��¼ID��Ӧ�Ľڵ�
    std::unordered_map<std::string, int> instanceIndex;     

    void buildInstance(CADMesh* mesh, vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::ShaderSet> shader, const vsg::dmat4& modelMatrix);
    void buildObjInstance(CADMesh* mesh, vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::ShaderSet> shader, const vsg::dmat4& modelMatrix);
};