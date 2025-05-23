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
        std::cout << count << std::endl;

    std::unordered_map <std::string, int> nums;

    nums["vertices"] = num_vertices;
    nums["normals"] = num_normals;
    nums["uvs"] = num_uvs;
    nums["indices"] = count;

    return nums;
}

void OBJLoader::components_to_vec2s(std::vector<float> components, vsg::ref_ptr<vsg::vec2Array>& vecs) {
    for(size_t vec_start = 0; vec_start < components.size(); vec_start+=2) {
        vsg::vec2 normal(components[vec_start], -components[vec_start+1]);
        vecs->at(vec_start/2) = normal;
    }
}

void OBJLoader::components_to_vec3s(std::vector<float> components, vsg::ref_ptr<vsg::vec3Array>& vecs) {
    for(size_t vec_start = 0; vec_start < components.size(); vec_start+=3) {
        vsg::vec3 normal(components[vec_start], components[vec_start+1], components[vec_start+2]);
        vecs->at(vec_start/3) = normal;
    }
}

void OBJLoader::load_materials(const std::vector<tinyobj::material_t>& objmaterials,std::vector<vsg::ref_ptr<vsg::PbrMaterialValue>>& mat_val, std::vector<std::vector<std::string>>& textures){
    int i = 0;
    for(auto mat = objmaterials.begin(); mat < objmaterials.end(); ++mat) {
        size_t index = std::distance(objmaterials.begin(), mat);
        auto material = vsg::PbrMaterialValue::create();
        material->value().roughnessFactor = 1.f;
        material->value().metallicFactor = 0.8f;
        material->value().baseColorFactor = vsg::vec4{1.f, 1.f, 1.f, 1.0f};
        material->value().specularFactor = vsg::vec4{mat->specular[0], mat->specular[1], mat->specular[2], 1.0f};
        mat_val.push_back(material);
        std::cout << mat->diffuse_texname << std::endl;
        textures.push_back({mat->diffuse_texname, mat->normal_texname, mat->metallic_texname, mat->roughness_texname});
        i++;
    }
}

void OBJLoader::load_obj(const char* filename, const char* materials_path, vsg::ref_ptr<vsg::vec3Array>& vertices,
                         vsg::ref_ptr<vsg::vec3Array>& vertnormals, vsg::ref_ptr<vsg::vec2Array>& vertuvs,
                         vsg::ref_ptr<vsg::vec3Array>& colors, std::vector<vsg::ref_ptr<vsg::PbrMaterialValue>>& materials,
                         std::vector<std::vector<vsg::ref_ptr<vsg::uintArray>>>& indices, std::vector<std::vector<std::string>>& textures, std::vector<int>& mtr_ids)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> objmaterials;
    std::string warn, err;


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
    }
    if(attrib.texcoords.size() != 0) {
        components_to_vec2s(attrib.texcoords, vertuvs);
    }
    else if(attrib.colors.size() != 0) {
        components_to_vec3s(attrib.colors, colors);
    }

    std::cout << "Loading Material" << std::endl;
    if(objmaterials.size() != 0) {
        load_materials(objmaterials, materials, textures);
    }

    for(auto shape = shapes.begin(); shape < shapes.end(); shape++) {
        mtr_ids.push_back(shape->mesh.material_ids[0]);
        int count = 0;
        vsg::ref_ptr<vsg::uintArray> indice_pos = vsg::uintArray::create(shape->mesh.indices.size());
        vsg::ref_ptr<vsg::uintArray> indice_normal = vsg::uintArray::create(shape->mesh.indices.size());
        vsg::ref_ptr<vsg::uintArray> indice_coord = vsg::uintArray::create(shape->mesh.indices.size());
        for(auto index = shape->mesh.indices.begin(); index < shape->mesh.indices.end(); index++) {          
            indice_pos->set(count,index->vertex_index);
            indice_normal->set(count,index->normal_index);
            indice_coord->set(count,index->texcoord_index);
            count++;
        }
        indices.push_back({indice_pos, indice_normal, indice_coord});
    }

    std::cout << "Loaded materials." << std::endl;

    std::cout << "Loaded model " << filename << "." << std::endl;

}


