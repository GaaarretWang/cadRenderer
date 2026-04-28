#include "CADMesh.h"
#include <chrono>
#include <iomanip>
#include <vsg/all.h>
#include <communication/dataInterface.h>
#include <vsgXchange/all.h>
#include <random>
#include "Utils.h"
#include <tuple>
#include <algorithm>

// ==================== 静态成员初始化 ====================

vsg::ImageInfoList CADMesh::camera_info;
vsg::ImageInfoList CADMesh::depth_info;
std::unordered_map<std::string, vsg::ImageInfoList> CADMesh::texture_name_to_image_map;  // 纹理缓存（避免重复加载）
std::unordered_map<std::string, ProtoData*> CADMesh::proto_id_to_data_map;                // proto 查找表
std::vector<ProtoData*> CADMesh::insert_order_to_data;                                    // 按加载顺序排列的 proto 列表

// 全局材质数组（每个 proto 对应一个 PBR 材质，buildDrawData 时打包为 GPU buffer）
std::vector<vsg::ref_ptr<vsg::PbrMaterialValue>> CADMesh::global_material_array;
vsg::ref_ptr<vsg::PbrMaterialArray> CADMesh::global_material_buffer;

// 实例名 → 矩阵索引映射（支持按名称更新实例变换/高亮状态）
std::unordered_map<std::string, std::vector<MatrixIndex>> CADMesh::id_to_matrix_index_map;

// static const std::unordered_map<std::string, std::tuple<float,float,float,float,float>> glassModelsMaterials = {
//     {"#FABE47", {1.000f, 0.736f, 0.033f, 0.995f, 0.366f}},
//     {"#D3B27D", {0.422f, 0.491f, 0.033f, 0.972f, 0.522f}},
//     {"#000000", {0.000f, 0.000f, 0.000f, 0.000f, 0.176f}},
//     {"#FF0000", {0.649f, 0.000f, 0.000f, 0.000f, 0.735f}},
//     {"#333333", {0.035f, 0.035f, 0.035f, 0.687f, 0.983f}},
//     {"#BBBBBB", {0.733f, 0.733f, 0.733f, 0.000f, 0.200f}},
//     {"#333300", {0.200f, 0.200f, 0.000f, 0.000f, 0.200f}},
//     {"#8F8C80", {0.140f, 0.146f, 0.151f, 0.995f, 0.437f}},
//     {"#F6B983", {0.158f, 0.147f, 0.163f, 0.972f, 0.579f}},
//     {"#595959", {0.349f, 0.349f, 0.349f, 0.972f, 0.200f}},
//     {"#B40000", {0.706f, 0.000f, 0.000f, 0.000f, 0.200f}},
//     {"#D2D2FF", {0.824f, 0.824f, 1.000f, 0.900f, 0.911f}},
//     {"#332D13", {0.428f, 0.317f, 0.145f, 0.995f, 0.721f}},
//     {"#9993BF", {0.600f, 0.576f, 0.749f, 0.000f, 0.200f}},
//     {"#C4B3D1", {0.769f, 0.702f, 0.820f, 0.000f, 0.200f}},
//     {"#FFFF00", {1.000f, 1.000f, 0.000f, 0.000f, 0.200f}},
//     {"#DCC81B", {0.863f, 0.784f, 0.106f, 0.000f, 0.200f}},
//     {"#0000CC", {0.000f, 0.000f, 0.000f, 0.000f, 0.000f}},
//     {"#FFCC66", {0.140f, 0.133f, 0.137f, 1.020f, 0.450f}},
//     {"#FFFFFF", {0.140f, 0.123f, 0.158f, 0.995f, 0.626f}},
//     {"#FF8000", {1.000f, 0.502f, 0.000f, 0.000f, 0.200f}},
// };

// 全局 model 矩阵累加器：初始化阶段临时存储所有 model 的变换矩阵
// buildDrawData 最后阶段将其拷贝到 global_model_matrix_buffer 并释放
static std::vector<vsg::dmat4> global_model_matrices_accumulator;

// 全局 model 矩阵 buffer（当前帧 + 上一帧双缓冲）
vsg::ref_ptr<vsg::mat4Array> CADMesh::global_model_matrix_buffer;
vsg::ref_ptr<vsg::BufferInfo> CADMesh::global_model_matrix_buffer_info;
vsg::ref_ptr<vsg::mat4Array> CADMesh::last_global_model_matrix_buffer;
vsg::ref_ptr<vsg::BufferInfo> CADMesh::last_global_model_matrix_buffer_info;
std::unordered_map<std::string, uint32_t> CADMesh::model_name_to_global_index;  // model 名 → 全局索引

DynamicLines CADMesh::dynamic_lines;
DynamicPoints CADMesh::dynamic_points;
DynamicTexts CADMesh::dynamic_texts;

DynamicLines CADMesh::pmi_lines;
DynamicTexts CADMesh::pmi_texts;
std::vector<PMITransformNode> CADMesh::pmi_transform_nodes;

std::vector<std::string> CADMesh::scene_instance_names;
std::vector<vsg::dmat4> CADMesh::scene_original_transforms;
std::string CADMesh::scenes_json_path;
int CADMesh::current_scene_id = -1;
vsg::View* CADMesh::active_view = nullptr;
std::unordered_map<std::string, std::string> CADMesh::instance_name_to_rel_path;

/**
 * copyCurrentToLastMatrices — 双缓冲矩阵切换
 *
 * 每帧开始时调用，将当前帧的变换矩阵保存为"上一帧"状态。
 * 用途：
 *   - 运动模糊（Motion Blur）：shader 中比较 current vs last 矩阵，计算运动方向和距离
 *   - 时间滤波（Temporal Filtering）：利用两帧之间的变换差异做抗抖动
 *
 * 切换机制：
 *   global_model_matrix_buffer（当前帧 model 矩阵）→ last_global_model_matrix_buffer
 *   proto_data->instance_buffer（当前帧实例矩阵）→ proto_data->last_instance_buffer
 *
 * dirty() 调用通知 VSG 该 buffer 内容已变化，需要重新上传到 GPU
 */
void CADMesh::copyCurrentToLastMatrices()
{
    // 拷贝全局 model 矩阵到上一帧缓冲
    if (global_model_matrix_buffer && last_global_model_matrix_buffer) {
        for (size_t i = 0; i < global_model_matrix_buffer->size(); i++) {
            last_global_model_matrix_buffer->set(i, global_model_matrix_buffer->at(i));
        }
        last_global_model_matrix_buffer->dirty();
    }

    // 拷贝每个 proto 的实例矩阵到上一帧缓冲
    for (ProtoData* proto_data : insert_order_to_data) {
        if (proto_data->instance_buffer && proto_data->last_instance_buffer) {
            for (size_t i = 0; i < proto_data->instance_buffer->size(); i++) {
                proto_data->last_instance_buffer->set(i, proto_data->instance_buffer->at(i));
            }
            proto_data->last_instance_buffer->dirty();
        }
    }
}

void CADMesh::updatePMITransforms(uint32_t model_idx)
{
    for (auto& node : pmi_transform_nodes) {
        if (node.model_index == model_idx) {
            node.transform->matrix = vsg::mat4(vsg::dmat4(global_model_matrix_buffer->at(model_idx)) * node.instance_matrix);
        }
    }
}

template<typename T>
vsg::vec3 CADMesh::toVec3(const flatbuffers::Vector<T>* flat_vector, int begin)
{
    // return vsg::vec3(flat_vector->Get(begin) / 1000, flat_vector->Get(begin + 1) / 1000, flat_vector->Get(begin + 2) / 1000);
    return vsg::vec3(flat_vector->Get(begin), flat_vector->Get(begin + 1), flat_vector->Get(begin + 2));
}

template<typename T>
vsg::vec3 CADMesh::toNewVec3(std::vector<T>* flat_vector, int begin)
{
    float a = flat_vector->at(begin);
    float b = flat_vector->at(begin + 1);
    float c = flat_vector->at(begin + 2);
    return vsg::vec3(a, b, c);
}

template<typename T>
vsg::vec2 CADMesh::toNewVec2(std::vector<T>* flat_vector, int begin)
{
    float a = flat_vector->at(begin);
    float b = flat_vector->at(begin + 1);
    return vsg::vec2(a, b);
}

template<typename T>
vsg::vec2 CADMesh::toVec2(const flatbuffers::Vector<T>* flat_vector, int begin)
{
    return vsg::vec2(flat_vector->Get(begin), flat_vector->Get(begin + 1));
}

vsg::vec3 CADMesh::toVec3(const flatbuffers::String* string_vector)
{
    if (string_vector->str().size() == 0)
        return vsg::vec3(0.f, 0.f, 0.f);
    vsg::vec3 vector3;
    std::string word = string_vector->str();
    int num;
    for (int i = 0; i < 3; i++)
    {
        std::stringstream converter;
        converter << std::hex << word.substr(i * 2 + 1, 2);
        converter >> num;
        vector3[i] = num / 255.0;
    }

    return vector3;
}

vsg::vec4 CADMesh::hexToRGB(const std::string& color)
{
    // 去掉 '#' 字符
    std::string testcolor = color.substr(1);

    // 将 hex 转换为 RGB
    int red = std::stoi(testcolor.substr(0, 2), nullptr, 16);
    int green = std::stoi(testcolor.substr(2, 2), nullptr, 16);
    int blue = std::stoi(testcolor.substr(4, 2), nullptr, 16);

    // 将 RGB 转换为 0.0 到 1.0 之间的浮点数
    float r = red / 255.0f;
    float g = green / 255.0f;
    float b = blue / 255.0f;

    return vsg::vec4{r, g, b, 1.0};
}

