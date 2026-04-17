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
#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _WIN32
static std::string utf8ToGbk(const std::string& utf8Str) {
    if (utf8Str.empty()) return utf8Str;
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8Str.data(), static_cast<int>(utf8Str.size()), nullptr, 0);
    if (wlen <= 0) return utf8Str;
    std::wstring wstr(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8Str.data(), static_cast<int>(utf8Str.size()), &wstr[0], wlen);
    int glen = WideCharToMultiByte(CP_ACP, 0, wstr.data(), wlen, nullptr, 0, nullptr, nullptr);
    if (glen <= 0) return utf8Str;
    std::string gbkStr(glen, 0);
    WideCharToMultiByte(CP_ACP, 0, wstr.data(), wlen, &gbkStr[0], glen, nullptr, nullptr);
    return gbkStr;
}
#endif

vsg::ImageInfoList CADMesh::camera_info;
vsg::ImageInfoList CADMesh::depth_info;
std::unordered_map<std::string, vsg::ImageInfoList> CADMesh::texture_name_to_image_map;
std::unordered_map<std::string, ProtoData*> CADMesh::proto_id_to_data_map;
std::vector<ProtoData*> CADMesh::insert_order_to_data;
std::unordered_map<std::string, uint32_t> CADMesh::fb_color_to_material_index;
std::unordered_map<std::string, std::string> CADMesh::fb_color_to_leader_material_key;

// Global material array
std::vector<vsg::ref_ptr<vsg::PbrMaterialValue>> CADMesh::global_material_array;
vsg::ref_ptr<vsg::PbrMaterialArray> CADMesh::global_material_buffer;

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

static std::vector<vsg::dmat4> global_model_matrices_accumulator;

namespace
{
std::string normalizeMaterialColorKey(std::string color)
{
    if (color.empty())
    {
        return "#000000";
    }

    if (color.front() != '#')
    {
        color.insert(color.begin(), '#');
    }

    std::transform(color.begin(), color.end(), color.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });

    return color;
}

std::string extractPersistMaterialKey(const std::string& full_path)
{
    const size_t last_slash = full_path.find_last_of("/\\");
    if (last_slash == std::string::npos)
    {
        return full_path;
    }
    return full_path.substr(last_slash + 1);
}
}

vsg::ref_ptr<vsg::mat4Array> CADMesh::global_model_matrix_buffer;
vsg::ref_ptr<vsg::BufferInfo> CADMesh::global_model_matrix_buffer_info;
vsg::ref_ptr<vsg::mat4Array> CADMesh::last_global_model_matrix_buffer;
vsg::ref_ptr<vsg::BufferInfo> CADMesh::last_global_model_matrix_buffer_info;
std::unordered_map<std::string, uint32_t> CADMesh::model_name_to_global_index;

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

void CADMesh::copyCurrentToLastMatrices()
{
    // Copy the global model matrices into the previous-frame buffer.
    if (global_model_matrix_buffer && last_global_model_matrix_buffer) {
        for (size_t i = 0; i < global_model_matrix_buffer->size(); i++) {
            last_global_model_matrix_buffer->set(i, global_model_matrix_buffer->at(i));
        }
        last_global_model_matrix_buffer->dirty();
    }

    // Copy each proto instance matrix into the previous-frame buffer.
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
    // Strip the leading '#'.
    std::string testcolor = color.substr(1);

    // Convert the hex triplet to RGB.
    int red = std::stoi(testcolor.substr(0, 2), nullptr, 16);
    int green = std::stoi(testcolor.substr(2, 2), nullptr, 16);
    int blue = std::stoi(testcolor.substr(4, 2), nullptr, 16);

    // Normalize the RGB values into the [0, 1] range.
    float r = red / 255.0f;
    float g = green / 255.0f;
    float b = blue / 255.0f;

    return vsg::vec4{r, g, b, 1.0};
}

