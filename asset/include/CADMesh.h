#pragma  once
#include "renderGeo_generated.h"
#include <iostream>
#include <unordered_set>
#include <vsg/all.h>
#include "communication/dataInterface.h"
#include "OBJLoader.h"
#define EXPLODE

// ---- 基础数据结构 ----

struct RGB
{
    float r, g, b;
};

// 线段：用于 PMI 标注和调试线框绘制
struct Line
{
    vsg::vec3 begin;
    vsg::vec3 end;

    Line(const vsg::vec3& beginPoint, const vsg::vec3& endPoint) :
        begin(beginPoint), end(endPoint)
    {
    }
};

// OBJ 材质参数（来自 .mtl 文件），用于传统 OBJ 格式模型
// 注意：FB 格式模型使用 vsg::PbrMaterialValue（PBR 材质），不使用此结构
struct MaterialObj
{
    vsg::vec3 ambient = vsg::vec3(0.1f, 0.1f, 0.1f);
    vsg::vec3 diffuse = vsg::vec3(0.7f, 0.f, 0.f);
    vsg::vec3 specular = vsg::vec3(1.0f, 1.0f, 1.0f);
    vsg::vec3 transmittance = vsg::vec3(0.0f, 0.0f, 0.0f);
    vsg::vec3 emission = vsg::vec3(0.0f, 0.0f, 0.0f);
    float shininess = 0.f;
    float ior = 1.0f;     // index of refraction（折射率）
    float dissolve = 1.f; // 1 == opaque; 0 == fully transparent（不透明度）

    int illum = 0;
    int diffuseTextureID = -1;
};

// 顶点结构体（OBJ 加载时用于顶点去重）
// 同时包含位置、UV、法线、颜色、切线，作为哈希键用于合并相同属性的顶点
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

/**
 * ProtoData — 原型数据（Prototype Data）
 *
 * 核心概念：一个 CAD 模型文件（.fb/.obj）可能包含多个独立的子网格（sub-mesh），
 * 每个子网格称为一个 "proto"（原型）。同一个 proto 可以有多个实例（instance），
 * 每个实例拥有不同的世界变换矩阵（instance_matrix），共享同一份几何数据（vertices/indices）。
 *
 * 这是一种典型的 GPU-Driven 实例化渲染（Instanced Rendering）数据组织方式：
 *   - 几何数据只存一份（vertices, normals, uvs, indices）
 *   - 每个实例的变换矩阵存入 instance_buffer → 传入 compute shader 做剔除
 *   - compute shader 输出可见实例的矩阵到 output_instance_buffer → 用于 DrawIndexedIndirect
 *
 * 双矩阵机制（current/last）：
 *   - instance_buffer: 当前帧的实例变换矩阵
 *   - last_instance_buffer: 上一帧的实例变换矩阵
 *   - 用途：运动模糊（motion blur）、时间滤波（temporal filtering）
 */
struct ProtoData
{
    // ---- 包围盒（用于遮挡剔除的 AABB 测试）----
    vsg::ref_ptr<vsg::BufferInfo> bounds_buffer_info;  // 包围盒的 GPU buffer 绑定信息
    vsg::ref_ptr<vsg::vec4Array> bounds_data;           // [0]=min.xyzw, [1]=max.xyzw

    std::string proto_id = "";  // 原型唯一标识（模型路径 + proto 索引）

    // ---- 几何数据（所有实例共享）----
    vsg::ref_ptr<vsg::vec3Array> vertices;   // 顶点位置数组（object space）
    vsg::ref_ptr<vsg::vec3Array> normals;    // 顶点法线数组
    vsg::ref_ptr<vsg::vec4Array> colors;     // 顶点颜色数组（OBJ 格式时有值，FB 格式为 nullptr）
    vsg::ref_ptr<vsg::vec2Array> uvs;        // 纹理坐标数组
    vsg::ref_ptr<vsg::uintArray> indices;    // 索引数组（triangle list）

