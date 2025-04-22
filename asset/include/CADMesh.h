#pragma  once
#include "renderGeo_generated.h"
#include <iostream>
#include <unordered_set>
#include <vsg/all.h>
#include <vsgXchange/all.h>
#include "communication/dataInterface.h"
#include "OBJLoader.h"
#define EXPLODE

struct RGB
{
    float r, g, b;
};

struct Line
{
    vsg::vec3 begin;
    vsg::vec3 end;

    Line(const vsg::vec3& beginPoint, const vsg::vec3& endPoint) :
        begin(beginPoint), end(endPoint)
    {
    }
};

struct MaterialObj
{
    // Structure holding the material
    vsg::vec3 ambient = vsg::vec3(0.1f, 0.1f, 0.1f);
    vsg::vec3 diffuse = vsg::vec3(0.7f, 0.f, 0.f);
    vsg::vec3 specular = vsg::vec3(1.0f, 1.0f, 1.0f);
    vsg::vec3 transmittance = vsg::vec3(0.0f, 0.0f, 0.0f);
    vsg::vec3 emission = vsg::vec3(0.0f, 0.0f, 0.0f);
    float shininess = 0.f;
    float ior = 1.0f;     // index of refraction
    float dissolve = 1.f; // 1 == opaque; 0 == fully transparent

    int illum = 0;
    int diffuseTextureID = -1;
};

struct TinyModelVertex
{
    TinyModelVertex()
    {
        pos = vsg::vec3(0.0f, 0.0f, 0.0f);
        uv = vsg::vec2(0.0f, 0.0f);
        normal = vsg::vec3(0.0f, 0.0f, 0.0f);
        tangent = vsg::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    };
    TinyModelVertex(vsg::vec3 _pos, vsg::vec2 _uv, vsg::vec3 _normal, vsg::vec4 _tangent)
    {
        pos = _pos;
        uv = _uv;
        normal = _normal;
        tangent = _tangent;
    }
    vsg::vec3 pos;
    vsg::vec2 uv;
    vsg::vec3 normal;
    vsg::vec4 tangent;

    bool operator==(const TinyModelVertex& other) const
    {
        return pos == other.pos && uv == other.uv && normal == other.normal;
    }
};

struct treeNode
{
    std::vector<std::string> kids;
    vsg::ref_ptr<vsg::MatrixTransform> transform;
    vsg::mat4 originalMatrix;
    vsg::dbox bounds;
};

struct ProtoData
{
    vsg::dbox bounds;
    std::string proto_id = "";
    vsg::ref_ptr<vsg::vec3Array> vertices;
    vsg::ref_ptr<vsg::vec3Array> normals;
    vsg::ref_ptr<vsg::vec2Array> uvs;
    vsg::ref_ptr<vsg::uintArray> indices;
    std::string diffuse_path = "";
    std::string normal_path = "";
    std::string mr_path = "";
    vsg::ref_ptr<vsg::PbrMaterialValue> material;
    vsg::ref_ptr<vsg::mat4Array> instance_buffer;
    std::vector<std::string> instance_id;
    std::vector<vsg::dmat4> instance_matrix;
    vsg::ref_ptr<vsg::ShaderSet> shaderset;
    vsg::ref_ptr<vsg::Group> scene;
    vsg::ref_ptr<vsg::DrawIndexedIndirect> draw_indirect;
    vsg::ref_ptr<vsg::BufferInfo> full_buffer;
};

struct MatrixIndex
{
    ProtoData* proto_data;
    int index;

    MatrixIndex(ProtoData* proto_data, int index): proto_data(proto_data), index(index){}
};

namespace std
{
    template<typename T>
    struct hash<vsg::t_vec2<T>>
    {
        std::size_t operator()(const vsg::t_vec2<T>& vec) const noexcept
        {
            std::hash<T> hasher;
            std::size_t seed = 0;
            for (size_t i = 0; i < 2; ++i)
            {
                seed ^= hasher(vec[i]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            }
            return seed;
        }
    };
    template<typename T>
    struct hash<vsg::t_vec3<T>>
    {
        std::size_t operator()(const vsg::t_vec3<T>& vec) const noexcept
        {
            std::hash<T> hasher;
            std::size_t seed = 0;
            for (size_t i = 0; i < 3; ++i)
            {
                seed ^= hasher(vec[i]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            }
            return seed;
        }
    };
    template<typename T>
    struct hash<vsg::t_vec4<T>>
    {
        std::size_t operator()(const vsg::t_vec4<T>& vec) const noexcept
        {
            std::hash<T> hasher;
            std::size_t seed = 0;
            for (size_t i = 0; i < 4; ++i)
            {
                seed ^= hasher(vec[i]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            }
            return seed;
        }
    };
    template<>
    struct hash<TinyModelVertex>
    {
        size_t operator()(TinyModelVertex const& vertex) const
        {
            return ((hash<vsg::vec3>()(vertex.pos) ^
                     (hash<vsg::vec3>()(vertex.normal) << 1)) >>
                    1) ^
                   (hash<vsg::vec2>()(vertex.uv) << 1);
        }
    };
} // namespace std

class CADMesh
{
private:
    template<typename T>
    vsg::vec3 toVec3(const flatbuffers::Vector<T>* flat_vector, int begin = 0);
    vsg::vec3 toVec3(const flatbuffers::String* string_vector);
    template<typename T>
    vsg::vec3 toNewVec3(std::vector<T>* flat_vector, int begin);
    template<typename T>
    vsg::vec2 toNewVec2(std::vector<T>* flat_vector, int begin);
    template<typename T>
    vsg::vec2 toVec2(const flatbuffers::Vector<T>* flat_vector, int begin = 0);
    RGB hexToRGB(const std::string& color);

