#include "assimp.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <vector>

bool importMeshPly(const std::string& pFile, 
    SimpleMesh &mesh)
{
    Assimp::Importer importer;
    // And have it read the given file with some example postprocessing
    // Usually - if speed is not the most important aspect for you - you'll
    // probably to request more postprocessing than we do in this example.
    const aiScene* scene = importer.ReadFile(pFile,
                                             aiProcess_Triangulate | aiProcess_GenSmoothNormals);

    // If the import failed, report it
    if (nullptr == scene)
    {
        return false;
    }

    auto aiMesh = scene->mMeshes[0];
    vsg::ref_ptr<vsg::vec3Array> vsgVertices = vsg::vec3Array::create(aiMesh->mNumVertices);
    vsg::ref_ptr<vsg::vec3Array> vsgNormals = vsg::vec3Array::create(aiMesh->mNumVertices);
    vsg::ref_ptr<vsg::uintArray> vsgIndices = vsg::uintArray::create(aiMesh->mNumFaces * 3);
    for (uint32_t i = 0; i < aiMesh->mNumVertices; i++)
    {
        vsgVertices->set(i, vsg::vec3(aiMesh->mVertices[i].x, aiMesh->mVertices[i].y, aiMesh->mVertices[i].z));
        vsgNormals->set(i, vsg::vec3(aiMesh->mNormals[i].x, aiMesh->mNormals[i].y, aiMesh->mNormals[i].z));
    }
    for(uint32_t i = 0; i < aiMesh->mNumFaces; i++)
    {
        if(aiMesh->mFaces[i].mNumIndices != 3) {
            return false;
        }
        vsgIndices->set(3u * i,   aiMesh->mFaces[i].mIndices[0]);
        vsgIndices->set(3u * i+1, aiMesh->mFaces[i].mIndices[1]);
        vsgIndices->set(3u * i+2, aiMesh->mFaces[i].mIndices[2]);
    }
    mesh.numVertices = aiMesh->mNumVertices;
    mesh.vertices = vsgVertices;
    mesh.normals = vsgNormals;
    mesh.numIndices = aiMesh->mNumFaces * 3;
    mesh.indices = vsgIndices;
    return true;
};