    // ---- 材质纹理路径 ----
    std::string diffuse_path = "";   // 漫反射贴图路径（空字符串表示无贴图，使用基础色）
    std::string normal_path = "";    // 法线贴图路径
    std::string mr_path = "";        // 金属度-粗糙度贴图路径（R8G8 双通道格式）

    uint32_t material_index;  // 在 global_material_array 中的索引，指向该 proto 的 PBR 材质参数

    // ---- 实例化渲染相关 buffer ----
    vsg::ref_ptr<vsg::mat4Array> instance_buffer;        // 当前帧实例矩阵数组（每个实例一个 mat4）
    vsg::ref_ptr<vsg::mat4Array> last_instance_buffer;   // 上一帧实例矩阵数组（用于运动模糊等时间效果）
    vsg::ref_ptr<vsg::BufferInfo> input_instance_buffer_info;   // instance_buffer 的 descriptor 绑定
    vsg::ref_ptr<vsg::BufferInfo> last_instance_buffer_info;    // last_instance_buffer 的 descriptor 绑定

    // ---- 高亮/选中数据 ----
    // 每实例 4 个 uint: [highlight_state, padding, padding, model_index]
    // highlight_state: 0=无高亮, 1=选中高亮; model_index 用于在 shader 中查找全局 model 矩阵
    vsg::ref_ptr<vsg::uintArray> highlight_buffer;
    vsg::ref_ptr<vsg::BufferInfo> input_highlight_buffer_info;

    vsg::ref_ptr<vsg::BufferInfo> output_instance_buffer_info;  // compute shader 输出：剔除后的可见实例数据

    // ---- 实例列表 ----
    std::vector<std::string> instance_id;          // 每个实例的唯一 ID
    std::vector<vsg::dmat4> instance_matrix;       // 每个实例的默认变换矩阵（CPU 端备份）
    std::vector<uint32_t> instance_model_indices;  // 每个实例对应的 model 矩阵在 global_model_matrix_buffer 中的索引

    // ---- 渲染管线 ----
    vsg::ref_ptr<vsg::ShaderSet> shaderset;  // 该 proto 使用的 shader 程序集（PBR/line/point 等）
    vsg::ref_ptr<vsg::Group> scene;          // 该 proto 所属的场景图节点组
    vsg::ref_ptr<vsg::DrawIndexedIndirect> draw_indirect;  // GPU-Driven 间接绘制命令
    vsg::ref_ptr<vsg::BufferInfo> indirect_full_buffer_info; // 完整间接命令 buffer（用于备份/恢复）

    bool back_cull;  // 是否启用背面剔除（某些双面材质模型需要关闭）
};

// 动态线框数据：运行时每帧更新顶点/索引内容，用于可视化调试或标注
// 预分配 20000 个顶点/索引的容量，未使用区域用 -10000.f 填充使其不可见
struct DynamicLines
{
    vsg::ref_ptr<vsg::vec3Array> vertices;   // 顶点数组（DYNAMIC_DATA，每帧 CPU 端写入）
    vsg::ref_ptr<vsg::uintArray> indices;    // 索引数组
    vsg::ref_ptr<vsg::vec4Value> colors;     // 线段颜色（单值，所有线段共享）
};

// 动态点云数据：与 DynamicLines 结构类似，但使用 POINT_LIST 拓扑
struct DynamicPoints
{
    vsg::ref_ptr<vsg::vec3Array> vertices;
    vsg::ref_ptr<vsg::uintArray> indices;
    vsg::ref_ptr<vsg::vec4Value> colors;
};

// 动态文字数据：运行时可更新文字内容、位置、颜色等属性
struct DynamicTexts
{
    std::vector<vsg::ref_ptr<vsg::stringValue>> dynamic_text_labels;  // 文字内容
    std::vector<vsg::ref_ptr<vsg::StandardLayout>> standardLayout;     // 文字布局（位置/颜色/billboard 等）
    std::vector<vsg::ref_ptr<vsg::Text>> text;                         // VSG Text 节点
};