/**
 * preprocessFBProtoData — 加载 FB (FlatBuffer) 格式 CAD 模型
 *
 * 功能：通过 cadDataManager 接口解析 .fb 文件，提取几何数据（顶点/法线/UV/索引）和材质参数，
 *       构建 ProtoData 并注册到全局查找表中。
 *
 * 两次调用模式（同一个 CADMesh 实例）：
 *   第一次调用（proto_ids 为空）：完整加载模型 → 创建 ProtoData → 记录 proto_id
 *   第二次调用（proto_ids 非空）：复用已有 ProtoData，只追加新的实例矩阵
 *     → 同一模型文件的多个实例共享几何数据，只分配不同的变换矩阵
 *
 * 数据流程：
 *   1. cadDataManager 解析 .fb → RenderInfo 列表（每个子网格一个 RenderInfo）
 *   2. 提取顶点/法线/UV/索引 → 创建 vsg::vec3Array/vec2Array/uintArray
 *   3. 提取材质参数（颜色/粗糙度/金属度）→ 创建 PbrMaterialValue
 *   4. 构建 ProtoData（几何 + 材质 + 纹理路径）
 *   5. 注册全局 model 矩阵和实例矩阵
 */
void CADMesh::preprocessFBProtoData(const std::string model_path, const char* material_path, const vsg::dmat4& modelMatrix, vsg::ref_ptr<vsg::ShaderSet> model_shaderset, vsg::ref_ptr<vsg::Group> scene, std::string model_instance_name)
{
    // ---- 快速路径：proto 已加载过，只追加实例矩阵 ----
    if(proto_ids.size() > 0){
        // 注册（或查找）全局 model 矩阵索引
        uint32_t model_idx;
        if (model_name_to_global_index.count(model_instance_name) == 0) {
            model_idx = global_model_matrices_accumulator.size();
            global_model_matrices_accumulator.push_back(modelMatrix);
            model_name_to_global_index[model_instance_name] = model_idx;
        } else {
            model_idx = model_name_to_global_index[model_instance_name];
        }

        // 为每个已有 proto 追加新的实例矩阵（复用默认矩阵作为新实例的初始变换）
        for(auto& id: proto_ids){
            for(int i = 0; i < proto_id_default_matrix_map[id].size(); i ++){
                auto matrix = proto_id_default_matrix_map[id][i];
                auto proto_instance_name = proto_id_instance_name_map[id][i];
                proto_id_to_data_map[id]->instance_matrix.push_back(matrix);
                proto_id_to_data_map[id]->instance_model_indices.push_back(model_idx);

                auto proto_data = proto_id_to_data_map[id];
                // 注册到矩阵索引映射表（支持按名称查找实例数据）
                id_to_matrix_index_map[model_instance_name + proto_id_instance_name_map[id][i]].push_back(MatrixIndex(proto_data, proto_data->instance_matrix.size() - 1));
                id_to_matrix_index_map[model_instance_name].push_back(MatrixIndex(proto_data, proto_data->instance_matrix.size() - 1));
            }
        }
        return;
    }

    bool LoadByJson = false; 
	//执行接口的init方法：包含Json文件读取等一些初始化操作
    cadDataManager::DataInterface datainterface;
	datainterface.init();

	//---------------------------------------通过json文件加载数据------------------------------------
	if (LoadByJson) {
		bool isReadLocalFBData = datainterface.isReadLocalFBData();
		if (isReadLocalFBData) {
			//通过json文件参数读取本地路径下的fb文件
			datainterface.parseLocalModel();
		}
		else {
			bool isConvertModelByFile = datainterface.isConvertModelByFile();
			if (isConvertModelByFile) {
				//path为本地文件路径，直接上传本地CAD文件进行转换
				datainterface.convertModelByFile();
			}
			else {
				//path为云端文件路径，直接转换云端CAD模型
				datainterface.convertModelByPath();
			}
		}
		//auto instanceInfos = datainterface.getInstanceInfos();
		//auto fbModelData = datainterface.getModelFlatbuffersData();
	}
    
	//---------------------------------------通过传参加载数据------------------------------------------
	if (!LoadByJson) {
        size_t lastSlash = model_path.find_last_of("/\\");
        std::string fbFilePath = model_path.substr(0, lastSlash);
        std::string fbFileName = model_path.substr(lastSlash + 1);
        std::string cloudPath = "/home/cadar/cadDataManager/model";
        std::string cloudName = "TT8-R600.stp";

		//std::string fbFileName = "NAUO6副本.fb";
		//std::string fbFilePath = "G:/1.4project/caddatamanagerfor1.4/FBData";

		//std::string cadFileName = "卡通吉普车.stp";
		//std::string cadFilePath = "F:/model";

		//转换本地flatBuffer模型
		datainterface.parseLocalModel(fbFileName, fbFilePath);

		//转换本地CAD模型
		//DataInterface::convertModelByFile("127.0.0.1", 9000, cadFileName, cadFilePath, ConversionPrecision::low);

		//转换云端CAD模型  转云端的用这个代码，速度比较慢，测试用本地的。
		// datainterface.convertModelByPath("101.76.208.70", 9000, cloudName, cloudPath, cadDataManager::ConversionPrecision::low);
    
		//通过以上任何一种方式转换模型后，数据接口都将获取最后转换的模型数据
		//auto renderInfo = DataInterface::getRenderInfo();
		//auto pmi = DataInterface::getPmiInfos(true);
		//auto instances = DataInterface::getInstances();

		//通过setActiveDocumentData，传入文件名，可以切换“活跃状态”，再次获取数据时将获取“活跃模型数据”
		//DataInterface::setActiveDocumentData(fbFileName);
		//auto renderInfo2 = DataInterface::getRenderInfo();

		//通过removeModelData移除模型数据
		//DataInterface::removeModelData(cadFileName);
	}
    datainterface.loadMaterialData("/home/lab/workspace/wgy/cadRenderer/asset/data/JsonData/CockpitMaterial.json");//括号输入json路径
	// auto info = datainterface.getRenderInfo();
    auto MapInfo = datainterface.getRenderInfoMap();
	pmi = datainterface.getPmiInfos();
	auto instances = datainterface.getInstances();
	// auto instanceInfos = datainterface.getInstanceInfos();
	std::string fbModelData = datainterface.getModelFlatbuffersData();
    size_t lastSlash = model_path.find_last_of("/\\");
    std::string fbFilePath = model_path.substr(0, lastSlash);
    std::string fbFileName = model_path.substr(lastSlash + 1);
    this->fbFileName = fbFileName;
    if(fbFileName == "window.fb"){
        auto depthState = vsg::DepthStencilState::create();
        depthState->depthTestEnable = VK_TRUE;
        depthState->depthWriteEnable = VK_FALSE;
        model_shaderset->defaultGraphicsPipelineStates.push_back(depthState);
    }

    uint8_t* buffer_data;
    int buffer_size;
    // 注册全局 model 矩阵
    uint32_t model_idx;
    if (model_name_to_global_index.count(model_instance_name) == 0) {
        model_idx = global_model_matrices_accumulator.size();
        global_model_matrices_accumulator.push_back(modelMatrix);
        model_name_to_global_index[model_instance_name] = model_idx;
    } else {
        model_idx = model_name_to_global_index[model_instance_name];
    }
    // ---- 遍历所有 RenderInfo，提取每个子网格的几何和材质数据 ----
    for (auto it = MapInfo.begin(); it != MapInfo.end(); ++it){
        auto info = it->second;
        for (int o = 0; o < info.size(); o++) {
            std::unordered_map<TinyModelVertex, uint32_t> uniqueVertices;  // 顶点去重哈希表
            std::vector<TinyModelVertex> mVertices{};                      // 去重后的顶点列表
            std::vector<vsg::vec3> mVerticesPos{};                         // 顶点位置（辅助）
            std::vector<vsg::vec3> mVerticesNor{};                         // 顶点法线（辅助）
            std::vector<uint32_t> mIndices{};                              // 索引数组（指向去重后顶点）

            // 从 cadDataManager 提取 RenderInfo 的各项数据
            cadDataManager::RenderInfo modelfbs = info[o];
            int num = modelfbs.matrixNum;            // 该子网格的实例数量
            auto matrix = modelfbs.matrix;            // 实例变换矩阵（flat array，每 16 个 float 为一个 mat4）
            auto type = modelfbs.type;                // 几何类型："mesh" 表示三角网格
            auto protoId = modelfbs.protoId;          // 原型 ID（用于同一模型的多实例共享）
            auto modelGeo = modelfbs.geo;             // 几何数据接口
            auto modelIndex = modelGeo->getIndex();   // 索引数组（uint32 列表）
            auto position = modelGeo->getPosition();  // 顶点位置数组（每 3 个 float 为一个 vec3）
            auto normal = modelGeo->getNormal();      // 法线数组
            auto uv = modelGeo->getUV();              // UV 坐标数组

            // 材质参数提取
            auto modelPar = modelfbs.params;
            auto metalness = modelPar->mMetalness;
            auto specular = modelPar->mSpecular;
            auto opacity = modelPar->mOpacity;
            auto color = modelPar->mColor;                    // Hex 颜色字符串，如 "#FF8000"
            auto emissive = modelPar->mEmissive;
            auto emissiveIntensity = modelPar->mEmissiveIntensity;
            auto shininess = modelPar->mShininess;
            auto roughness = modelPar->mRoughness;
            auto transmission = modelPar->mTransmission;
            auto material = modelPar->getMaterialName();      // 材质名称（目前未完全生效）
            auto proto_instance_ids = modelfbs.instanceIds;   // 子实例 ID 列表
            std::string testcolor = color.substr(1);

            // 构建 PBR 材质对象：将 FB 材质参数映射到 vsg::PbrMaterial
            vsg::ref_ptr<vsg::PbrMaterialValue> default_material = vsg::PbrMaterialValue::create();
            default_material->value().baseColorFactor = hexToRGB(color);   // Hex → vec4(RGBA)
            default_material->value().roughnessFactor = roughness;
            default_material->value().metallicFactor = metalness;
            // std::string color_upper = color;
            // std::transform(color_upper.begin(), color_upper.end(), color_upper.begin(), ::toupper);
            // auto mat_it = glassModelsMaterials.find(color_upper);
            // if (mat_it != glassModelsMaterials.end()) {
            //     auto& [r, g, b, metal, rough] = mat_it->second;
            //     default_material->value().baseColorFactor = vsg::vec4{r, g, b, 1.0f};
            //     default_material->value().metallicFactor = metal;
            //     default_material->value().roughnessFactor = rough;
            // }
            if(fbFileName == "window.fb"){
                default_material->value().baseColorFactor.w = 0.7;
            }

            // ---- 仅处理三角网格类型 ----
            if (type == "mesh")
            {
                // 将 FB 的 flat 数组数据拷贝到 VSG 的 typed 数组（支持 GPU buffer 绑定）
                vsg::ref_ptr<vsg::vec3Array> vertices = vsg::vec3Array::create(position.size() / 3);
                vsg::ref_ptr<vsg::vec3Array> normals = vsg::vec3Array::create(normal.size() / 3);
                vsg::ref_ptr<vsg::vec2Array> uvs = vsg::vec2Array::create(uv.size() / 2);
                vsg::ref_ptr<vsg::uintArray> indices = vsg::uintArray::create(modelIndex.size());
                float* position_beginPointer = static_cast<float*>(vertices->dataPointer(0));
                std::copy(position.begin(), position.end(), position_beginPointer);
                float* normal_beginPointer = static_cast<float*>(normals->dataPointer(0));
                std::copy(normal.begin(), normal.end(), normal_beginPointer);
                float* uvs_beginPointer = static_cast<float*>(uvs->dataPointer(0));
                std::copy(uv.begin(), uv.end(), uvs_beginPointer);
                int* indices_beginPointer = static_cast<int*>(indices->dataPointer(0));
                std::copy(modelIndex.begin(), modelIndex.end(), indices_beginPointer);

                // ---- 构建 ProtoData ----
                ProtoData* proto_data;
                std::string proto_id = model_path + modelfbs.protoId + std::to_string(o);
                proto_ids.push_back(proto_id);
                {
                    proto_data = new ProtoData();
                    proto_data->vertices = vertices;
                    proto_data->normals = normals;
                    proto_data->colors = nullptr;
                    proto_data->uvs = nullptr;
                    proto_data->indices = indices;
                    proto_data->proto_id = protoId;
                    
                    uint32_t material_idx = global_material_array.size();
                    global_material_array.push_back(default_material);
                    proto_data->material_index = material_idx;
                    proto_data->shaderset = model_shaderset;
                    proto_data->scene = scene;
                    proto_data->back_cull = back_cull;
                    proto_id_to_data_map[proto_id] = proto_data;
                    insert_order_to_data.push_back(proto_data);
                }
                // ---- 解析实例变换矩阵并注册 ----
                // matrix 是 flat 数组，每 16 个 float 对应一个 4x4 变换矩阵（行主序）
                proto_id_default_matrix_map[proto_id] = std::vector<vsg::dmat4>();
                proto_id_instance_name_map[proto_id] = std::vector<std::string>();
                for(int m_i = 0; m_i < matrix.size() / 16; m_i++){
                    // 从 flat 数组解析 4x4 矩阵
                    vsg::dmat4 transforms_matrix;
                    for (int m = 0; m < 4; m++)
                        for (int n = 0; n < 4; n++)
                            transforms_matrix[m][n] = matrix[m_i * 16 + m * 4 + n];

                    // 记录默认矩阵和实例名（用于同一模型的多实例复用）
                    proto_id_default_matrix_map[proto_id].push_back(transforms_matrix);
                    proto_id_instance_name_map[proto_id].push_back(proto_instance_ids[m_i]);

                    // 注册实例矩阵到 ProtoData（每个实例对应一个 instance_matrix 条目）
                    proto_data->instance_matrix.push_back(transforms_matrix);
                    proto_data->instance_model_indices.push_back(model_idx);

                    // 注册到矩阵索引映射表（两级索引）：
                    //   1. model_instance_name + proto_instance_id → 精确到子实例
                    //   2. model_instance_name → 整个 model 级别（包含所有子实例）
                    if(id_to_matrix_index_map.find(model_instance_name + proto_id_instance_name_map[proto_id][m_i]) == id_to_matrix_index_map.end())
                        id_to_matrix_index_map[model_instance_name + proto_id_instance_name_map[proto_id][m_i]] = std::vector<MatrixIndex>();
                    id_to_matrix_index_map[model_instance_name + proto_id_instance_name_map[proto_id][m_i]].push_back(MatrixIndex(proto_data, proto_data->instance_matrix.size() - 1));

                    if(id_to_matrix_index_map.find(model_instance_name) == id_to_matrix_index_map.end())
                        id_to_matrix_index_map[model_instance_name] = std::vector<MatrixIndex>();
                    id_to_matrix_index_map[model_instance_name].push_back(MatrixIndex(proto_data, proto_data->instance_matrix.size() - 1));
                }
                // std::cout << std::endl;
            }
        }
    }
}