    std::vector<MaterialObj> mMaterials{};
    std::vector<std::string> mTextures{};
    std::vector<uint32_t> mMatIndex{};
    std::unordered_map<std::string, uint32_t> uniqueMaterials; //pos,normal,uv...

public:
    int Nodenumber;
    int Triangnumber;
    int countnum = 0;

    flatbuffers::FlatBufferBuilder builder_out;
    std::unordered_map<std::string, uint32_t> protoIndex;
    std::unordered_map<std::string, uint32_t> protoTriangleNum;
    std::unordered_map<int, uint32_t> materialIndex;
    // std::unordered_map<std::string, int> instanceIndex;     
    // std::unordered_map<std::string, treeNode> nodePtr;        //

    std::vector<vsg::ref_ptr<vsg::vec3Array>> objVerticesVector;
    std::vector<vsg::ref_ptr<vsg::vec3Array>> objNormalsVector;
    std::vector<vsg::ref_ptr<vsg::vec2Array>> objUVVector;
    std::vector<std::vector<std::string>> objTexturePath;
    std::vector<vsg::ref_ptr<vsg::uintArray>> objIndicesVector;
    std::vector<std::vector<int>> objMaterialIndice;
    std::vector<vsg::ref_ptr<vsg::PbrMaterialArray>> objMaterialVector;

    std::vector<cadDataManager::pmiInfo> pmi;
    std::vector<vsg::ref_ptr<vsg::vec3Array>> verticesVector;
    std::vector<vsg::ref_ptr<vsg::vec3Array>> normalsVector;
    std::vector<vsg::ref_ptr<vsg::vec2Array>> UVVector;
    std::vector<vsg::ref_ptr<vsg::vec2Array>> coordinatesVector;
    std::vector<vsg::ref_ptr<vsg::uintArray>> indicesVector;
    std::vector<vsg::ref_ptr<vsg::PbrMaterialValue>> materialVector;
    std::vector<std::string> materialNameVector;
    std::vector<std::vector<float>> transformVector;
    std::vector<int> transformNumVector;
    std::unordered_map<std::string, int> meshIndice; //存储零件名和它在数组中的位置，便于查找
    
    cadDataManager::RenderInfo info;

    vsg::GeometryInfo geomInfo;
    vsg::StateInfo stateInfo;
    vsg::ref_ptr<vsg::Builder> builder = vsg::Builder::create();
    vsg::ref_ptr<vsg::Options> options = vsg::Options::create();
    std::vector<Line> lines;
    std::vector<vsg::vec3> positions;
    std::unordered_map<vsg::vec3, uint32_t> positionToIndex;
    std::vector<uint32_t> indices;

    vsg::ref_ptr<vsg::StateGroup> stateGroup_total = vsg::StateGroup::create();
    
    static vsg::ImageInfoList camera_info;
    static vsg::ImageInfoList depth_info;
    static vsg::ref_ptr<vsg::Data> params;
    static std::unordered_map<std::string, vsg::ImageInfoList> texture_name_to_image_map;
    static std::unordered_map<std::string, ProtoData*> proto_id_to_data_map;
    static std::vector<vsg::ref_ptr<vsg::PbrMaterialValue>> materials;

    static std::unordered_map<std::string, std::vector<MatrixIndex>> id_to_matrix_index_map;

    std::vector<std::string> proto_ids;
    std::unordered_map<std::string, std::vector<vsg::dmat4>> proto_id_default_matrix_map;
    std::unordered_map<std::string, std::vector<std::string>> proto_id_instance_name_map;


    //����ģ��
    void buildNewNode(const std::string& path, bool fullNormal, vsg::ref_ptr<vsg::Group> scene);
    void buildObjNode(const char* path, const char* material_path, const vsg::dmat4& modelMatrix, vsg::ref_ptr<vsg::ShaderSet> model_shaderset, vsg::ref_ptr<vsg::Group> scene);
    static void CreateDefaultMaterials();
    void preprocessProtoData(const char* model_path, const char* material_path, const vsg::dmat4& modelMatrix, vsg::ref_ptr<vsg::ShaderSet> model_shaderset, vsg::ref_ptr<vsg::Group> scene, std::string instance_name);
    void preprocessFBProtoData(const std::string model_path, const char* material_path, const vsg::dmat4& modelMatrix, vsg::ref_ptr<vsg::ShaderSet> model_shaderset, vsg::ref_ptr<vsg::Group> scene, std::string instance_name);
    static void buildDrawData(vsg::ref_ptr<vsg::ShaderSet> model_shaderset, vsg::ref_ptr<vsg::Group> scene);
};