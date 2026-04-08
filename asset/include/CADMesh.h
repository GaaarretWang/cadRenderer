#pragma  once
#include "renderGeo_generated.h"
#include <iostream>
#include <unordered_set>
#include <vsg/all.h>
#include "communication/dataInterface.h"
#include "OBJLoader.h"

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
        color = vsg::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        tangent = vsg::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    };
    TinyModelVertex(vsg::vec3 _pos, vsg::vec2 _uv, vsg::vec3 _normal, vsg::vec4 _color, vsg::vec4 _tangent)
    {
        pos = _pos;
        uv = _uv;
        normal = _normal;
        color = _color;
        tangent = _tangent;
    }
    vsg::vec3 pos;
    vsg::vec2 uv;
    vsg::vec3 normal;
    vsg::vec4 color;
    vsg::vec4 tangent;

    bool operator==(const TinyModelVertex& other) const
    {
        return pos == other.pos && uv == other.uv && normal == other.normal && color == other.color;
    }
};

struct ProtoData
{
    enum class MaterialSource
    {
        Obj,
        Fb
    };

    vsg::ref_ptr<vsg::BufferInfo> bounds_buffer_info;
    vsg::ref_ptr<vsg::vec4Array> bounds_data;
    std::string proto_id = "";
    vsg::ref_ptr<vsg::vec3Array> vertices;
    vsg::ref_ptr<vsg::vec3Array> normals;
    vsg::ref_ptr<vsg::vec4Array> colors;
    vsg::ref_ptr<vsg::vec2Array> uvs;
    vsg::ref_ptr<vsg::uintArray> indices;
    std::string diffuse_path = "";
    std::string normal_path = "";
    std::string mr_path = "";
    uint32_t material_index;  // Index into global_material_array
    MaterialSource material_source = MaterialSource::Obj;
    std::string material_persist_key;
    std::string fb_color_group_key;
    vsg::ref_ptr<vsg::mat4Array> instance_buffer;
    vsg::ref_ptr<vsg::mat4Array> last_instance_buffer; // Proto matrices from the previous frame.
    vsg::ref_ptr<vsg::BufferInfo> input_instance_buffer_info;
    vsg::ref_ptr<vsg::BufferInfo> last_instance_buffer_info; // BufferInfo for the previous-frame proto matrices.
    vsg::ref_ptr<vsg::uintArray> highlight_buffer;
    vsg::ref_ptr<vsg::BufferInfo> input_highlight_buffer_info;
    vsg::ref_ptr<vsg::BufferInfo> output_instance_buffer_info;
    std::vector<std::string> instance_id;
    std::vector<vsg::dmat4> instance_matrix;
    std::vector<uint32_t> instance_model_indices;  // Global model-matrix index for each instance.
    vsg::ref_ptr<vsg::ShaderSet> shaderset;
    vsg::ref_ptr<vsg::Group> scene;
    vsg::ref_ptr<vsg::DrawIndexedIndirect> draw_indirect;
    vsg::ref_ptr<vsg::BufferInfo> indirect_full_buffer_info;
    bool back_cull;
};

struct DynamicLines
{
    vsg::ref_ptr<vsg::vec3Array> vertices;
    vsg::ref_ptr<vsg::uintArray> indices;
    vsg::ref_ptr<vsg::vec4Value> colors;
};

struct DynamicPoints
{
    vsg::ref_ptr<vsg::vec3Array> vertices;
    vsg::ref_ptr<vsg::uintArray> indices;
    vsg::ref_ptr<vsg::vec4Value> colors;
};

struct DynamicTexts
{
    std::vector<vsg::ref_ptr<vsg::stringValue>> dynamic_text_labels;
    std::vector<vsg::ref_ptr<vsg::StandardLayout>> standardLayout;
    std::vector<vsg::ref_ptr<vsg::Text>> text;
};

struct PMITransformNode
{
    vsg::ref_ptr<vsg::MatrixTransform> transform;
    uint32_t model_index;
    vsg::dmat4 instance_matrix;
    bool is_text;
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

struct GlobalConstantData{
    float z_far;
    int width;
    int height;
    int enable_real_depth_occlusion;
    int shadow_mode;
};

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
    vsg::vec4 hexToRGB(const std::string& color);

    std::vector<MaterialObj> mMaterials{};
    std::vector<std::string> mTextures{};
    std::vector<uint32_t> mMatIndex{};
    std::unordered_map<std::string, uint32_t> uniqueMaterials;

public:
    std::string fbFileName; // Store the model filename for standalone PMI queries.