/**
 * preprocessProtoData — 加载 OBJ 格式模型
 *
 * 与 preprocessFBProtoData 的区别：
 *   - 数据源：OBJLoader（读取 .obj + .mtl）而非 cadDataManager
 *   - 顶点去重：使用 TinyModelVertex 哈希表合并相同属性的顶点（OBJ 格式中位置/法线/UV 独立索引）
 *   - 纹理：直接从 mtl 文件中读取 diffuse/normal/metallic-roughness 贴图路径
 *   - 材质：从 PbrMaterialValue 数组读取，而非 FB 的参数字段
 *
 * OBJ 顶点去重逻辑：
 *   OBJ 格式允许位置、法线、UV 使用不同的索引（f 1/2/3 4/5/6 ...），
 *   但 Vulkan 要求一个顶点的所有属性共享同一索引。
 *   因此需要将 (pos, normal, uv, color) 组合为 TinyModelVertex，用哈希表去重，
 *   生成新的紧凑顶点数组和对应的索引数组。
 */
void CADMesh::preprocessProtoData(const char* model_path, const char* material_path, const vsg::dmat4& modelMatrix, vsg::ref_ptr<vsg::ShaderSet> model_shaderset, vsg::ref_ptr<vsg::Group> scene, std::string model_instance_name)
{
    // 快速路径：proto 已加载，只追加实例
    if(proto_ids.size() > 0){
        // 注册全局 model 矩阵
        uint32_t model_idx;
        if (model_name_to_global_index.count(model_instance_name) == 0) {
            model_idx = global_model_matrices_accumulator.size();
            global_model_matrices_accumulator.push_back(modelMatrix);
            model_name_to_global_index[model_instance_name] = model_idx;
        } else {
            model_idx = model_name_to_global_index[model_instance_name];
        }

        for(auto& id: proto_ids){
            proto_id_to_data_map[id]->instance_matrix.push_back(proto_id_to_data_map[id]->instance_matrix[0]);
            proto_id_to_data_map[id]->instance_model_indices.push_back(model_idx);

            auto proto_data = proto_id_to_data_map[id];
            id_to_matrix_index_map[model_instance_name].push_back(MatrixIndex(proto_data, proto_data->instance_matrix.size() - 1));
        }
        return;
    }
    assert(model_path != nullptr);
    OBJLoader objLoader;
    std::unordered_map<std::string, int> num = objLoader.vertex_count(model_path);
    size_t lastSlash = std::string(model_path).find_last_of("/\\");
    std::string fileName = std::string(model_path).substr(lastSlash + 1);

    auto vertices = vsg::vec3Array::create(num["vertices"]); 
    auto normals = vsg::vec3Array::create(num["normals"]);
    std::vector<vsg::ref_ptr<vsg::PbrMaterialValue>> materials;
    
    auto options = vsg::Options::create();
    options->add(vsgXchange::all::create());
    auto createImageInfo = [](vsg::ref_ptr<vsg::Data> in_data) -> vsg::ImageInfoList {
        auto sampler = vsg::Sampler::create();
        vsg::ref_ptr<vsg::ImageInfo> imageInfosIBL = vsg::ImageInfo::create(sampler, in_data);
        vsg::ImageInfoList imageInfosListIBL = {imageInfosIBL};
        return imageInfosListIBL;
    };
    std::cout <<"success creating obj"<<std::endl;

    std::vector<std::vector<vsg::ref_ptr<vsg::uintArray>>> indices;// = vsg::uintArray::create(num["indices"]);
    vsg::ref_ptr<vsg::vec2Array> verticesUV = vsg::vec2Array::create(num["uvs"]);
    vsg::ref_ptr<vsg::vec3Array> colors = vsg::vec3Array::create(num["vertices"]);
    std::vector<std::vector<std::string>> textures;
    std::vector<std::vector<int>> mtr_ids;
    std::cout << material_path << std::endl;
    objLoader.load_obj(model_path, material_path, vertices, normals, verticesUV, colors, materials, indices, textures, mtr_ids);
    if(mtr_ids.size() > 0)
        std::cout << "mtr_ids[0].size()" << mtr_ids[0].size() << std::endl;
    std::cout <<"success Loading obj "<<material_path<<std::endl;
    std::cout << "indices.size()" << indices.size() << std::endl;
    for (int i = 0; i < indices.size(); i += 1)
    {
        std::unordered_map<TinyModelVertex, uint32_t> uniqueVertices; //存储点信息，相同点只存一份
        std::vector<TinyModelVertex> mVertices;
        std::vector<uint32_t> mIndices;
        std::cout << "indices[i][0]->size()" << indices[i][0]->size() << std::endl;
        for(int j = 0; j < indices[i][0]->size(); j ++){
            TinyModelVertex vertex;
            int index_pos = indices[i][0]->at(j);
            vertex.pos = vertices->at(index_pos);
            int index_normal = indices[i][1]->at(j);
            vertex.normal = normals->at(index_normal);
            if(mtr_ids.size() > i && mtr_ids[i].size() > j / 3 && mtr_ids[i][j / 3] < materials.size()){
                vertex.color = materials[mtr_ids[i][j / 3]]->value().baseColorFactor;
            }
            int index_coord = indices[i][2]->at(j);
            if(index_coord < verticesUV->size())
                vertex.uv = verticesUV->at(index_coord);    
            if (uniqueVertices.count(vertex) == 0) //if unique 唯一
            {                                      //push进数组。记录位置
                uniqueVertices[vertex] = static_cast<uint32_t>(mVertices.size());
                mVertices.push_back(vertex);
            }
            mIndices.push_back(uniqueVertices[vertex]); //根据新proto的数组，索引位置改变
        }
        auto vertices_i = vsg::vec3Array::create(mVertices.size()); 
        auto normals_i = vsg::vec3Array::create(mVertices.size());
        auto uvs_i = vsg::vec2Array::create(mVertices.size());
        auto colors_i = vsg::vec4Array::create(mVertices.size());
        auto indices_i = vsg::uintArray::create(mIndices.size());
        for(int m = 0; m < mVertices.size(); m ++){
            vertices_i->at(m) = mVertices[m].pos;
            normals_i->at(m) = mVertices[m].normal;
            uvs_i->at(m) = mVertices[m].uv;
            colors_i->at(m) = mVertices[m].color;
        }
        for(int m = 0; m < mIndices.size(); m ++){
            indices_i->at(m) = mIndices[m];
        }
        ProtoData* proto_data;
        std::string proto_id = model_path + std::to_string(i);
        proto_ids.push_back(proto_id);
        {
            proto_data = new ProtoData();
            proto_data->vertices = vertices_i;
            proto_data->normals = normals_i;
            proto_data->colors = colors_i;
            proto_data->uvs = uvs_i;
            proto_data->indices = indices_i;
            proto_data->proto_id = proto_id;

            if(i < mtr_ids.size() && mtr_ids[i][0] < materials.size()){
                // Push material to global array and store index
                uint32_t material_idx = global_material_array.size();
                global_material_array.push_back(materials[mtr_ids[i][0]]);
                proto_data->material_index = material_idx;
                if(textures.size() > mtr_ids[i][0])
                {
                    if(textures[mtr_ids[i][0]][0] != "")
                        proto_data->diffuse_path = std::string(material_path) + "/tex/" + textures[mtr_ids[i][0]][0];
                    if(textures[mtr_ids[i][0]][1] != "")
                        proto_data->normal_path = std::string(material_path) + "/tex/" + textures[mtr_ids[i][0]][1];
                    if(textures[mtr_ids[i][0]][2] != "")
                        proto_data->mr_path = std::string(material_path) + "/tex/" + textures[mtr_ids[i][0]][2];
                }
            }
            else{
                proto_data->diffuse_path = "";
                proto_data->normal_path = "";
                proto_data->mr_path = "";

                // Push default material to global array and store index
                auto default_material = vsg::PbrMaterialValue::create();
                uint32_t material_idx = global_material_array.size();
                global_material_array.push_back(default_material);
                proto_data->material_index = material_idx;
            }

            proto_data->shaderset = model_shaderset;
            proto_data->scene = scene;
            proto_data->back_cull = back_cull;
            proto_id_to_data_map[proto_id] = proto_data;
            insert_order_to_data.push_back(proto_data);
        }
        proto_id_default_matrix_map[proto_id] = std::vector<vsg::dmat4>();
        proto_id_instance_name_map[proto_id] = std::vector<std::string>();
        // 注册全局 model 矩阵
        uint32_t model_idx;
        if (model_name_to_global_index.count(model_instance_name) == 0) {
            model_idx = global_model_matrices_accumulator.size();
            global_model_matrices_accumulator.push_back(modelMatrix);
            model_name_to_global_index[model_instance_name] = model_idx;
        } else {
            model_idx = model_name_to_global_index[model_instance_name];
        }
        proto_data->instance_matrix.push_back(vsg::dmat4());
        proto_data->instance_model_indices.push_back(model_idx);
        proto_id_default_matrix_map[proto_id].push_back(vsg::dmat4());
        proto_id_instance_name_map[proto_id].push_back("0");

        if(id_to_matrix_index_map.find(model_instance_name) == id_to_matrix_index_map.end())
            id_to_matrix_index_map[model_instance_name] = std::vector<MatrixIndex>();
        id_to_matrix_index_map[model_instance_name].push_back(MatrixIndex(proto_data, proto_data->instance_matrix.size() - 1));

        if(i < mtr_ids.size() && textures.size() > mtr_ids[i][0]){
            if(textures[mtr_ids[i][0]][0] != "" && texture_name_to_image_map.find(proto_data->diffuse_path) == texture_name_to_image_map.end()){
                vsg::ref_ptr<vsg::Data> textureData = vsg::read_cast<vsg::Data>(proto_data->diffuse_path, options);
                texture_name_to_image_map[proto_data->diffuse_path] = createImageInfo(textureData);
            }
            if(textures[mtr_ids[i][0]][1] != "" && texture_name_to_image_map.find(proto_data->normal_path) == texture_name_to_image_map.end()){
                vsg::ref_ptr<vsg::Data> textureData = vsg::read_cast<vsg::Data>(proto_data->normal_path, options);
                texture_name_to_image_map[proto_data->normal_path] = createImageInfo(textureData);
            }
            if(textures[mtr_ids[i][0]][2] != "" && texture_name_to_image_map.find(proto_data->mr_path) == texture_name_to_image_map.end()){
                vsg::ref_ptr<vsg::Data> metallicData = vsg::read_cast<vsg::Data>(proto_data->mr_path, options);
                vsg::ref_ptr<vsg::Data> roughnessData = vsg::read_cast<vsg::Data>(std::string(material_path) + "/tex/" + textures[mtr_ids[i][0]][3], options);
                vsg::ref_ptr<vsg::Data> mrData = vsg::ubvec2Array2D::create(metallicData->width(), metallicData->height(), vsg::Data::Properties{VK_FORMAT_R8G8_UNORM});
                auto* metallicPtr = static_cast<const uint8_t*>(metallicData->dataPointer());
                auto* roughnessPtr = static_cast<const uint8_t*>(roughnessData->dataPointer());
                auto* mrPtr = static_cast<uint8_t*>(mrData->dataPointer());

                for (size_t i = 0; i < metallicData->dataSize()/4; ++i) {
                    // 假设RGBA顺序，取R通道（每4字节中的第0字节）
                    uint8_t metallic = metallicPtr[i * 4];      // R通道
                    uint8_t roughness = roughnessPtr[i * 4];     // R通道
                    mrPtr[i*2] = metallic; // 组合为双通道
                    mrPtr[i*2+1] = roughness; // 组合为双通道
                }
                texture_name_to_image_map[proto_data->mr_path] = createImageInfo(mrData);
            }
        }
    }
}