// PMI 变换节点：记录 PMI 标注与 model 矩阵的绑定关系
// 当 model 矩阵更新时（updatePMITransforms），同步更新这些节点的 transform
struct PMITransformNode
{
    vsg::ref_ptr<vsg::MatrixTransform> transform;  // 场景图中的变换节点
    uint32_t model_index;                           // 对应的 model 矩阵在 global_model_matrix_buffer 中的索引
    vsg::dmat4 instance_matrix;                     // PMI 自身的实例级变换（model 矩阵 * instance 矩阵）
    bool is_text;                                   // true=文字节点, false=线段节点
};


// MatrixIndex — 矩阵索引：将实例名称映射到其在 ProtoData 中的位置
// 用于 updateObjectPose() 和 repaint() 快速查找指定实例的数据
// proto_data: 指向该实例所属的 ProtoData
// index: 该实例在 proto_data->instance_matrix 中的索引
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

// 全局常量数据（传入 shader 的 ConstantBuffer）
// 每帧更新一次，所有 shader 共享
struct GlobalConstantData{
    float z_far;       // 远平面距离（用于深度线性化等计算）
    int shader_type;   // 合成 shader 类型枚举（FULL_MODEL / CAMERA_DEPTH / CAD_DAPTH）
    int width;         // 渲染宽度
    int height;        // 渲染高度
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
    std::string fbFileName; // 存储模型文件名，用于PMI独立查询

    int Nodenumber;
    int Triangnumber;
    int countnum = 0;

    flatbuffers::FlatBufferBuilder builder_out;
    std::unordered_map<std::string, uint32_t> protoIndex;
    std::unordered_map<std::string, uint32_t> protoTriangleNum;
    std::unordered_map<int, uint32_t> materialIndex;

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
    std::unordered_map<std::string, int> meshIndice;
    
    bool back_cull = true;

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

    // ---- 全局共享资源（static，所有 CADMesh 实例共享）----

    static vsg::ImageInfoList camera_info;   // 相机图像纹理（AR/MR 背景），供 shader 采样
    static vsg::ImageInfoList depth_info;    // 深度图像纹理，供 shader 采样
    static std::unordered_map<std::string, vsg::ImageInfoList> texture_name_to_image_map;  // 纹理路径 → GPU ImageInfo 缓存
    static std::unordered_map<std::string, ProtoData*> proto_id_to_data_map;  // proto_id → ProtoData 查找表
    static std::vector<ProtoData*> insert_order_to_data;  // 按插入顺序存储的所有 ProtoData（buildDrawData 按此顺序遍历）

    // 全局材质数组：每个 proto 一个 PBR 材质，按 insert_order_to_data 的顺序排列
    // buildDrawData 时将其打包为 global_material_buffer 传入 shader
    static std::vector<vsg::ref_ptr<vsg::PbrMaterialValue>> global_material_array;
    static vsg::ref_ptr<vsg::PbrMaterialArray> global_material_buffer;  // GPU 端材质数组 buffer

    // 实例名 → 矩阵索引映射表：支持通过实例名称快速查找其在 ProtoData 中的位置
    // key 格式：model_instance_name（整个 model 级别）或 model_instance_name + proto_instance_id（子实例级别）
    static std::unordered_map<std::string, std::vector<MatrixIndex>> id_to_matrix_index_map;

    // 全局 model 矩阵：每个 model（一个 .fb/.obj 文件实例）一个 mat4
    // 与 ProtoData 的 instance_matrix（proto 级别）区分：
    //   - global_model_matrix: model 级别（整个文件的变换，如位置/旋转）
    //   - instance_matrix: proto 级别（子网格在 model 内的局部变换）
    // 最终变换 = global_model_matrix * instance_matrix
    static vsg::ref_ptr<vsg::mat4Array> global_model_matrix_buffer;
    static vsg::ref_ptr<vsg::BufferInfo> global_model_matrix_buffer_info;
    static vsg::ref_ptr<vsg::mat4Array> last_global_model_matrix_buffer;  // 上一帧的 model 矩阵（双缓冲）
    static vsg::ref_ptr<vsg::BufferInfo> last_global_model_matrix_buffer_info;
    static std::unordered_map<std::string, uint32_t> model_name_to_global_index;  // model 名称 → 全局索引

