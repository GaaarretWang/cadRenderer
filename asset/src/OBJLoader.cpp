#include "OBJLoader.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

std::unordered_map<std::string, int> OBJLoader::vertex_count(const char* filename){
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> objmat_vals;
    std::string warn;
    std::string err;

    bool success = tinyobj::LoadObj(&attrib, &shapes, &objmat_vals, &warn, &err,
        filename, //model to load
        "", //directory to search for mat_vals
        true); //enable triangulation

    if(!success){
        std::cerr << err << std::endl;
        assert(success);
    }


    int num_vertices = attrib.vertices.size()/3;
    int num_normals = attrib.normals.size()/3;
    int num_uvs = attrib.texcoords.size()/2;
    std::cout << "num_vertices" << num_vertices << std::endl;
    std::cout << "num_normals" << num_normals << std::endl;
    int count = 0;
    for(auto shape = shapes.begin(); shape < shapes.end(); shape++) {
        for(auto index = shape->mesh.indices.begin(); index < shape->mesh.indices.end(); index++) {
            //indices->set(count,index->vertex_index);
            count++;
        }
    }

    std::unordered_map <std::string, int> nums;

    nums["vertices"] = num_vertices;
    nums["normals"] = num_normals;
    nums["uvs"] = num_uvs;
    nums["indices"] = count;

    return nums;
}

void OBJLoader::components_to_vec2s(std::vector<float> components, vsg::ref_ptr<vsg::vec2Array>& vecs) {
    for(size_t vec_start = 0; vec_start < components.size(); vec_start+=2) {
        vecs->at(vec_start/2) = vsg::vec2(components[vec_start], components[vec_start+1]
        );
    }
}

void OBJLoader::components_to_vec3s(std::vector<float> components, vsg::ref_ptr<vsg::vec3Array>& vecs) {
    for(size_t vec_start = 0; vec_start < components.size(); vec_start+=3) {
        vsg::vec3 normal(components[vec_start], components[vec_start+1], components[vec_start+2]);
        vecs->at(vec_start/3) = normal;
    }
}

void OBJLoader::load_materials(const std::vector<tinyobj::material_t>& objmaterials,vsg::ref_ptr<vsg::PhongMaterialArray>& mat_val){
    for(auto mat = objmaterials.begin(); mat < objmaterials.end(); ++mat) {
        size_t index = std::distance(objmaterials.begin(), mat);
        mat_val->set(index,
            vsg::PhongMaterial{
                vsg::vec4{mat->ambient[0], mat->ambient[1], mat->ambient[2], 1.0f},
                vsg::vec4{mat->diffuse[0], mat->diffuse[1], mat->diffuse[2], 1.0f},
                vsg::vec4{mat->specular[0], mat->specular[1], mat->specular[2], 1.0f},
                vsg::vec4{mat->emission[0], mat->emission[1], mat->emission[2], 1.0f},
                mat->shininess
            }
        );
    }

}

void OBJLoader::load_obj(const char* filename, const char* materials_path,vsg::ref_ptr<vsg::vec3Array> &vertices, vsg::ref_ptr<vsg::vec3Array>& vertnormals, vsg::ref_ptr<vsg::vec2Array>& vertuvs, vsg::ref_ptr<vsg::vec3Array>& colors, vsg::ref_ptr<vsg::PhongMaterialArray>& materials,vsg::ref_ptr<vsg::uintArray>& indices) {

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> objmaterials;
    std::string warn;
    std::string err;


    //load all data in Obj file
    //'triangulate'
    bool success = tinyobj::LoadObj(&attrib, &shapes, &objmaterials, &warn, &err,
        filename, //model to load
        materials_path, //directory to search for mat_vals
        true); //enable triangulation

    //boilerplate error handling
    if (!err.empty()) {
        std::cerr << err << std::endl;
    }
    if (!success) {
        exit(1);
    }
    
    

    if(attrib.vertices.size() != 0) {
        components_to_vec3s(attrib.vertices, vertices);
    }
    if(attrib.normals.size() != 0) {
        components_to_vec3s(attrib.normals, vertnormals);//不单独拿出来就不执行,不加上述if else会超限segmentation fault (core dumped)
    }else if(attrib.texcoords.size() != 0) {
        components_to_vec2s(attrib.texcoords, vertuvs);
    }else if(attrib.colors.size() != 0) {
        components_to_vec3s(attrib.colors, colors);
    }


    std::cout << "Loaded vertices." << std::endl;

    int count = 0;
    for(auto shape = shapes.begin(); shape < shapes.end(); shape++) {
        for(auto index = shape->mesh.indices.begin(); index < shape->mesh.indices.end(); index++) {
            indices->set(count,index->vertex_index);
            count++;
        }
    }

    if(objmaterials.size() != 0) {
        load_materials(objmaterials, materials);
    }

    std::cout << "Loaded materials." << std::endl;

    std::cout << "Loaded model " << filename << "." << std::endl;

}