/**
 * buildDrawData — 构建 GPU 绘制数据（核心函数）
 *
 * 为每个 ProtoData 创建完整的渲染管线配置，包括：
 *   1. 全局材质 buffer（global_material_buffer）
 *   2. 每个 proto 的：
 *      - 实例矩阵 buffer（current + last 双缓冲）
 *      - 高亮/选中 buffer
 *      - 计算输出 buffer（供 compute shader 写入剔除后的实例数据）
 *      - 包围盒 buffer（供遮挡剔除使用）
 *      - GraphicsPipelineConfig（shader、descriptor set、顶点输入布局）
 *      - DrawIndexedIndirect 命令（GPU-Driven 间接绘制）
 *      - StateGroup 场景图节点
 *   3. 全局 model 矩阵 buffer（current + last 双缓冲）
 *
 * VSG 渲染管线配置流程：
 *   GraphicsPipelineConfigurator → assignDescriptor/assignTexture/assignArray → init → copyTo(StateGroup)
 *
 * @param scene                     场景图根节点（子节点将挂载到此处）
 * @param pc                        Push Constants（每帧全局数据）
 * @param constant_data_buffer_info_list  ConstantBuffer descriptor（全局常量）
 * @param ShadowSampleImageView     阴影采样结果图像（供 shader 采样阴影）
 */