    std::unordered_map<std::string, uint32_t> protoIndex;
    std::unordered_map<std::string, uint32_t> protoTriangleNum;
    std::unordered_map<int, uint32_t> materialIndex;

    std::vector<cadDataManager::pmiInfo> pmi;

    bool back_cull = true;
    
    static vsg::ImageInfoList camera_info;
    static vsg::ImageInfoList depth_info;
    static std::unordered_map<std::string, vsg::ImageInfoList> texture_name_to_image_map;
    static std::unordered_map<std::string, ProtoData*> proto_id_to_data_map;
    static std::vector<ProtoData*> insert_order_to_data;
    static std::unordered_map<std::string, uint32_t> fb_color_to_material_index;
    static std::unordered_map<std::string, std::string> fb_color_to_leader_material_key;

    // Global material array (same size as insert_order_to_data)
    static std::vector<vsg::ref_ptr<vsg::PbrMaterialValue>> global_material_array;
    static vsg::ref_ptr<vsg::PbrMaterialArray> global_material_buffer;

    static std::unordered_map<std::string, std::vector<MatrixIndex>> id_to_matrix_index_map;

    static vsg::ref_ptr<vsg::mat4Array> global_model_matrix_buffer;
    static vsg::ref_ptr<vsg::BufferInfo> global_model_matrix_buffer_info;
    static vsg::ref_ptr<vsg::mat4Array> last_global_model_matrix_buffer;
    static vsg::ref_ptr<vsg::BufferInfo> last_global_model_matrix_buffer_info;
    static std::unordered_map<std::string, uint32_t> model_name_to_global_index;

    static DynamicLines dynamic_lines;
    static DynamicPoints dynamic_points;
    static DynamicTexts dynamic_texts;

    static DynamicLines pmi_lines;
    static DynamicTexts pmi_texts;
    static std::vector<PMITransformNode> pmi_transform_nodes;

    // Scene instance data consumed by ImGui.
    static std::vector<std::string> scene_instance_names;
    static std::vector<vsg::dmat4> scene_original_transforms;
    static std::string scenes_json_path;
    static int current_scene_id;
    static vsg::View* active_view;

    // Instance-name to model-relative-path mapping used when saving Scenes.json.
    static std::unordered_map<std::string, std::string> instance_name_to_rel_path;

    // At the start of each frame, copy the current matrices into the previous-frame buffers.
    static void copyCurrentToLastMatrices();

    // Update the PMI transform matrices for the specified model at runtime.
    static void updatePMITransforms(uint32_t model_idx);

    std::vector<std::string> proto_ids;
    std::unordered_map<std::string, std::vector<vsg::dmat4>> proto_id_default_matrix_map;
    std::unordered_map<std::string, std::vector<std::string>> proto_id_instance_name_map;

    void preprocessProtoData(const char* model_path, const char* material_path, const vsg::dmat4& modelMatrix, vsg::ref_ptr<vsg::ShaderSet> model_shaderset, vsg::ref_ptr<vsg::Group> scene, std::string instance_name);
    void preprocessFBProtoData(const std::string model_path, const char* material_path, const vsg::dmat4& modelMatrix, vsg::ref_ptr<vsg::ShaderSet> model_shaderset, vsg::ref_ptr<vsg::Group> scene, std::string instance_name);
    static void buildDrawData(vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::PushConstants> pc, vsg::BufferInfoList constant_data_buffer_info_list, vsg::ref_ptr<vsg::ImageView> ShadowSampleImageView);

    static void buildDynamicLinesData(vsg::ref_ptr<vsg::ShaderSet> model_shaderset, vsg::ref_ptr<vsg::Group> scene, vsg::BufferInfoList constant_data_buffer_info_list);
    static void buildDynamicPointsData(vsg::ref_ptr<vsg::ShaderSet> model_shaderset, vsg::ref_ptr<vsg::Group> scene, vsg::BufferInfoList constant_data_buffer_info_list);
    static void buildDynamicTextsData(vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::Options> options, std::string font_path);
    static void processPMI(
        std::unordered_map<std::string, CADMesh*>& transfered_meshes,
        vsg::ref_ptr<vsg::ShaderSet> line_shader,
        vsg::ref_ptr<vsg::Group> wireframeGroup,
        vsg::ref_ptr<vsg::Group> textGroup,
        vsg::ref_ptr<vsg::Options> options,
        vsg::BufferInfoList constant_data_buffer_info_list,
        std::string font_path);
};