void CADMesh::preprocessFBProtoData(const std::string model_path, const char* material_path, const vsg::dmat4& modelMatrix, vsg::ref_ptr<vsg::ShaderSet> model_shaderset, vsg::ref_ptr<vsg::Group> scene, std::string model_instance_name, vsg::ref_ptr<vsg::Group> transparent_scene)
{
    if(proto_ids.size() > 0){
        // Register the global model matrix.
        uint32_t model_idx;
        if (model_name_to_global_index.count(model_instance_name) == 0) {
            model_idx = global_model_matrices_accumulator.size();
            global_model_matrices_accumulator.push_back(modelMatrix);
            model_name_to_global_index[model_instance_name] = model_idx;
        } else {
            model_idx = model_name_to_global_index[model_instance_name];
        }

        for(auto& id: proto_ids){
            for(int i = 0; i < proto_id_default_matrix_map[id].size(); i ++){
                auto matrix = proto_id_default_matrix_map[id][i];
                auto proto_instance_name = proto_id_instance_name_map[id][i];
                proto_id_to_data_map[id]->instance_matrix.push_back(matrix);
                proto_id_to_data_map[id]->instance_model_indices.push_back(model_idx);

                auto proto_data = proto_id_to_data_map[id];
                id_to_matrix_index_map[model_instance_name + proto_id_instance_name_map[id][i]].push_back(MatrixIndex(proto_data, proto_data->instance_matrix.size() - 1));
                id_to_matrix_index_map[model_instance_name].push_back(MatrixIndex(proto_data, proto_data->instance_matrix.size() - 1));
            }
        }
        return;
    }

    bool LoadByJson = false; 
	// Initialize the data interface, including JSON loading and other setup work.
    cadDataManager::DataInterface datainterface;
	datainterface.init();

	//---------------------------------------Load data from JSON---------------------------------------
	if (LoadByJson) {
		bool isReadLocalFBData = datainterface.isReadLocalFBData();
		if (isReadLocalFBData) {
			// Read the local FB file according to the JSON configuration.
			datainterface.parseLocalModel();
		}
		else {
			bool isConvertModelByFile = datainterface.isConvertModelByFile();
			if (isConvertModelByFile) {
				// The path points to a local file, so convert the local CAD file directly.
				datainterface.convertModelByFile();
			}
			else {
				// The path points to a cloud file, so convert the remote CAD model directly.
				datainterface.convertModelByPath();
			}
		}
		//auto instanceInfos = datainterface.getInstanceInfos();
		//auto fbModelData = datainterface.getModelFlatbuffersData();
	}
    
	//--------------------------------------Load data from arguments-----------------------------------
	if (!LoadByJson) {
        size_t lastSlash = model_path.find_last_of("/\\");
        std::string fbFilePath = model_path.substr(0, lastSlash);
        std::string fbFileName = model_path.substr(lastSlash + 1);
        std::string cloudPath = "/home/cadar/cadDataManager/model";
        std::string cloudName = "TT8-R600.stp";

		//std::string fbFileName = "sample_copy.fb";
		//std::string fbFilePath = "G:/1.4project/caddatamanagerfor1.4/FBData";

		//std::string cadFileName = "cartoon_jeep.stp";
		//std::string cadFilePath = "F:/model";

		// Convert a local FlatBuffer model.
#ifdef _WIN32
		fbFileName = utf8ToGbk(fbFileName);
		fbFilePath = utf8ToGbk(fbFilePath);
#endif
		datainterface.parseLocalModel(fbFileName, fbFilePath);

		// Convert a local CAD model.
		//DataInterface::convertModelByFile("127.0.0.1", 9000, cadFileName, cadFilePath, ConversionPrecision::low);

		// Convert a cloud-hosted CAD model. This path is slower, so local data is preferred for testing.
		// datainterface.convertModelByPath("101.76.208.70", 9000, cloudName, cloudPath, cadDataManager::ConversionPrecision::low);
    
		// After any conversion path finishes, the data interface exposes the latest converted model data.
		//auto renderInfo = DataInterface::getRenderInfo();
		//auto pmi = DataInterface::getPmiInfos(true);
		//auto instances = DataInterface::getInstances();

		// Use setActiveDocumentData(filename) to switch the active model before fetching its data again.
		//DataInterface::setActiveDocumentData(fbFileName);
		//auto renderInfo2 = DataInterface::getRenderInfo();

		// Use removeModelData to remove model data from the interface.
		//DataInterface::removeModelData(cadFileName);
	}
    datainterface.loadMaterialData("../asset/data/JsonData/CockpitMaterial.json"); // Pass the material JSON path here.
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
    // Register the global model matrix.
    uint32_t model_idx;
    if (model_name_to_global_index.count(model_instance_name) == 0) {
        model_idx = global_model_matrices_accumulator.size();
        global_model_matrices_accumulator.push_back(modelMatrix);
        model_name_to_global_index[model_instance_name] = model_idx;
    } else {
        model_idx = model_name_to_global_index[model_instance_name];
    }
    for (auto it = MapInfo.begin(); it != MapInfo.end(); ++it){
        auto info = it->second;
        for (int o = 0; o < info.size(); o++) {
            std::unordered_map<TinyModelVertex, uint32_t> uniqueVertices; // Store unique vertex data once.
            std::vector<TinyModelVertex> mVertices{};                     // Store vertex-position entries.
            std::vector<vsg::vec3> mVerticesPos{};                        // Store vertex positions.
            std::vector<vsg::vec3> mVerticesNor{};                        // Store vertex normals.
            std::vector<uint32_t> mIndices{};                             // Store indices into the vertex arrays.
            
            cadDataManager::RenderInfo modelfbs = info[o];
            int num = modelfbs.matrixNum;
            auto matrix = modelfbs.matrix;
            auto type = modelfbs.type;
            auto protoId = modelfbs.protoId;
            auto modelGeo = modelfbs.geo;
            auto modelIndex = modelGeo->getIndex();
            auto position = modelGeo->getPosition();
            auto normal = modelGeo->getNormal();
            auto uv = modelGeo->getUV();
            auto modelPar = modelfbs.params;
            auto metalness = modelPar->mMetalness;
            auto specular = modelPar->mSpecular;
            auto opacity = modelPar->mOpacity;
            auto color = modelPar->mColor; // This will later be upgraded to a 3D RGB representation.
            auto emissive = modelPar->mEmissive;
            auto emissiveIntensity = modelPar->mEmissiveIntensity;
            auto shininess = modelPar->mShininess;
            auto roughness = modelPar->mRoughness;
            auto transmission = modelPar->mTransmission;
            auto material = modelPar->getMaterialName(); // Fetch the material name here, though it is not applied yet.
            auto proto_instance_ids = modelfbs.instanceIds;
            const std::string color_group_key = normalizeMaterialColorKey(color);

            // Set the material parameters.
            vsg::ref_ptr<vsg::PbrMaterialValue> default_material = vsg::PbrMaterialValue::create(); 
            default_material->value().baseColorFactor = hexToRGB(color);
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

            if (type == "mesh")
            {
                vsg::ref_ptr<vsg::vec3Array> vertices = vsg::vec3Array::create(position.size() / 3); // Allocate array storage.
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
                    proto_data->material_source = ProtoData::MaterialSource::Fb;
                    proto_data->fb_color_group_key = color_group_key;
                    proto_data->material_persist_key = extractPersistMaterialKey(proto_id);
                    proto_data->is_transparent = default_material->value().baseColorFactor.w < 1.0f;
                    // if(i < mtr_ids.size() && textures.size() > mtr_ids[i]){
                    //     proto_data->diffuse_path = "../asset/data/obj/helicopter-engine/tex/" + textures[mtr_ids[i]][0];
                    //     proto_data->normal_path = "../asset/data/obj/helicopter-engine/tex/" + textures[mtr_ids[i]][1];
                    //     proto_data->mr_path = "../asset/data/obj/helicopter-engine/tex/" + textures[mtr_ids[i]][2];
                    //     proto_data->material = materials[mtr_ids[i]];
                    // }else{
                    //     proto_data->diffuse_path = "";
                    //     proto_data->normal_path = "";
                    //     proto_data->mr_path = "";
                    // }
                    uint32_t material_idx = 0;
                    auto material_itr = fb_color_to_material_index.find(color_group_key);
                    if (material_itr == fb_color_to_material_index.end())
                    {
                        material_idx = global_material_array.size();
                        global_material_array.push_back(default_material);
                        fb_color_to_material_index[color_group_key] = material_idx;
                        fb_color_to_leader_material_key[color_group_key] = proto_data->material_persist_key;
                    }
                    else
                    {
                        material_idx = material_itr->second;
                    }
                    proto_data->material_index = material_idx;
                    proto_data->shaderset = model_shaderset;
                    proto_data->scene = (proto_data->is_transparent && transparent_scene) ? transparent_scene : scene;
                    proto_data->back_cull = back_cull;
                    proto_id_to_data_map[proto_id] = proto_data;
                    insert_order_to_data.push_back(proto_data);
                }
                proto_id_default_matrix_map[proto_id] = std::vector<vsg::dmat4>();
                proto_id_instance_name_map[proto_id] = std::vector<std::string>();
                for(int m_i = 0; m_i < matrix.size() / 16; m_i++){
                    vsg::dmat4 transforms_matrix;
                    for (int m = 0; m < 4; m++)
                        for (int n = 0; n < 4; n++)
                            transforms_matrix[m][n] = matrix[m_i * 16 + m * 4 + n];

                    proto_id_default_matrix_map[proto_id].push_back(transforms_matrix);
                    proto_id_instance_name_map[proto_id].push_back(proto_instance_ids[m_i]);

                    proto_data->instance_matrix.push_back(transforms_matrix);
                    proto_data->instance_model_indices.push_back(model_idx);

                    // std::cout << "proto_instance_ids[m_i] " << model_instance_name + proto_id_instance_name_map[proto_id][m_i] << std::endl;
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

void CADMesh::preprocessProtoData(const char* model_path, const char* material_path, const vsg::dmat4& modelMatrix, vsg::ref_ptr<vsg::ShaderSet> model_shaderset, vsg::ref_ptr<vsg::Group> scene, std::string model_instance_name, vsg::ref_ptr<vsg::Group> transparent_scene)
{
    if(proto_ids.size() > 0){
        // Register the global model matrix.
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
        std::unordered_map<TinyModelVertex, uint32_t> uniqueVertices; // Store unique vertex data once.
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
            if (uniqueVertices.count(vertex) == 0) // Insert the vertex only if it is new.
            {                                      // Append the vertex and record its position.
                uniqueVertices[vertex] = static_cast<uint32_t>(mVertices.size());
                mVertices.push_back(vertex);
            }
            mIndices.push_back(uniqueVertices[vertex]); // Rebuild indices against the new proto-local vertex array.
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
            proto_data->material_source = ProtoData::MaterialSource::Obj;
            proto_data->material_persist_key = extractPersistMaterialKey(proto_id);
            proto_data->fb_color_group_key.clear();

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
                proto_data->is_transparent = materials[mtr_ids[i][0]]->value().baseColorFactor.w < 1.0f;
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
                proto_data->is_transparent = false;
            }

            proto_data->shaderset = model_shaderset;
            proto_data->scene = (proto_data->is_transparent && transparent_scene) ? transparent_scene : scene;
            proto_data->back_cull = back_cull;
            proto_id_to_data_map[proto_id] = proto_data;
            insert_order_to_data.push_back(proto_data);
        }
        proto_id_default_matrix_map[proto_id] = std::vector<vsg::dmat4>();
        proto_id_instance_name_map[proto_id] = std::vector<std::string>();
        // Register the global model matrix.
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
                    // Assume RGBA ordering and read the R channel from each four-byte texel.
                    uint8_t metallic = metallicPtr[i * 4];      // R channel
                    uint8_t roughness = roughnessPtr[i * 4];     // R channel
                    mrPtr[i*2] = metallic; // Pack into the two-channel output texture.
                    mrPtr[i*2+1] = roughness; // Pack into the two-channel output texture.
                }
                texture_name_to_image_map[proto_data->mr_path] = createImageInfo(mrData);
            }
        }
    }
}