void CADMesh::buildDrawData(vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::PushConstants> pc, vsg::BufferInfoList constant_data_buffer_info_list, vsg::ref_ptr<vsg::ImageView> ShadowSampleImageView){
    // ---- Step 1: 构建全局材质 buffer ----
    // 将 global_material_array（CPU 端）打包为 PbrMaterialArray（GPU 端 buffer）
    // 每个 proto 通过 material_index 索引到此 buffer 中的对应材质
    if (!global_material_buffer || global_material_buffer->size() != global_material_array.size()) {
        global_material_buffer = vsg::PbrMaterialArray::create(global_material_array.size());
        for (size_t i = 0; i < global_material_array.size(); ++i) {
            global_material_buffer->set(i, global_material_array[i]->value());
        }
        global_material_buffer->properties.dataVariance = vsg::DataVariance::DYNAMIC_DATA;
    }

    // ---- Step 2: 为每个 ProtoData 构建渲染管线 ----
    for(ProtoData* proto_data : insert_order_to_data){
        // 创建图形管线配置（基于该 proto 的 ShaderSet）
        auto graphicsPipelineConfig = vsg::GraphicsPipelineConfigurator::create(proto_data->shaderset);

        // 配置背面剔除：某些双面材质模型（如 window.fb）需要关闭
        if(! proto_data->back_cull){
            for (auto& state : graphicsPipelineConfig->pipelineStates) {
                if (auto rasterState = state.cast<vsg::RasterizationState>()) {
                    auto rasterizationState = vsg::RasterizationState::create();
                    rasterizationState->cullMode = VK_CULL_MODE_NONE;
                    state = rasterizationState;
                    break;
                }
            }
        }
        graphicsPipelineConfig->subpass = 0;  // 在 RenderPass 的 Subpass 0 中执行

        // ---- 创建实例矩阵 buffer（当前帧，DYNAMIC_DATA_TRANSFER_AFTER_RECORD）----
        // DYNAMIC_DATA_TRANSFER_AFTER_RECORD: 在 command buffer 录制之后再上传数据
        // 这意味着每帧渲染时可以修改矩阵内容，GPU 读取的是最新值
        proto_data->instance_buffer = vsg::mat4Array::create(proto_data->instance_matrix.size());
        proto_data->instance_buffer->properties.dataVariance = vsg::DYNAMIC_DATA_TRANSFER_AFTER_RECORD;
        for(int i = 0; i < proto_data->instance_matrix.size(); i ++){
            proto_data->instance_buffer->set(i, vsg::mat4(proto_data->instance_matrix[i]));
        }
        proto_data->input_instance_buffer_info = vsg::BufferInfo::create(proto_data->instance_buffer);

        // ---- 创建上一帧实例矩阵 buffer（双缓冲，用于运动模糊等）----
        proto_data->last_instance_buffer = vsg::mat4Array::create(proto_data->instance_matrix.size());
        proto_data->last_instance_buffer->properties.dataVariance = vsg::DYNAMIC_DATA;
        for(int i = 0; i < proto_data->instance_matrix.size(); i ++){
            proto_data->last_instance_buffer->set(i, vsg::mat4(proto_data->instance_matrix[i]));
        }
        proto_data->last_instance_buffer_info = vsg::BufferInfo::create(proto_data->last_instance_buffer);

        // ---- 创建高亮/选中 buffer ----
        // 每实例 4 个 uint: [highlight_state, 0, 0, model_index]
        // highlight_state: 0=无高亮, 1=选中高亮
        // model_index: 该实例在 global_model_matrix_buffer 中的索引，shader 用它查找 model 矩阵
        proto_data->highlight_buffer = vsg::uintArray::create(proto_data->instance_matrix.size() * 4);
        proto_data->highlight_buffer->properties.dataVariance = vsg::DYNAMIC_DATA;
        for(int i = 0; i < proto_data->instance_matrix.size(); i++){
            proto_data->highlight_buffer->set(i * 4, 0);
            proto_data->highlight_buffer->set(i * 4 + 1, 0);
            proto_data->highlight_buffer->set(i * 4 + 2, 0);
            proto_data->highlight_buffer->set(i * 4 + 3, proto_data->instance_model_indices[i]);
        }
        proto_data->input_highlight_buffer_info = vsg::BufferInfo::create(proto_data->highlight_buffer);

        // ---- 创建 compute shader 输出 buffer ----
        // compute shader 剔除后将 可见实例数据 写入此 buffer
        // 每实例 512 字节: 2个mat4(可见实例的 current/last 矩阵) + 1个int(实例ID) + padding填充对齐
        auto instance_data_buffer = vsg::floatArray::create(proto_data->instance_matrix.size() * 512);
        proto_data->output_instance_buffer_info = vsg::BufferInfo::create(instance_data_buffer);

        // ---- 绑定 descriptor set ----
        // instanceModelMatrix: compute shader 输出的可见实例数据，vertex shader 读取
        // ConstantBuffer: 全局常量（z_far, shader_type, width, height）
        vsg::BufferInfoList info_list = {proto_data->output_instance_buffer_info};
        graphicsPipelineConfig->assignDescriptor("instanceModelMatrix", info_list);
        graphicsPipelineConfig->assignDescriptor("ConstantBuffer", constant_data_buffer_info_list);

        // 绑定阴影采样贴图（Subpass 2 的 denoise 输出）
        auto noiseSampler = Utils::createNearestClampSampler();
        vsg::ImageInfoList ShadowSampleViewList = {vsg::ImageInfo::create(noiseSampler, ShadowSampleImageView, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)};
        graphicsPipelineConfig->assignTexture("shadowsampler", ShadowSampleViewList);

        // ---- 绑定材质纹理（可选）----
        if(proto_data->diffuse_path != ""){
            graphicsPipelineConfig->assignTexture("diffuseMap", texture_name_to_image_map[proto_data->diffuse_path]);
        }
        if(proto_data->normal_path != ""){
            graphicsPipelineConfig->assignTexture("normalMap", texture_name_to_image_map[proto_data->normal_path]);
        }
        if(proto_data->mr_path != ""){
            graphicsPipelineConfig->assignTexture("mrMap", texture_name_to_image_map[proto_data->mr_path]);
        }

        // 绑定相机图像和深度图（AR/MR 虚实融合用）
        graphicsPipelineConfig->assignTexture("cameraImage", camera_info);
        graphicsPipelineConfig->assignTexture("depthImage", depth_info);

        // 绑定全局材质数组 descriptor（shader 通过 material_index 索引到具体材质）
        graphicsPipelineConfig->assignDescriptor("materialArray", global_material_buffer);

        // ---- 计算包围盒（AABB）----
        // 遍历所有顶点，计算轴对齐包围盒
        // 包围盒数据存储到 bounds_data，用于 compute shader 的遮挡剔除测试
        // 取得 proto_data->vertices 中的所有顶点，计算出最小点和最大点，构成包围盒 bounds
        vsg::box bounds;
        for (uint32_t i = 0; i < proto_data->vertices->size(); ++i)
        {
            bounds.add(proto_data->vertices->at(i));
        }

        // ---- 配置顶点输入布局 ----
        // vsg_Vertex: 顶点位置（per-vertex，每个顶点一个值）
        // vsg_Normal: 顶点法线（per-vertex）
        // vsg_TexCoord0: 纹理坐标（per-vertex，可选）
        // vsg_Color: 顶点颜色（per-instance，所有实例共享白色，实际颜色由材质决定）
        vsg::DataList vertexArrays;
        graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, proto_data->vertices);
        graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Normal", VK_VERTEX_INPUT_RATE_VERTEX, proto_data->normals);
        if(proto_data->uvs)
            graphicsPipelineConfig->assignArray(vertexArrays, "vsg_TexCoord0", VK_VERTEX_INPUT_RATE_VERTEX, proto_data->uvs);
        // 使用 per-instance 的统一白色，实际颜色由 PbrMaterial 的 baseColorFactor 决定
        graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, vsg::vec4Value::create(vsg::vec4{1.0f, 1.0f, 1.0f, 1.0f}));

        // ---- 配置实例 ID 数组（per-instance attribute）----
        // 每个实例一个 vec4: (instance_id, material_index, 1.0, 1.0)
        // instance_id: 唯一标识符，shader 中用于查找高亮状态
        // material_index: 在 global_material_buffer 中的索引，shader 用它获取 PBR 材质参数
        static float instance_id = 0;
        auto instance_id_array = vsg::vec4Array::create(proto_data->instance_matrix.size());
        uint32_t material_index = proto_data->material_index;
        for (size_t i = 0; i < proto_data->instance_matrix.size(); ++i) {
            instance_id_array->set(i, vsg::vec4(++instance_id, float(material_index), 1.0f, 1.0f));
        }
        graphicsPipelineConfig->assignArray(vertexArrays, "vsg_InstanceID", VK_VERTEX_INPUT_RATE_INSTANCE, instance_id_array);

        // ---- 构建绘制命令 ----
        auto drawCommands = vsg::Commands::create();
        drawCommands->addChild(vsg::BindVertexBuffers::create(graphicsPipelineConfig->baseAttributeBinding, vertexArrays));
        drawCommands->addChild(vsg::BindIndexBuffer::create(proto_data->indices));

        // ---- 存储包围盒数据（供遮挡剔除 compute shader 使用）----
        proto_data->bounds_data = vsg::vec4Array::create(10);
        proto_data->bounds_data->set(0, vsg::vec4(bounds.min.x, bounds.min.y, bounds.min.z, 1));
        proto_data->bounds_data->set(1, vsg::vec4(bounds.max.x, bounds.max.y, bounds.max.z, 1));
        proto_data->bounds_data->properties.dataVariance = vsg::DYNAMIC_DATA;
        proto_data->bounds_buffer_info = vsg::BufferInfo::create(proto_data->bounds_data);

        // ---- 创建 DrawIndexedIndirect 间接绘制命令 ----
        // VkDrawIndexedIndirectCommand 结构：
        //   indexCount: 每个实例绘制的索引数
        //   instanceCount: 实例数量（compute shader 会修改此值为可见实例数）
        //   firstIndex: 起始索引偏移
        //   vertexOffset: 顶点偏移
        //   firstInstance: 起始实例偏移
        VkDrawIndexedIndirectCommand cmd = {
            proto_data->indices->size(),            // indexCount
            proto_data->instance_matrix.size(),     // instanceCount（初始值=全部实例，compute shader 会覆盖为可见数）
            0,         // firstIndex
            0,         // vertexOffset
            0          // firstInstance
        };

        // 间接命令完整备份（用于帧间恢复，如切换剔除模式时重置命令）
        auto indirect_full_buffer = vsg::Array<VkDrawIndexedIndirectCommand>::create(1);
        indirect_full_buffer->set(0, cmd);
        auto indirect_full_buffer_info = vsg::BufferInfo::create(indirect_full_buffer);
        proto_data->indirect_full_buffer_info = indirect_full_buffer_info;

        // 实际使用的间接命令 buffer（compute shader 会修改其 instanceCount）
        auto indirectBuffer = vsg::Array<VkDrawIndexedIndirectCommand>::create(1);
        indirectBuffer->set(0, cmd);
        auto draw_indirect = vsg::DrawIndexedIndirect::create(
            indirectBuffer,                          // 间接命令缓冲区
            1,                                       // 绘制命令数量
            sizeof(VkDrawIndexedIndirectCommand)     // 命令步长
        );
        // 绑定实例矩阵和高亮 buffer 到 DrawIndexedIndirect
        // VSG 的 DrawIndexedIndirect 扩展了这些字段，供 vertex shader 直接读取
        draw_indirect->instanceMatrix = proto_data->instance_buffer;
        draw_indirect->highlightBuffer = proto_data->highlight_buffer;
        proto_data->draw_indirect = draw_indirect;
        drawCommands->addChild(draw_indirect);

        // ---- 初始化管线并挂载到场景图 ----
        graphicsPipelineConfig->init();

        // 创建 StateGroup（管理管线状态的场景图节点）
        auto stateGroup = vsg::StateGroup::create();
        graphicsPipelineConfig->copyTo(stateGroup);

        // 只有第一个 proto 挂载 Push Constants（所有 proto 共享同一份 push constant）
        static int i = 0;
        if(++i == 1)
            stateGroup->add(pc);

        stateGroup->addChild(drawCommands);
        proto_data->scene->addChild(stateGroup);
    }

    // ---- Step 3: 构建全局 model 矩阵 buffer ----
    // 全局 model 矩阵 buffer（当前帧）
    // DYNAMIC_DATA_TRANSFER_AFTER_RECORD: 每帧录制 command buffer 之后再上传，确保 GPU 读取最新值
    global_model_matrix_buffer = vsg::mat4Array::create(global_model_matrices_accumulator.size());
    global_model_matrix_buffer->properties.dataVariance = vsg::DYNAMIC_DATA_TRANSFER_AFTER_RECORD;
    for (size_t i = 0; i < global_model_matrices_accumulator.size(); i++)
        global_model_matrix_buffer->set(i, vsg::mat4(global_model_matrices_accumulator[i]));
    global_model_matrix_buffer_info = vsg::BufferInfo::create(global_model_matrix_buffer);

    // 全局 model 矩阵 buffer（上一帧，用于运动模糊）
    last_global_model_matrix_buffer = vsg::mat4Array::create(global_model_matrices_accumulator.size());
    last_global_model_matrix_buffer->properties.dataVariance = vsg::DYNAMIC_DATA;
    for (size_t i = 0; i < global_model_matrices_accumulator.size(); i++)
        last_global_model_matrix_buffer->set(i, vsg::mat4(global_model_matrices_accumulator[i]));
    last_global_model_matrix_buffer_info = vsg::BufferInfo::create(last_global_model_matrix_buffer);

    // 释放累积器（已拷贝到 buffer，不再需要）
    global_model_matrices_accumulator.clear();
    global_model_matrices_accumulator.shrink_to_fit();

    // ---- Step 4: 将全局 model 矩阵 buffer 绑定到所有 DrawIndexedIndirect ----
    // vertex shader 通过 globalModelMatrix 将 model 级变换与 proto 级实例矩阵相乘
    for (ProtoData* proto_data : insert_order_to_data) {
        proto_data->draw_indirect->globalModelMatrix = global_model_matrix_buffer;
    }
}