    // 动态绘制数据（运行时每帧可更新内容）
    static DynamicLines dynamic_lines;   // 动态线框
    static DynamicPoints dynamic_points; // 动态点云
    static DynamicTexts dynamic_texts;   // 动态文字

    // PMI 标注数据（初始化时构建，之后跟随 model 矩阵更新）
    static DynamicLines pmi_lines;                  // PMI 线段
    static DynamicTexts pmi_texts;                  // PMI 文字
    static std::vector<PMITransformNode> pmi_transform_nodes;  // PMI 变换节点列表

    // 场景实例数据（供ImGui面板读取和场景保存/加载）
    static std::vector<std::string> scene_instance_names;
    static std::vector<vsg::dmat4> scene_original_transforms;
    static std::string scenes_json_path;
    static int current_scene_id;
    static vsg::View* active_view;

    // 实例名 → 模型相对路径映射（供保存 Scenes.json 时使用）
    static std::unordered_map<std::string, std::string> instance_name_to_rel_path;

    // 每帧开始时：将当前帧矩阵拷贝到上一帧矩阵缓冲（双缓冲切换）
    static void copyCurrentToLastMatrices();

    // 运行时更新指定 model 的 PMI 变换矩阵
    static void updatePMITransforms(uint32_t model_idx);

    // ---- 模型加载相关 ----
    std::vector<std::string> proto_ids;  // 该 CADMesh 实例加载的所有 proto ID 列表
    std::unordered_map<std::string, std::vector<vsg::dmat4>> proto_id_default_matrix_map;      // proto_id → 默认实例矩阵列表（用于同一模型的多实例复用）
    std::unordered_map<std::string, std::vector<std::string>> proto_id_instance_name_map;       // proto_id → 实例名称列表

    // 加载 OBJ 格式模型：提取几何数据 + 材质 + 纹理，构建 ProtoData
    void preprocessProtoData(const char* model_path, const char* material_path, const vsg::dmat4& modelMatrix, vsg::ref_ptr<vsg::ShaderSet> model_shaderset, vsg::ref_ptr<vsg::Group> scene, std::string instance_name);

    // 加载 FB (FlatBuffer) 格式模型：通过 cadDataManager 接口提取几何 + 材质参数，构建 ProtoData
    void preprocessFBProtoData(const std::string model_path, const char* material_path, const vsg::dmat4& modelMatrix, vsg::ref_ptr<vsg::ShaderSet> model_shaderset, vsg::ref_ptr<vsg::Group> scene, std::string instance_name);

    // 构建 GPU 绘制数据（核心函数）：为每个 ProtoData 创建 GraphicsPipeline + descriptor set + DrawIndexedIndirect
    static void buildDrawData(vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::PushConstants> pc, vsg::BufferInfoList constant_data_buffer_info_list, vsg::ref_ptr<vsg::ImageView> ShadowSampleImageView);

    // 构建动态线框绘制管线
    static void buildDynamicLinesData(vsg::ref_ptr<vsg::ShaderSet> model_shaderset, vsg::ref_ptr<vsg::Group> scene, vsg::BufferInfoList constant_data_buffer_info_list);

    // 构建动态点云绘制管线
    static void buildDynamicPointsData(vsg::ref_ptr<vsg::ShaderSet> model_shaderset, vsg::ref_ptr<vsg::Group> scene, vsg::BufferInfoList constant_data_buffer_info_list);

    // 构建动态文字节点
    static void buildDynamicTextsData(vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::Options> options, std::string font_path);

    // 处理 PMI（Product Manufacturing Information）标注：遍历所有模型，提取 PMI 数据并构建线段+文字节点
    static void processPMI(
        std::unordered_map<std::string, CADMesh*>& transfered_meshes,
        vsg::ref_ptr<vsg::ShaderSet> line_shader,
        vsg::ref_ptr<vsg::Group> wireframeGroup,
        vsg::ref_ptr<vsg::Group> textGroup,
        vsg::ref_ptr<vsg::Options> options,
        vsg::BufferInfoList constant_data_buffer_info_list,
        std::string font_path);
};