void CADMesh::buildDrawData(vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::PushConstants> pc, vsg::BufferInfoList constant_data_buffer_info_list, vsg::ref_ptr<vsg::ImageView> ShadowSampleImageView){
    // Create material buffer from global array
    if (!global_material_buffer || global_material_buffer->size() != global_material_array.size()) {
        global_material_buffer = vsg::PbrMaterialArray::create(global_material_array.size());
        for (size_t i = 0; i < global_material_array.size(); ++i) {
            global_material_buffer->set(i, global_material_array[i]->value());
        }
        global_material_buffer->properties.dataVariance = vsg::DataVariance::DYNAMIC_DATA;
    }
    for(ProtoData* proto_data : insert_order_to_data){
        auto graphicsPipelineConfig = vsg::GraphicsPipelineConfigurator::create(proto_data->shaderset);
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
        graphicsPipelineConfig->subpass = 0;
        proto_data->instance_buffer = vsg::mat4Array::create(proto_data->instance_matrix.size());
        proto_data->instance_buffer->properties.dataVariance = vsg::DYNAMIC_DATA_TRANSFER_AFTER_RECORD;
        for(int i = 0; i < proto_data->instance_matrix.size(); i ++){
            proto_data->instance_buffer->set(i, vsg::mat4(proto_data->instance_matrix[i]));
        }
        proto_data->input_instance_buffer_info = vsg::BufferInfo::create(proto_data->instance_buffer);

        // Create the previous-frame proto matrix buffer.
        proto_data->last_instance_buffer = vsg::mat4Array::create(proto_data->instance_matrix.size());
        proto_data->last_instance_buffer->properties.dataVariance = vsg::DYNAMIC_DATA;
        for(int i = 0; i < proto_data->instance_matrix.size(); i ++){
            proto_data->last_instance_buffer->set(i, vsg::mat4(proto_data->instance_matrix[i]));
        }
        proto_data->last_instance_buffer_info = vsg::BufferInfo::create(proto_data->last_instance_buffer);

        proto_data->highlight_buffer = vsg::uintArray::create(proto_data->instance_matrix.size() * 4);
        proto_data->highlight_buffer->properties.dataVariance = vsg::DYNAMIC_DATA;
        for(int i = 0; i < proto_data->instance_matrix.size(); i++){
            proto_data->highlight_buffer->set(i * 4, 0);
            proto_data->highlight_buffer->set(i * 4 + 1, 0);
            proto_data->highlight_buffer->set(i * 4 + 2, 0);
            proto_data->highlight_buffer->set(i * 4 + 3, proto_data->instance_model_indices[i]);
        }
        proto_data->input_highlight_buffer_info = vsg::BufferInfo::create(proto_data->highlight_buffer);

        proto_data->shadow_visibility_buffer = vsg::uintArray::create(proto_data->instance_matrix.size() * 8);
        proto_data->shadow_visibility_buffer->properties.dataVariance = vsg::DYNAMIC_DATA;
        for (int i = 0; i < proto_data->shadow_visibility_buffer->size(); ++i)
        {
            proto_data->shadow_visibility_buffer->set(i, 0u);
        }
        proto_data->shadow_visibility_buffer_info = vsg::BufferInfo::create(proto_data->shadow_visibility_buffer);

        // Two mat4 values, one int, and three padding ints: 36 scalars total.
        auto instance_data_buffer = vsg::floatArray::create(proto_data->instance_matrix.size() * 512);
        proto_data->output_instance_buffer_info = vsg::BufferInfo::create(instance_data_buffer);

        vsg::BufferInfoList info_list = {proto_data->output_instance_buffer_info};
        graphicsPipelineConfig->assignDescriptor("instanceModelMatrix", info_list);
        graphicsPipelineConfig->assignDescriptor("ConstantBuffer", constant_data_buffer_info_list);
        
        auto noiseSampler = Utils::createNearestClampSampler();
        vsg::ImageInfoList ShadowSampleViewList = {vsg::ImageInfo::create(noiseSampler, ShadowSampleImageView, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)};
        graphicsPipelineConfig->assignTexture("shadowsampler", ShadowSampleViewList);

        if(proto_data->diffuse_path != ""){
            graphicsPipelineConfig->assignTexture("diffuseMap", texture_name_to_image_map[proto_data->diffuse_path]);
        }
        if(proto_data->normal_path != ""){
            graphicsPipelineConfig->assignTexture("normalMap", texture_name_to_image_map[proto_data->normal_path]);
        }
        if(proto_data->mr_path != ""){
            graphicsPipelineConfig->assignTexture("mrMap", texture_name_to_image_map[proto_data->mr_path]);
        }

        //todo
        graphicsPipelineConfig->assignTexture("cameraImage", camera_info);
        graphicsPipelineConfig->assignTexture("depthImage", depth_info);
        graphicsPipelineConfig->assignDescriptor("materialArray", global_material_buffer);

        vsg::box bounds;
        for (uint32_t i = 0; i < proto_data->vertices->size(); ++i)
        {
            bounds.add(proto_data->vertices->at(i));
        }
        
        vsg::DataList vertexArrays;
        graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, proto_data->vertices);
        graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Normal", VK_VERTEX_INPUT_RATE_VERTEX, proto_data->normals);
        if(proto_data->uvs)
            graphicsPipelineConfig->assignArray(vertexArrays, "vsg_TexCoord0", VK_VERTEX_INPUT_RATE_VERTEX, proto_data->uvs);
        // if(proto_data->colors)
        //     graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_VERTEX, proto_data->colors);
        // else
            graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, vsg::vec4Value::create(vsg::vec4{1.0f, 1.0f, 1.0f, 1.0f}));

        static float instance_id = 0;
        auto instance_id_array = vsg::vec4Array::create(proto_data->instance_matrix.size());

        // Use proto_data's material_index (stored during preprocessFBProtoData)
        uint32_t material_index = proto_data->material_index;

        for (size_t i = 0; i < proto_data->instance_matrix.size(); ++i) {
            instance_id_array->set(i, vsg::vec4(++instance_id, float(material_index), 1.0f, 1.0f));
        }
        graphicsPipelineConfig->assignArray(vertexArrays, "vsg_InstanceID", VK_VERTEX_INPUT_RATE_INSTANCE, instance_id_array);
        auto drawCommands = vsg::Commands::create();
        drawCommands->addChild(vsg::BindVertexBuffers::create(graphicsPipelineConfig->baseAttributeBinding, vertexArrays));
        drawCommands->addChild(vsg::BindIndexBuffer::create(proto_data->indices));

        proto_data->bounds_data = vsg::vec4Array::create(10);
        proto_data->bounds_data->set(0, vsg::vec4(bounds.min.x, bounds.min.y, bounds.min.z, 1));
        proto_data->bounds_data->set(1, vsg::vec4(bounds.max.x, bounds.max.y, bounds.max.z, 1));
        proto_data->bounds_data->properties.dataVariance = vsg::DYNAMIC_DATA;
        proto_data->bounds_buffer_info = vsg::BufferInfo::create(proto_data->bounds_data);

        VkDrawIndexedIndirectCommand cmd = {
            proto_data->indices->size(),      // indexCount
            proto_data->instance_matrix.size(),     // instanceCount
            0,         // firstIndex
            0,         // vertexOffsetid == 0
            0          // firstInstance
        };
        auto indirect_full_buffer = vsg::Array<VkDrawIndexedIndirectCommand>::create(1);
        indirect_full_buffer->set(0, cmd);
        auto indirect_full_buffer_info = vsg::BufferInfo::create(indirect_full_buffer);
        proto_data->indirect_full_buffer_info = indirect_full_buffer_info;

        auto indirectBuffer = vsg::Array<VkDrawIndexedIndirectCommand>::create(1);
        indirectBuffer->set(0, cmd);
        auto draw_indirect = vsg::DrawIndexedIndirect::create(
            indirectBuffer,  // Indirect-command buffer
            1,              // Number of draw commands
            sizeof(VkDrawIndexedIndirectCommand) // Command stride
        );
        draw_indirect->instanceMatrix = proto_data->instance_buffer;
        draw_indirect->highlightBuffer = proto_data->highlight_buffer;
        proto_data->draw_indirect = draw_indirect;
        drawCommands->addChild(draw_indirect);
        // auto draw_indexed = vsg::DrawIndexed::create(proto_data->indices->size(), proto_data->instance_matrix.size() / 2, 0, 0, 0);
        // draw_indexed->instanceMatrix = proto_data->instance_buffer;
        // drawCommands->addChild(draw_indexed);
        graphicsPipelineConfig->init();

        auto stateGroup = vsg::StateGroup::create();
        graphicsPipelineConfig->copyTo(stateGroup);
        static int i = 0;
        if(++i == 1) 
            stateGroup->add(pc);
        stateGroup->addChild(drawCommands);
        proto_data->scene->addChild(stateGroup);
    }

    // Create the global model-matrix buffer.
    global_model_matrix_buffer = vsg::mat4Array::create(global_model_matrices_accumulator.size());
    global_model_matrix_buffer->properties.dataVariance = vsg::DYNAMIC_DATA_TRANSFER_AFTER_RECORD;
    for (size_t i = 0; i < global_model_matrices_accumulator.size(); i++)
        global_model_matrix_buffer->set(i, vsg::mat4(global_model_matrices_accumulator[i]));
    global_model_matrix_buffer_info = vsg::BufferInfo::create(global_model_matrix_buffer);

    // Create the previous-frame global model-matrix buffer.
    last_global_model_matrix_buffer = vsg::mat4Array::create(global_model_matrices_accumulator.size());
    last_global_model_matrix_buffer->properties.dataVariance = vsg::DYNAMIC_DATA;
    for (size_t i = 0; i < global_model_matrices_accumulator.size(); i++)
        last_global_model_matrix_buffer->set(i, vsg::mat4(global_model_matrices_accumulator[i]));
    last_global_model_matrix_buffer_info = vsg::BufferInfo::create(last_global_model_matrix_buffer);

    // Release the temporary accumulator storage.
    global_model_matrices_accumulator.clear();
    global_model_matrices_accumulator.shrink_to_fit();

    // Bind the globalModelMatrix for every draw-indirect entry.
    for (ProtoData* proto_data : insert_order_to_data) {
        proto_data->draw_indirect->globalModelMatrix = global_model_matrix_buffer;
    }
}

