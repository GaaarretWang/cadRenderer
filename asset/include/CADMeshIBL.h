#pragma once

#include "renderGeo_generated.h"
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <vsg/all.h>

#include "CADMesh.h"

namespace v2
{
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
        vsg::vec3 pos;
        vsg::vec2 uv;
        vsg::vec3 normal;
        vsg::vec4 tangent;

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

    struct treeNodeV2
    {
        std::vector<uint32_t> children;
        vsg::mat4 originalMatrix;
        vsg::dbox bounds;
    };

    template<typename T>
    vsg::vec3 toVec3(const flatbuffers::Vector<T>* flat_vector, int begin = 0);
    vsg::vec3 toVec3(const flatbuffers::String* string_vector);
    template<typename T>
    vsg::vec2 toVec2(const flatbuffers::Vector<T>* flat_vector, int begin = 0);

    /// CADMesh储存FlatBuffer模型文件的VertexBuffer，记录每个Instance的状态（如创建相应的Transform节点）
    /// 可以创建绘制指令节点，根据具体实现选择单独VertexBuffer绘制/合并VeretxBuffer绘制/Instance绘制
    /// 单独VertexBuffer绘制：Mesh的每个Model单独一个VertexArray（对应之前的方法）
    /// 合并VertexBuffer绘制：Mesh的所有Model VertexArray合并到同一个，每个渲染指令设置VertexAttribute的Offset
    /// Instance绘制：Mesh的所有Model VertexArray合并到同一个，每个渲染指令向DrawArgsBuffer填充绘制参数
    class CADMesh
    {
    private:
        struct ProtoInfo
        {
            uint32_t fbIndex;
            uint32_t arrayIdx;
            uint32_t triangleNum;
        };

        struct InstanceInfo
        {
            uint32_t fbIndex;
            uint32_t arrayIdx;
            uint32_t parentIdx;
            uint32_t protoIdx;
            std::string instanceId;
            std::string parentId;
            //treeNode nodePtr;
            treeNodeV2 node;
        };

        struct treeNode
        {
            std::vector<std::string> kids;
            vsg::ref_ptr<vsg::MatrixTransform> transform;
            vsg::mat4 originalMatrix;
            vsg::dbox bounds;
        };

        std::vector<MaterialObj> mMaterials{};
        std::vector<std::string> mTextures{};
        std::vector<uint32_t> mMatIndex{};
        std::unordered_map<std::string, uint32_t> uniqueMaterials; //pos,normal,uv...

        std::vector<ProtoInfo> mProtos;
        std::unordered_map<std::string, uint32_t> mProtoMap;
        std::vector<InstanceInfo> mInstances;
        std::unordered_map<std::string, uint32_t> mInstanceMap;

        vsg::ref_ptr<vsg::Transform> mRootNode;

        std::unordered_map<int, uint32_t> materialIndex;
        std::unordered_set<std::string> added;              //记录添加到树中的节点
        flatbuffers::FlatBufferBuilder builder_out;

        inline uint32_t protoCount() { return mProtos.size(); }
        inline uint32_t instanceCount() { return mInstances.size(); }
        bool findProto(std::string id, ProtoInfo* pResult) {
            if (mProtoMap.find(id) == mProtoMap.end()) return false;
            pResult = &mProtos[mProtoMap[id]];
        }
        bool findInstance(std::string id, InstanceInfo* pResult)
        {
            if (mInstanceMap.find(id) == mInstanceMap.end()) return false;
            pResult = &mInstances[mInstanceMap[id]];
        }

    public:
        std::vector<vsg::ref_ptr<vsg::vec3Array>> verticesVector;
        std::vector<vsg::ref_ptr<vsg::vec3Array>> normalsVector;
        std::vector<vsg::ref_ptr<vsg::vec2Array>> coordinatesVector;
        std::vector<vsg::ref_ptr<vsg::uintArray>> indicesVector;

        treeNode node_ibl;
        treeNode node_shadow;

        vsg::dmat4 model_matrix;

        void loadFile(const std::string& path, bool fullNormal);
        void loadRenderFlatBuffer(const RenderFlatBuffer::RenderFlatBufferDoc* renderFlatBuffer, bool fullNormal);
        void buildObjNode(const char* model_path, const char* material_path, const vsg::dmat4& modelMatrix);
        void buildScene(vsg::ref_ptr<vsg::GraphicsPipelineConfigurator>gpc_ibl, vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> gpc_shadow, vsg::ref_ptr<vsg::PbrMaterialValue> object_mat, vsg::ref_ptr<vsg::Group> scenegraph);
        void buildOBJScene(vsg::ref_ptr<vsg::GraphicsPipelineConfigurator>gpc_ibl, vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> gpc_shadow,vsg::ref_ptr<vsg::PbrMaterialValue> object_mat, vsg::ref_ptr<vsg::Group> scenegraph);
        void explode();
        void recover();

        vsg::ref_ptr<vsg::Node> createNodeSeperateDraw(vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> gpc);
        vsg::ref_ptr<vsg::Node> createDrawCmd(vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> gpc);
        vsg::ref_ptr<vsg::Node> createOBJDrawCmd(vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> gpc);
        vsg::ref_ptr<vsg::Node> test(vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> gpc);

        vsg::ref_ptr<vsg::Node> testCube(vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> gpc);
    };

    std::size_t hash_vec2(const vsg::t_vec2<float>& vec) noexcept;
    std::size_t hash_vec3(const vsg::t_vec3<float>& vec) noexcept;
    std::size_t hash_vec4(const vsg::t_vec4<float>& vec) noexcept;
} // namespace v2

namespace std
{
    template<>
    struct hash<v2::TinyModelVertex>
    {
        size_t operator()(v2::TinyModelVertex const& vertex) const
        {
            return ((v2::hash_vec3(vertex.pos) ^
                     (v2::hash_vec3(vertex.normal) << 1)) >> 1) ^
                   (v2::hash_vec2(vertex.uv) << 1);
        }
    };
} // namespace std