/**
 * buildDynamicLinesData — 构建动态线框绘制管线
 *
 * 与静态 CAD 模型不同，动态线框数据每帧可由 CPU 端更新（addLineData）。
 * 预分配 20000 个顶点/索引的固定容量，未使用区域填充 -10000.f 使其不可见。
 *
 * 拓扑模式：VK_PRIMITIVE_TOPOLOGY_LINE_LIST（每两个索引构成一条线段）
 */
void CADMesh::buildDynamicLinesData(vsg::ref_ptr<vsg::ShaderSet> model_shaderset, vsg::ref_ptr<vsg::Group> scene, vsg::BufferInfoList constant_data_buffer_info_list)
{
    // 预分配固定容量的动态数据 buffer（DYNAMIC_DATA 允许每帧 CPU 写入）
    dynamic_lines.vertices = vsg::vec3Array::create(20000);
    dynamic_lines.vertices->properties.dataVariance = vsg::DataVariance::DYNAMIC_DATA;
    dynamic_lines.indices = vsg::uintArray::create(20000);
    dynamic_lines.indices->properties.dataVariance = vsg::DataVariance::DYNAMIC_DATA;
    auto graphicsPipelineConfig = vsg::GraphicsPipelineConfigurator::create(model_shaderset);
    graphicsPipelineConfig->assignTexture("cameraImage", camera_info);
    graphicsPipelineConfig->assignTexture("depthImage", depth_info);
    graphicsPipelineConfig->assignDescriptor("ConstantBuffer", constant_data_buffer_info_list);

    dynamic_lines.colors = vsg::vec4Value::create(vsg::vec4{1.0f, 1.0f, 1.0f, 1.0f});
    dynamic_lines.colors->properties.dataVariance = vsg::DataVariance::DYNAMIC_DATA;
    vsg::DataList vertexArrays;
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, dynamic_lines.vertices);
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, dynamic_lines.colors);
    auto drawCommands = vsg::Commands::create();
    drawCommands->addChild(vsg::BindVertexBuffers::create(graphicsPipelineConfig->baseAttributeBinding, vertexArrays));
    drawCommands->addChild(vsg::BindIndexBuffer::create(dynamic_lines.indices));

    VkDrawIndexedIndirectCommand cmd = {
        20000,      // indexCount
        1,          // instanceCount
        0,          // firstIndex
        0,          // vertexOffset
        0           // firstInstance
    };

    auto indirectBuffer = vsg::Array<VkDrawIndexedIndirectCommand>::create(1);
    indirectBuffer->set(0, cmd);
    auto draw_indirect = vsg::DrawIndexedIndirect::create(
        indirectBuffer,  // 间接命令缓冲区
        1,              // 绘制命令数量
        sizeof(VkDrawIndexedIndirectCommand) // 命令步长
    );
    drawCommands->addChild(draw_indirect);
    // auto draw_indexed = vsg::DrawIndexed::create(proto_data->indices->size(), proto_data->instance_matrix.size() / 2, 0, 0, 0);
    // draw_indexed->instanceMatrix = proto_data->instance_buffer;
    // drawCommands->addChild(draw_indexed);
    graphicsPipelineConfig->init();
    for (size_t i = 0; i < graphicsPipelineConfig->pipelineStates.size(); ++i)
    {
        if (graphicsPipelineConfig->pipelineStates[i]->is_compatible(typeid(vsg::InputAssemblyState)))
        {
            vsg::ref_ptr<vsg::InputAssemblyState> inputAssemblyState = 
                graphicsPipelineConfig->pipelineStates[i].cast<vsg::InputAssemblyState>();

            inputAssemblyState->topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            break;
        }
    }

    auto stateGroup = vsg::StateGroup::create();
    graphicsPipelineConfig->copyTo(stateGroup);
    stateGroup->addChild(drawCommands);
    scene->addChild(stateGroup);
}

