#pragma once
#include <vsg/all.h>

struct SimpleMesh
{
    uint32_t numVertices, numIndices;
    vsg::ref_ptr<vsg::vec3Array> vertices;
    vsg::ref_ptr<vsg::vec3Array> normals;
    vsg::ref_ptr<vsg::uintArray> indices;
};
bool importMeshPly(const std::string& pFile, SimpleMesh& mesh);