void CADMesh::buildDynamicLinesData(vsg::ref_ptr<vsg::ShaderSet> model_shaderset, vsg::ref_ptr<vsg::Group> scene, vsg::BufferInfoList constant_data_buffer_info_list)
{
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
        indirectBuffer,  // Indirect-command buffer
        1,              // Number of draw commands
        sizeof(VkDrawIndexedIndirectCommand) // Command stride
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
        indirectBuffer,  // Indirect-command buffer
        1,              // Number of draw commands
        sizeof(VkDrawIndexedIndirectCommand) // Command stride
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

// Helper: deduplicate vertices through positionToIndex and build a shared-vertex line mesh.
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

    // Load the font once for all PMI labels.
    auto font = vsg::read_cast<vsg::Font>(font_path, options);
    if (!font) {
        vsg::info("PMI: FAILED to read font, aborting");
        vsg::info("=== processPMI END ===");
        return;
    }

    // Iterate over each CADMesh and query its PMI data independently.
    for (auto& [path, mesh] : transfered_meshes) {
        if (mesh->fbFileName.empty()) {
            vsg::info("PMI: skipping mesh with empty fbFileName, path=", path);
            continue;
        }

        // Switch the active model before fetching PMI data for this mesh.
        cadDataManager::DataInterface::setActiveDocumentData(mesh->fbFileName);
        auto pmiList = cadDataManager::DataInterface::getPmiInfos();

        if (pmiList.empty()) {
            continue;
        }

        vsg::info("PMI: model '", mesh->fbFileName, "' has ", pmiList.size(), " PMI items");

        // Find all model_idx entries associated with this mesh.
        std::vector<uint32_t> model_indices;
        for (auto& [inst_name, idx] : model_name_to_global_index) {
            // Use id_to_matrix_index_map to resolve the proto_data for each instance.
            auto it = id_to_matrix_index_map.find(inst_name);
            if (it != id_to_matrix_index_map.end() && !it->second.empty()) {
                // Check whether the instance belongs to the current mesh via proto_id.
                // Instances coming from the same CADMesh share the same proto_id.
                model_indices.push_back(idx);
            }
        }
        // Deduplicate model_indices.
        std::sort(model_indices.begin(), model_indices.end());
        model_indices.erase(std::unique(model_indices.begin(), model_indices.end()), model_indices.end());

        // Process each PMI entry.
        for (auto& pmiInfo : pmiList) {
            total_pmi++;

            // Find all model_idx entries matching this PMI protoId.
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

            // ---- Build the arrow line segments ----
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
                // Weighted arrow points: temp5 = (3*p1 + 2*p4) / 5, temp6 = (3*p4 + 2*p1) / 5.
                vsg::vec3 temp5 = (temp2 * 3.0f + temp4 * 2.0f) / 5.0f;
                vsg::vec3 temp6 = (temp4 * 3.0f + temp2 * 2.0f) / 5.0f;
                // Draw four segments: temp1->temp2, temp3->temp4, temp2->temp5, temp6->temp4.
                lineSegments.push_back({temp1, temp2});
                lineSegments.push_back({temp3, temp4});
                lineSegments.push_back({temp2, temp5});
                lineSegments.push_back({temp6, temp4});
                textPos = (temp2 + temp4) / 2.0f;
            } else if (pmiInfo.type == "Radius") {
                vsg::vec3 temp1(pmiInfo.points[0][0], pmiInfo.points[0][1], pmiInfo.points[0][2]);
                vsg::vec3 temp2(pmiInfo.points[1][0], pmiInfo.points[1][1], pmiInfo.points[1][2]);
                // Weighted arrow points: temp5 = (2*p0 + p1) / 3, temp6 = (2*p1 + p0) / 3.
                vsg::vec3 temp5 = (temp1 * 2.0f + temp2) / 3.0f;
                vsg::vec3 temp6 = (temp2 * 2.0f + temp1) / 3.0f;
                // Draw two segments: temp1->temp5 and temp6->temp2.
                lineSegments.push_back({temp1, temp5});
                lineSegments.push_back({temp6, temp2});
            } else if (pmiInfo.type == "Diameter") {
                if (pmiInfo.points.size() < 3) {
                    vsg::info("PMI: type=Diameter skipped - needs 3 points, got ", pmiInfo.points.size());
                    continue;
                }
                vsg::vec3 temp1(pmiInfo.points[0][0], pmiInfo.points[0][1], pmiInfo.points[0][2]);
                vsg::vec3 temp2(pmiInfo.points[1][0], pmiInfo.points[1][1], pmiInfo.points[1][2]);
                // temp3 uses the first text anchor position.
                vsg::vec3 temp3(pmiInfo.text[0][0], pmiInfo.text[0][1], pmiInfo.text[0][2]);
                // Draw two segments: temp1->temp2 and temp2->temp3.
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

            // Create the draw pipeline.
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

            // Precompute the instance-matrix list.
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

            // Create PMI line segments for each matched model_idx.
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

            // ---- PMI text nodes ----
            if (pmiInfo.text.empty() || pmiInfo.value.empty()) {
                continue;
            }

            // Create text nodes for each matched model_idx.
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