void CADMesh::buildDynamicPointsData(vsg::ref_ptr<vsg::ShaderSet> model_shaderset, vsg::ref_ptr<vsg::Group> scene, vsg::BufferInfoList constant_data_buffer_info_list)
{
    dynamic_points.vertices = vsg::vec3Array::create(20000); 
    dynamic_points.vertices->properties.dataVariance = vsg::DataVariance::DYNAMIC_DATA;
    dynamic_points.indices = vsg::uintArray::create(20000); 
    dynamic_points.indices->properties.dataVariance = vsg::DataVariance::DYNAMIC_DATA;
    auto graphicsPipelineConfig = vsg::GraphicsPipelineConfigurator::create(model_shaderset);
    graphicsPipelineConfig->assignTexture("cameraImage", camera_info);
    graphicsPipelineConfig->assignTexture("depthImage", depth_info);
    graphicsPipelineConfig->assignDescriptor("ConstantBuffer", constant_data_buffer_info_list);

    dynamic_points.colors = vsg::vec4Value::create(vsg::vec4{1.0f, 1.0f, 1.0f, 1.0f});
    dynamic_points.colors->properties.dataVariance = vsg::DataVariance::DYNAMIC_DATA;
    vsg::DataList vertexArrays;
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, dynamic_points.vertices);
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, dynamic_points.colors);
    auto drawCommands = vsg::Commands::create();
    drawCommands->addChild(vsg::BindVertexBuffers::create(graphicsPipelineConfig->baseAttributeBinding, vertexArrays));
    drawCommands->addChild(vsg::BindIndexBuffer::create(dynamic_points.indices));

    VkDrawIndexedIndirectCommand cmd = {
        20000,      // indexCount
        1,          // instanceCount
        0,          // firstIndex
        0,          // vertexOffset
        0           // firstInstance
    };

    auto indirectBuffer = vsg::Array<VkDrawIndexedIndirectCommand>::create(1);
    indirectBuffer->set(0, cmd);
    auto draw_indirect = vsg::DrawIndexedIndirect::create(
        indirectBuffer,  // 间接命令缓冲区
        1,              // 绘制命令数量
        sizeof(VkDrawIndexedIndirectCommand) // 命令步长
    );
    drawCommands->addChild(draw_indirect);
    // auto draw_indexed = vsg::DrawIndexed::create(proto_data->indices->size(), proto_data->instance_matrix.size() / 2, 0, 0, 0);
    // draw_indexed->instanceMatrix = proto_data->instance_buffer;
    // drawCommands->addChild(draw_indexed);
    graphicsPipelineConfig->init();
    for (size_t i = 0; i < graphicsPipelineConfig->pipelineStates.size(); ++i)
    {
        if (graphicsPipelineConfig->pipelineStates[i]->is_compatible(typeid(vsg::InputAssemblyState)))
        {
            vsg::ref_ptr<vsg::InputAssemblyState> inputAssemblyState = 
                graphicsPipelineConfig->pipelineStates[i].cast<vsg::InputAssemblyState>();

            inputAssemblyState->topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            break;
        }
    }

    auto stateGroup = vsg::StateGroup::create();
    graphicsPipelineConfig->copyTo(stateGroup);
    stateGroup->addChild(drawCommands);
    scene->addChild(stateGroup);
}

void CADMesh::buildDynamicTextsData(vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::Options> options, std::string font_path)
{
    auto font = vsg::read_cast<vsg::Font>(font_path, options);
    if(!font){
        std::cout << "failed to read font" << std::endl;
    }

    for(int i = 0; i < 10; i ++){
        auto dynamic_text_label = vsg::stringValue::create("");
        dynamic_texts.dynamic_text_labels.push_back(dynamic_text_label);
        auto dynamic_text_layout = vsg::StandardLayout::create();
        dynamic_texts.standardLayout.push_back(dynamic_text_layout);
        auto dynamic_text = vsg::Text::create();
        dynamic_texts.text.push_back(dynamic_text);
        {
            // currently vsg::GpuLayoutTechnique is the only technique that supports dynamic updating of the text parameters
            dynamic_text->technique = vsg::GpuLayoutTechnique::create();

            dynamic_text_layout->billboard = true;
            dynamic_text_layout->position = vsg::vec3(0.0, 0.0, -6.0);
            dynamic_text_layout->horizontal = vsg::vec3(1.0, 0.0, 0.0);
            dynamic_text_layout->vertical = dynamic_text_layout->billboard ? vsg::vec3(0.0, 1.0, 0.0) : vsg::vec3(0.0, 0.0, 1.0) ;
            dynamic_text_layout->color = vsg::vec4(1.0, 0.9, 1.0, 1.0);
            dynamic_text_layout->outlineWidth = 0.1;

            dynamic_text->text = dynamic_text_label;
            dynamic_text->font = font;
            dynamic_text->layout = dynamic_text_layout;
            dynamic_text->setup(32); // allocate enough space for max possible characters
        }
        scene->addChild(dynamic_text);
    }
}

// 辅助函数：通过positionToIndex去重顶点，构建共享顶点+索引的线段网格
static std::pair<std::vector<vsg::vec3>, std::vector<uint32_t>> drawLinePMI(
    const std::vector<std::pair<vsg::vec3, vsg::vec3>>& lineSegments)
{
    std::vector<vsg::vec3> positions;
    std::unordered_map<vsg::vec3, uint32_t> positionToIndex;
    std::vector<uint32_t> indices;

    auto getOrCreateIndex = [&](const vsg::vec3& pt) -> uint32_t {
        auto it = positionToIndex.find(pt);
        if (it != positionToIndex.end()) {
            return it->second;
        }
        uint32_t idx = static_cast<uint32_t>(positions.size());
        positions.push_back(pt);
        positionToIndex[pt] = idx;
        return idx;
    };

    for (auto& [begin, end] : lineSegments) {
        indices.push_back(getOrCreateIndex(begin));
        indices.push_back(getOrCreateIndex(end));
    }

    return {positions, indices};
}

