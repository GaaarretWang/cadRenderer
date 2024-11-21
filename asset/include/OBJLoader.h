#ifndef OBJLOADER_H
#define OBJLOADER_H

#include <iostream>
#include <unordered_map>
#include <vsg/all.h>

// 前置声明
namespace tinyobj {
    struct material_t;
}

class OBJLoader
{
private:
    void components_to_vec2s(std::vector<float> components, vsg::ref_ptr<vsg::vec2Array>& vecs);
    void components_to_vec3s(std::vector<float> components, vsg::ref_ptr<vsg::vec3Array>& vecs);
    void load_materials(const std::vector<tinyobj::material_t>& objmaterials,vsg::ref_ptr<vsg::PhongMaterialArray>& mat_val);
public:
    std::unordered_map<std::string, int> vertex_count(const char* filename);
    void load_obj(const char* filename, const char* materials_path, vsg::ref_ptr<vsg::vec3Array>& vertices, vsg::ref_ptr<vsg::vec3Array>& vertnormals, vsg::ref_ptr<vsg::vec2Array>& vertuvs, vsg::ref_ptr<vsg::vec3Array>& colors, vsg::ref_ptr<vsg::PhongMaterialArray>& materials,vsg::ref_ptr<vsg::uintArray>& indices);
};

#endif