void CADMesh::processPMI(
    std::unordered_map<std::string, CADMesh*>& transfered_meshes,
    vsg::ref_ptr<vsg::ShaderSet> line_shader,
    vsg::ref_ptr<vsg::Group> wireframeGroup,
    vsg::ref_ptr<vsg::Group> textGroup,
    vsg::ref_ptr<vsg::Options> options,
    vsg::BufferInfoList constant_data_buffer_info_list,
    std::string font_path)
{
    vsg::info("=== processPMI START ===");

    int total_pmi = 0;
    int total_lines = 0;
    int total_text = 0;

    // 加载字体（全局只需一次）
    auto font = vsg::read_cast<vsg::Font>(font_path, options);
    if (!font) {
        vsg::info("PMI: FAILED to read font, aborting");
        vsg::info("=== processPMI END ===");
        return;
    }

    // 遍历每个CADMesh，独立查询PMI数据
    for (auto& [path, mesh] : transfered_meshes) {
        if (mesh->fbFileName.empty()) {
            vsg::info("PMI: skipping mesh with empty fbFileName, path=", path);
            continue;
        }

        // 切换活跃模型，获取此模型的PMI数据
        cadDataManager::DataInterface::setActiveDocumentData(mesh->fbFileName);
        auto pmiList = cadDataManager::DataInterface::getPmiInfos();

        if (pmiList.empty()) {
            continue;
        }

        vsg::info("PMI: model '", mesh->fbFileName, "' has ", pmiList.size(), " PMI items");

        // 找到此mesh关联的所有model_idx（可能有多个instance_name指向同一个CADMesh）
        std::vector<uint32_t> model_indices;
        for (auto& [inst_name, idx] : model_name_to_global_index) {
            // 通过id_to_matrix_index_map找到instance_name对应的proto_data
            auto it = id_to_matrix_index_map.find(inst_name);
            if (it != id_to_matrix_index_map.end() && !it->second.empty()) {
                // 检查是否属于当前mesh（通过proto_id匹配）
                // 所有来自同一CADMesh的instance共享proto_id
                model_indices.push_back(idx);
            }
        }
        // 去重model_indices
        std::sort(model_indices.begin(), model_indices.end());
        model_indices.erase(std::unique(model_indices.begin(), model_indices.end()), model_indices.end());

        // 对每个PMI进行处理
        for (auto& pmiInfo : pmiList) {
            total_pmi++;

            // 找到此PMI所属的protoId对应的所有model_idx（支持多个实例）
            std::vector<uint32_t> matched_model_indices;
            for (auto& [inst_name, idx] : model_name_to_global_index) {
                auto map_it = id_to_matrix_index_map.find(inst_name);
                if (map_it != id_to_matrix_index_map.end()) {
                    for (auto& mi : map_it->second) {
                        if (mi.proto_data && mi.proto_data->proto_id == pmiInfo.protoId) {
                            matched_model_indices.push_back(idx);
                            break;
                        }
                    }
                }
            }

            if (matched_model_indices.empty()) {
                vsg::info("PMI: could not find model for protoId=", pmiInfo.protoId, ", skipping");
                continue;
            }

            // ---- 绘制箭头线段 ----
            if (pmiInfo.points.size() < 2) {
                vsg::info("PMI: type=", pmiInfo.type, " skipped - points.size()=", pmiInfo.points.size(), " < 2");
                continue;
            }

            std::vector<std::pair<vsg::vec3, vsg::vec3>> lineSegments;
            vsg::vec3 textPos(pmiInfo.text[0][0], pmiInfo.text[0][1], pmiInfo.text[0][2]);

            if (pmiInfo.type == "Diagonal" || pmiInfo.type == "Horizontal") {
                if (pmiInfo.points.size() < 4) {
                    vsg::info("PMI: type=", pmiInfo.type, " skipped - needs 4 points, got ", pmiInfo.points.size());
                    continue;
                }
                vsg::vec3 temp1(pmiInfo.points[0][0], pmiInfo.points[0][1], pmiInfo.points[0][2]);
                vsg::vec3 temp2(pmiInfo.points[1][0], pmiInfo.points[1][1], pmiInfo.points[1][2]);
                vsg::vec3 temp3(pmiInfo.points[2][0], pmiInfo.points[2][1], pmiInfo.points[2][2]);
                vsg::vec3 temp4(pmiInfo.points[3][0], pmiInfo.points[3][1], pmiInfo.points[3][2]);
                temp2 = (temp2 + temp1 * 2.f) / 3.f;
                temp4 = (temp4 + temp3 * 2.f) / 3.f;
                // 箭头加权平均点：temp5 = (3*p1 + 2*p4) / 5, temp6 = (3*p4 + 2*p1) / 5
                vsg::vec3 temp5 = (temp2 * 3.0f + temp4 * 2.0f) / 5.0f;
                vsg::vec3 temp6 = (temp4 * 3.0f + temp2 * 2.0f) / 5.0f;
                // 画4条线：temp1→temp2, temp3→temp4, temp2→temp5, temp6→temp4
                lineSegments.push_back({temp1, temp2});
                lineSegments.push_back({temp3, temp4});
                lineSegments.push_back({temp2, temp5});
                lineSegments.push_back({temp6, temp4});
                textPos = (temp2 + temp4) / 2.0f;
            } else if (pmiInfo.type == "Radius") {
                vsg::vec3 temp1(pmiInfo.points[0][0], pmiInfo.points[0][1], pmiInfo.points[0][2]);
                vsg::vec3 temp2(pmiInfo.points[1][0], pmiInfo.points[1][1], pmiInfo.points[1][2]);
                // 箭头加权平均点：temp5 = (2*p0 + p1) / 3, temp6 = (2*p1 + p0) / 3
                vsg::vec3 temp5 = (temp1 * 2.0f + temp2) / 3.0f;
                vsg::vec3 temp6 = (temp2 * 2.0f + temp1) / 3.0f;
                // 画2条线：temp1→temp5, temp6→temp2
                lineSegments.push_back({temp1, temp5});
                lineSegments.push_back({temp6, temp2});
            } else if (pmiInfo.type == "Diameter") {
                if (pmiInfo.points.size() < 3) {
                    vsg::info("PMI: type=Diameter skipped - needs 3 points, got ", pmiInfo.points.size());
                    continue;
                }
                vsg::vec3 temp1(pmiInfo.points[0][0], pmiInfo.points[0][1], pmiInfo.points[0][2]);
                vsg::vec3 temp2(pmiInfo.points[1][0], pmiInfo.points[1][1], pmiInfo.points[1][2]);
                // temp3 = text[0]位置
                vsg::vec3 temp3(pmiInfo.text[0][0], pmiInfo.text[0][1], pmiInfo.text[0][2]);
                // 画2条线：temp1→temp2, temp2→temp3
                lineSegments.push_back({temp1, temp2});
                lineSegments.push_back({temp2, temp3});
            } else {
                vsg::info("PMI: unknown type '", pmiInfo.type, "', skipping");
                continue;
            }

            auto [line_verts, line_inds] = drawLinePMI(lineSegments);

            auto pmi_verts = vsg::vec3Array::create(line_verts.size());
            pmi_verts->properties.dataVariance = vsg::DataVariance::STATIC_DATA;
            std::copy(line_verts.begin(), line_verts.end(), pmi_verts->begin());

            auto pmi_inds = vsg::uintArray::create(line_inds.size());
            pmi_inds->properties.dataVariance = vsg::DataVariance::STATIC_DATA;
            std::copy(line_inds.begin(), line_inds.end(), pmi_inds->begin());

            auto pmi_color = vsg::vec4Value::create(vsg::vec4{0.0f, 0.0f, 0.0f, 1.0f});
            pmi_color->properties.dataVariance = vsg::DataVariance::STATIC_DATA;

            // 创建绘制管线
            auto graphicsPipelineConfig = vsg::GraphicsPipelineConfigurator::create(line_shader);
            graphicsPipelineConfig->assignTexture("cameraImage", camera_info);
            graphicsPipelineConfig->assignTexture("depthImage", depth_info);
            graphicsPipelineConfig->assignDescriptor("ConstantBuffer", constant_data_buffer_info_list);

            vsg::DataList vertexArrays;
            graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, pmi_verts);
            graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, pmi_color);

            auto drawCommands = vsg::Commands::create();
            drawCommands->addChild(vsg::BindVertexBuffers::create(graphicsPipelineConfig->baseAttributeBinding, vertexArrays));
            drawCommands->addChild(vsg::BindIndexBuffer::create(pmi_inds));

            VkDrawIndexedIndirectCommand cmd = {
                static_cast<uint32_t>(line_inds.size()),
                1, 0, 0, 0
            };
            auto indirectBuffer = vsg::Array<VkDrawIndexedIndirectCommand>::create(1);
            indirectBuffer->set(0, cmd);
            drawCommands->addChild(vsg::DrawIndexedIndirect::create(indirectBuffer, 1, sizeof(VkDrawIndexedIndirectCommand)));

            graphicsPipelineConfig->init();
            for (size_t i = 0; i < graphicsPipelineConfig->pipelineStates.size(); ++i) {
                if (graphicsPipelineConfig->pipelineStates[i]->is_compatible(typeid(vsg::InputAssemblyState))) {
                    auto inputAssemblyState = graphicsPipelineConfig->pipelineStates[i].cast<vsg::InputAssemblyState>();
                    inputAssemblyState->topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
                    break;
                }
            }

            auto lineStateGroup = vsg::StateGroup::create();
            graphicsPipelineConfig->copyTo(lineStateGroup);
            lineStateGroup->addChild(vsg::SetLineWidth::create(4.0f));
            lineStateGroup->addChild(drawCommands);

            // 预计算 instance 矩阵列表
            std::vector<vsg::dmat4> inst_matrices;
            if (pmiInfo.instanceMatrixList.empty()) {
                inst_matrices.push_back(vsg::dmat4());
            } else {
                for (auto& instMat : pmiInfo.instanceMatrixList) {
                    if (instMat.size() < 16) continue;
                    inst_matrices.emplace_back(
                        instMat[0], instMat[1], instMat[2], instMat[3],
                        instMat[4], instMat[5], instMat[6], instMat[7],
                        instMat[8], instMat[9], instMat[10], instMat[11],
                        instMat[12], instMat[13], instMat[14], instMat[15]
                    );
                }
            }

            // 为每个匹配的 model_idx 创建 PMI 线段
            for (auto midx : matched_model_indices) {
                for (auto& im : inst_matrices) {
                    auto transform = vsg::MatrixTransform::create();
                    transform->matrix = vsg::mat4(vsg::dmat4(global_model_matrix_buffer->at(midx)) * im);
                    transform->addChild(lineStateGroup);
                    wireframeGroup->addChild(transform);
                    pmi_transform_nodes.push_back({transform, midx, im, false});
                    total_lines++;
                }
            }

            // ---- PMI 文字节点 ----
            if (pmiInfo.text.empty() || pmiInfo.value.empty()) {
                continue;
            }

            // 为每个匹配的 model_idx 创建文字节点
            for (auto midx : matched_model_indices) {
                for (auto& im : inst_matrices) {
                    auto text_label = vsg::stringValue::create(pmiInfo.value + "mm");
                    pmi_texts.dynamic_text_labels.push_back(text_label);

                    auto text_layout = vsg::StandardLayout::create();
                    pmi_texts.standardLayout.push_back(text_layout);

                    auto text_node = vsg::Text::create();
                    pmi_texts.text.push_back(text_node);

                    text_node->technique = vsg::GpuLayoutTechnique::create();
                    text_layout->billboard = true;
                    text_layout->horizontal = vsg::vec3(0.1, 0.0, 0.0);
                    text_layout->vertical = vsg::vec3(0.0, 0.1, 0.0);
                    text_layout->color = vsg::vec4(0.0, 0.0, 0.0, 1.0);
                    // text_layout->outlineWidth = 0.1;
                    text_layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;
                    text_layout->verticalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;

                    if (!pmiInfo.text[0].empty() && pmiInfo.text[0].size() >= 3) {
                        text_layout->position = textPos;
                    }

                    text_node->text = text_label;
                    text_node->font = font;
                    text_node->layout = text_layout;
                    text_node->setup(32);

                    auto textTransform = vsg::MatrixTransform::create();
                    textTransform->matrix = vsg::mat4(vsg::dmat4(global_model_matrix_buffer->at(midx)) * im);
                    textTransform->addChild(text_node);
                    textGroup->addChild(textTransform);
                    pmi_transform_nodes.push_back({textTransform, midx, im, true});
                    total_text++;
                }
            }
        }
    }

    vsg::info("PMI: total processed - pmi=", total_pmi, " line_groups=", total_lines, " text_nodes=", total_text);
    vsg::info("=== processPMI END ===");
}

