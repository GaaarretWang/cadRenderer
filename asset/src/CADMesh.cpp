#include "CADMesh.h"
#include <chrono>
#include <iomanip>
#include <vsg/all.h>
#include <communication/dataInterface.h>
#include <vsgXchange/all.h>
#include <random>

vsg::ImageInfoList CADMesh::camera_info;
vsg::ImageInfoList CADMesh::depth_info;
vsg::ref_ptr<vsg::Data> CADMesh::params;
std::unordered_map<std::string, vsg::ImageInfoList> CADMesh::texture_name_to_image_map;
std::unordered_map<std::string, ProtoData*> CADMesh::proto_id_to_data_map;
std::vector<ProtoData*> CADMesh::insert_order_to_data;

std::unordered_map<std::string, std::vector<MatrixIndex>> CADMesh::id_to_matrix_index_map;

DynamicLines CADMesh::dynamic_lines;
DynamicPoints CADMesh::dynamic_points;
DynamicTexts CADMesh::dynamic_texts;

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

void CADMesh::preprocessFBProtoData(const std::string model_path, const char* material_path, const vsg::dmat4& modelMatrix, vsg::ref_ptr<vsg::ShaderSet> model_shaderset, vsg::ref_ptr<vsg::Group> scene, std::string model_instance_name)
{
    if(proto_ids.size() > 0){
        for(auto& id: proto_ids){
            for(int i = 0; i < proto_id_default_matrix_map[id].size(); i ++){
                auto matrix = proto_id_default_matrix_map[id][i];
                auto proto_instance_name = proto_id_instance_name_map[id][i];
                proto_id_to_data_map[id]->instance_matrix.push_back(matrix);
                proto_id_to_data_map[id]->instance_matrix.push_back(modelMatrix);

                auto proto_data = proto_id_to_data_map[id];
                id_to_matrix_index_map[model_instance_name + proto_data->proto_id + proto_id_instance_name_map[id][i]].push_back(MatrixIndex(proto_data, proto_data->instance_matrix.size() - 2));
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
    if(fbFileName == "window.fb"){
        auto depthState = vsg::DepthStencilState::create();
        depthState->depthTestEnable = VK_TRUE;
        depthState->depthWriteEnable = VK_FALSE;
        model_shaderset->defaultGraphicsPipelineStates.push_back(depthState);
    }

    uint8_t* buffer_data;
    int buffer_size;
    for (auto it = MapInfo.begin(); it != MapInfo.end(); ++it){
        auto info = it->second;
        for (int o = 0; o < info.size(); o++) {
            std::unordered_map<TinyModelVertex, uint32_t> uniqueVertices; //存储点信息，相同点只存一份
            std::vector<TinyModelVertex> mVertices{};                     //保存点在数组中位置信息
            std::vector<vsg::vec3> mVerticesPos{};                        //保存点在数组中位置信息
            std::vector<vsg::vec3> mVerticesNor{};                        //保存点在数组中位置信息
            std::vector<uint32_t> mIndices{};                             //索引，找点
            
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
            auto color = modelPar->mColor;//后续会改成三维rgb
            auto emissive = modelPar->mEmissive;
            auto emissiveIntensity = modelPar->mEmissiveIntensity;
            auto shininess = modelPar->mShininess;
            auto roughness = modelPar->mRoughness;
            auto transmission = modelPar->mTransmission;
            auto material = modelPar->getMaterialName();//这里得到材质的名称(未生效)
            auto proto_instance_ids = modelfbs.instanceIds;
            std::string testcolor = color.substr(1);

            //设置材质参数
            vsg::ref_ptr<vsg::PbrMaterialValue> default_material = vsg::PbrMaterialValue::create(); 
            default_material->value().baseColorFactor = hexToRGB(color);
            default_material->value().roughnessFactor = roughness;
            default_material->value().metallicFactor = metalness;
            if(fbFileName == "window.fb"){
                default_material->value().baseColorFactor.w = 0.7;
            }

            if (type == "mesh")
            {
                vsg::ref_ptr<vsg::vec3Array> vertices = vsg::vec3Array::create(position.size() / 3); //分配数组空间
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
                    proto_data->material = default_material;
                    proto_data->shaderset = model_shaderset;
                    proto_data->scene = scene;
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
                    proto_data->instance_matrix.push_back(modelMatrix);

                    // std::cout << "proto_instance_ids[m_i] " << model_instance_name + proto_data->proto_id + proto_id_instance_name_map[proto_id][m_i] << std::endl;
                    if(id_to_matrix_index_map.find(model_instance_name + proto_data->proto_id + proto_id_instance_name_map[proto_id][m_i]) == id_to_matrix_index_map.end())
                        id_to_matrix_index_map[model_instance_name + proto_data->proto_id + proto_id_instance_name_map[proto_id][m_i]] = std::vector<MatrixIndex>();
                    id_to_matrix_index_map[model_instance_name + proto_data->proto_id + proto_id_instance_name_map[proto_id][m_i]].push_back(MatrixIndex(proto_data, proto_data->instance_matrix.size() - 2));

                    if(id_to_matrix_index_map.find(model_instance_name) == id_to_matrix_index_map.end())
                        id_to_matrix_index_map[model_instance_name] = std::vector<MatrixIndex>();
                    id_to_matrix_index_map[model_instance_name].push_back(MatrixIndex(proto_data, proto_data->instance_matrix.size() - 1));
                }
                // std::cout << std::endl;
            }
        }
    }
}

void CADMesh::preprocessProtoData(const char* model_path, const char* material_path, const vsg::dmat4& modelMatrix, vsg::ref_ptr<vsg::ShaderSet> model_shaderset, vsg::ref_ptr<vsg::Group> scene, std::string model_instance_name)
{
    if(proto_ids.size() > 0){
        for(auto& id: proto_ids){
            proto_id_to_data_map[id]->instance_matrix.push_back(proto_id_to_data_map[id]->instance_matrix[0]);
            proto_id_to_data_map[id]->instance_matrix.push_back(modelMatrix);
                    
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
                proto_data->material = materials[mtr_ids[i][0]];
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
                proto_data->material = vsg::PbrMaterialValue::create();
            }
            if(fileName == "helicopter-engine.quads.obj"){
                proto_data->material->value().roughnessFactor = 1;
                proto_data->material->value().metallicFactor = 1;
            }

            proto_data->shaderset = model_shaderset;
            proto_data->scene = scene;
            proto_data->back_cull = back_cull;
            proto_id_to_data_map[proto_id] = proto_data;
            insert_order_to_data.push_back(proto_data);
        }
        proto_id_default_matrix_map[proto_id] = std::vector<vsg::dmat4>();
        proto_id_instance_name_map[proto_id] = std::vector<std::string>();
        proto_data->instance_matrix.push_back(vsg::dmat4());
        proto_data->instance_matrix.push_back(modelMatrix);
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

void CADMesh::buildDrawData(vsg::ref_ptr<vsg::ShaderSet> model_shaderset, vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::PushConstants> pc){
    for(ProtoData* proto_data : insert_order_to_data){
        if(! proto_data->back_cull){
            auto rasterizationState = vsg::RasterizationState::create();
            rasterizationState->cullMode = VK_CULL_MODE_NONE;
            proto_data->shaderset->defaultGraphicsPipelineStates.push_back(rasterizationState);
        }
        auto graphicsPipelineConfig = vsg::GraphicsPipelineConfigurator::create(proto_data->shaderset);
        graphicsPipelineConfig->subpass = 0;
        proto_data->instance_buffer = vsg::mat4Array::create(proto_data->instance_matrix.size());
        proto_data->instance_buffer->properties.dataVariance = vsg::DYNAMIC_DATA;
        for(int i = 0; i < proto_data->instance_matrix.size(); i ++){
            proto_data->instance_buffer->set(i, vsg::mat4(proto_data->instance_matrix[i]));
        }
        proto_data->input_instance_buffer_info = vsg::BufferInfo::create(proto_data->instance_buffer);

        proto_data->highlight_buffer = vsg::uintArray::create(proto_data->instance_matrix.size() / 2 * 4);
        proto_data->highlight_buffer->properties.dataVariance = vsg::DYNAMIC_DATA;
        for(int i = 0; i < proto_data->instance_matrix.size() / 2 * 4; i ++){
            proto_data->highlight_buffer->set(i, 0);
        }
        proto_data->input_highlight_buffer_info = vsg::BufferInfo::create(proto_data->highlight_buffer);

        // 2个mat4，1个int，3个padding int，一共36
        auto instance_data_buffer = vsg::floatArray::create(proto_data->instance_matrix.size() / 2 * 512);
        proto_data->output_instance_buffer_info = vsg::BufferInfo::create(instance_data_buffer);

        vsg::BufferInfoList info_list = {proto_data->output_instance_buffer_info};
        graphicsPipelineConfig->assignDescriptor("instanceModelMatrix", info_list);
        if(proto_data->diffuse_path != ""){
            graphicsPipelineConfig->assignTexture("diffuseMap", texture_name_to_image_map[proto_data->diffuse_path]);
        }
        if(proto_data->normal_path != ""){
            graphicsPipelineConfig->assignTexture("normalMap", texture_name_to_image_map[proto_data->normal_path]);
        }
        if(proto_data->mr_path != ""){
            graphicsPipelineConfig->assignTexture("mrMap", texture_name_to_image_map[proto_data->mr_path]);
        }
        graphicsPipelineConfig->assignTexture("cameraImage", camera_info);
        graphicsPipelineConfig->assignTexture("depthImage", depth_info);
        graphicsPipelineConfig->assignUniform("params", params);
        if(proto_data->material != nullptr){
            proto_data->material->properties.dataVariance = vsg::DataVariance::DYNAMIC_DATA;
            graphicsPipelineConfig->assignDescriptor("material", proto_data->material);
        }
        else
            graphicsPipelineConfig->assignDescriptor("material", vsg::PbrMaterialValue::create());

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
        if(proto_data->colors)
            graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_VERTEX, proto_data->colors);
        else
            graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, vsg::vec4Value::create(vsg::vec4{1.0f, 1.0f, 1.0f, 1.0f}));
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
            proto_data->instance_matrix.size() / 2,     // instanceCount
            0,         // firstIndex
            0,         // vertexOffset
            0          // firstInstance
        };
        auto indirect_full_buffer = vsg::Array<VkDrawIndexedIndirectCommand>::create(1);
        indirect_full_buffer->set(0, cmd);
        auto indirect_full_buffer_info = vsg::BufferInfo::create(indirect_full_buffer);
        proto_data->indirect_full_buffer_info = indirect_full_buffer_info;

        auto indirectBuffer = vsg::Array<VkDrawIndexedIndirectCommand>::create(1);
        indirectBuffer->set(0, cmd);
        auto draw_indirect = vsg::DrawIndexedIndirect::create(
            indirectBuffer,  // 间接命令缓冲区
            1,              // 绘制命令数量
            sizeof(VkDrawIndexedIndirectCommand) // 命令步长
        );
        draw_indirect->instanceMatrix = proto_data->instance_buffer;
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
}

void CADMesh::buildSSAOData(vsg::ref_ptr<vsg::ShaderSet> model_shaderset, vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::ImageView> GBufferView0, vsg::ref_ptr<vsg::ImageView> GBufferView1, vsg::ref_ptr<vsg::ImageView> GBufferView2, VkExtent2D extent){
    auto rasterizationState = vsg::RasterizationState::create();
    rasterizationState->cullMode = VK_CULL_MODE_NONE;
    model_shaderset->defaultGraphicsPipelineStates.push_back(rasterizationState);
    auto graphicsPipelineConfig = vsg::GraphicsPipelineConfigurator::create(model_shaderset);
    graphicsPipelineConfig->subpass = 1;
    auto instance_buffer = vsg::mat4Array::create(2);
    instance_buffer->properties.dataVariance = vsg::DYNAMIC_DATA;
    for(int i = 0; i < 2; i ++){
        instance_buffer->set(i, vsg::mat4());
    }
    auto input_instance_buffer_info = vsg::BufferInfo::create(instance_buffer);

    // 2个mat4，1个int，3个padding int，一共36
    auto instance_data_buffer = vsg::floatArray::create(512);
    instance_data_buffer->set(0, 1);
    instance_data_buffer->set(5, 1);
    instance_data_buffer->set(10, 1);
    instance_data_buffer->set(15, 1);
    auto output_instance_buffer_info = vsg::BufferInfo::create(instance_data_buffer);

    vsg::BufferInfoList info_list = {output_instance_buffer_info};
    unsigned int seed = 100;
    std::mt19937 generator(seed); // 梅森旋转算法，性能和随机性都好
    std::uniform_real_distribution<float> distribution(-1.0f, 1.0f);

    auto samplerNoiseData = vsg::ubvec4Array2D::create(extent.width, extent.height); // VSG 2D 纹理数据容器（RGBA8）
    for (uint32_t i = 0; i < extent.width; ++i){
        for (uint32_t j = 0; j < extent.height; ++j){
            float randX = distribution(generator); // C++ 标准线性随机数（x 分量）
            float randY = distribution(generator); // C++ 标准线性随机数（y 分量）
            vsg::vec2 dir = vsg::normalize(vsg::vec2(randX, randY)) * 0.5f + vsg::vec2(0.5f, 0.5f);
            samplerNoiseData->set(i, j, vsg::ubvec4(static_cast<uint8_t>(dir.x * 255.0f), 
                    static_cast<uint8_t>(dir.y * 255.0f), static_cast<uint8_t>(0), static_cast<uint8_t>(0)));
        }
    }
    samplerNoiseData->properties.format = VK_FORMAT_R8G8B8A8_UNORM;
    samplerNoiseData->properties.dataVariance = vsg::DYNAMIC_DATA;

    auto noiseSampler = vsg::Sampler::create();
    noiseSampler->magFilter = VK_FILTER_NEAREST;
    noiseSampler->minFilter = VK_FILTER_NEAREST;
    noiseSampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    noiseSampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    noiseSampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    noiseSampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    noiseSampler->maxAnisotropy = 1.0f;
    noiseSampler->minLod = 0.0f;
    noiseSampler->maxLod = 0.0f;
    vsg::ImageInfoList noiseImageInfoList = {vsg::ImageInfo::create(noiseSampler, samplerNoiseData)};
    graphicsPipelineConfig->assignTexture("samplerNoise", noiseImageInfoList);

    graphicsPipelineConfig->assignDescriptor("instanceModelMatrix", info_list);
    graphicsPipelineConfig->assignTexture("cameraImage", camera_info);
    graphicsPipelineConfig->assignTexture("depthImage", depth_info);
    graphicsPipelineConfig->assignUniform("params", params);
    graphicsPipelineConfig->assignDescriptor("material", vsg::PbrMaterialValue::create());

    vsg::ref_ptr<vsg::Sampler> in_sampler{nullptr};
    vsg::ImageInfoList GBufferViewList0 = {vsg::ImageInfo::create(noiseSampler, GBufferView0, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)};
    vsg::ImageInfoList GBufferViewList1 = {vsg::ImageInfo::create(noiseSampler, GBufferView1, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)};
    vsg::ImageInfoList GBufferViewList2 = {vsg::ImageInfo::create(noiseSampler, GBufferView2, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)};
    graphicsPipelineConfig->assignTexture("colorInputAttachment", GBufferViewList0);
    graphicsPipelineConfig->assignTexture("normalInputAttachment", GBufferViewList1);
    graphicsPipelineConfig->assignTexture("worldPosInputAttachment", GBufferViewList2);

    // auto vertices = vsg::floatArray::create({-3, 1, 1, 1, 1, 1, 1, -3, 1});
    // auto normals = vsg::floatArray::create({0, 0, 1, 0, 0, 1, 0, 0, 1});
    // auto indices = vsg::intArray::create({0, 1, 2});
    auto vertices = vsg::vec3Array::create({
         vsg::vec3(-1, -1, 1.0f),
         vsg::vec3(1, -1, 1.0f),
         vsg::vec3(1, 1, 1.0f),
         vsg::vec3(-1, 1, 1.0f)
        });

    auto normals = vsg::vec3Array::create(
        {
            {1.0f, 1.0f, 1.0f},
            {1.0f, 1.0f, 1.0f},
            {1.0f, 1.0f, 1.0f},
            {1.0f, 1.0f, 1.0f}
        });

    auto texcoords = vsg::vec2Array::create(
        {{0.0f, 1.0f},
         {1.0f, 1.0f},
         {1.0f, 0.0f},
         {0.0f, 0.0f}
        });

    auto indices = vsg::ushortArray::create(
        {0, 1, 2,
         2, 3, 0});

    vsg::box bounds;
    for (uint32_t i = 0; i < vertices->size(); ++i)
    {
        bounds.add(vertices->at(i));
    }
    
    vsg::DataList vertexArrays;
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, vertices);
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Normal", VK_VERTEX_INPUT_RATE_VERTEX, normals);
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_TexCoord0", VK_VERTEX_INPUT_RATE_VERTEX, texcoords);
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, vsg::vec4Value::create(vsg::vec4{1.0f, 1.0f, 1.0f, 1.0f}));
    auto drawCommands = vsg::Commands::create();
    drawCommands->addChild(vsg::BindVertexBuffers::create(graphicsPipelineConfig->baseAttributeBinding, vertexArrays));
    drawCommands->addChild(vsg::BindIndexBuffer::create(indices));

    VkDrawIndexedIndirectCommand cmd = {
        6,      // indexCount
        1,     // instanceCount
        0,         // firstIndex
        0,         // vertexOffset
        0          // firstInstance
    };

    auto indirectBuffer = vsg::Array<VkDrawIndexedIndirectCommand>::create(1);
    indirectBuffer->set(0, cmd);
    auto draw_indirect = vsg::DrawIndexedIndirect::create(
        indirectBuffer,  // 间接命令缓冲区
        1,              // 绘制命令数量
        sizeof(VkDrawIndexedIndirectCommand) // 命令步长
    );
    draw_indirect->instanceMatrix = instance_buffer;
    drawCommands->addChild(draw_indirect);
    graphicsPipelineConfig->init();

    auto stateGroup = vsg::StateGroup::create();
    graphicsPipelineConfig->copyTo(stateGroup);
    stateGroup->addChild(drawCommands);
    scene->addChild(stateGroup);
}

void CADMesh::buildSSAODenoiseData(vsg::ref_ptr<vsg::ShaderSet> model_shaderset, vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::ImageView> GBufferView0, vsg::ref_ptr<vsg::ImageView> SSAOResultImageView){
    auto rasterizationState = vsg::RasterizationState::create();
    rasterizationState->cullMode = VK_CULL_MODE_NONE;
    model_shaderset->defaultGraphicsPipelineStates.push_back(rasterizationState);
    auto graphicsPipelineConfig = vsg::GraphicsPipelineConfigurator::create(model_shaderset);
    graphicsPipelineConfig->subpass = 2;
    auto instance_buffer = vsg::mat4Array::create(2);
    instance_buffer->properties.dataVariance = vsg::DYNAMIC_DATA;
    for(int i = 0; i < 2; i ++){
        instance_buffer->set(i, vsg::mat4());
    }
    auto input_instance_buffer_info = vsg::BufferInfo::create(instance_buffer);

    // 2个mat4，1个int，3个padding int，一共36
    auto instance_data_buffer = vsg::floatArray::create(512);
    instance_data_buffer->set(0, 1);
    instance_data_buffer->set(5, 1);
    instance_data_buffer->set(10, 1);
    instance_data_buffer->set(15, 1);
    auto output_instance_buffer_info = vsg::BufferInfo::create(instance_data_buffer);

    vsg::BufferInfoList info_list = {output_instance_buffer_info};

    auto noiseSampler = vsg::Sampler::create();
    noiseSampler->magFilter = VK_FILTER_NEAREST;
    noiseSampler->minFilter = VK_FILTER_NEAREST;
    noiseSampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    noiseSampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    noiseSampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    noiseSampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    noiseSampler->maxAnisotropy = 1.0f;
    noiseSampler->minLod = 0.0f;
    noiseSampler->maxLod = 0.0f;

    graphicsPipelineConfig->assignDescriptor("instanceModelMatrix", info_list);
    graphicsPipelineConfig->assignTexture("cameraImage", camera_info);
    graphicsPipelineConfig->assignTexture("depthImage", depth_info);
    graphicsPipelineConfig->assignUniform("params", params);
    graphicsPipelineConfig->assignDescriptor("material", vsg::PbrMaterialValue::create());

    vsg::ref_ptr<vsg::Sampler> in_sampler{nullptr};
    vsg::ImageInfoList GBufferViewList0 = {vsg::ImageInfo::create(noiseSampler, GBufferView0, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)};
    vsg::ImageInfoList GBufferViewList1 = {vsg::ImageInfo::create(noiseSampler, SSAOResultImageView, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)};
    graphicsPipelineConfig->assignTexture("colorInputAttachment", GBufferViewList0);
    graphicsPipelineConfig->assignTexture("samplerSSAO", GBufferViewList1);

    // auto vertices = vsg::floatArray::create({-3, 1, 1, 1, 1, 1, 1, -3, 1});
    // auto normals = vsg::floatArray::create({0, 0, 1, 0, 0, 1, 0, 0, 1});
    // auto indices = vsg::intArray::create({0, 1, 2});
    auto vertices = vsg::vec3Array::create({
         vsg::vec3(-1, -1, 1.0f),
         vsg::vec3(1, -1, 1.0f),
         vsg::vec3(1, 1, 1.0f),
         vsg::vec3(-1, 1, 1.0f)
        });

    auto normals = vsg::vec3Array::create(
        {
            {1.0f, 1.0f, 1.0f},
            {1.0f, 1.0f, 1.0f},
            {1.0f, 1.0f, 1.0f},
            {1.0f, 1.0f, 1.0f}
        });

    auto texcoords = vsg::vec2Array::create(
        {{0.0f, 1.0f},
         {1.0f, 1.0f},
         {1.0f, 0.0f},
         {0.0f, 0.0f}
        });

    auto indices = vsg::ushortArray::create(
        {0, 1, 2,
         2, 3, 0});

    vsg::box bounds;
    for (uint32_t i = 0; i < vertices->size(); ++i)
    {
        bounds.add(vertices->at(i));
    }
    
    vsg::DataList vertexArrays;
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, vertices);
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Normal", VK_VERTEX_INPUT_RATE_VERTEX, normals);
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_TexCoord0", VK_VERTEX_INPUT_RATE_VERTEX, texcoords);
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, vsg::vec4Value::create(vsg::vec4{1.0f, 1.0f, 1.0f, 1.0f}));
    auto drawCommands = vsg::Commands::create();
    drawCommands->addChild(vsg::BindVertexBuffers::create(graphicsPipelineConfig->baseAttributeBinding, vertexArrays));
    drawCommands->addChild(vsg::BindIndexBuffer::create(indices));

    VkDrawIndexedIndirectCommand cmd = {
        6,      // indexCount
        1,     // instanceCount
        0,         // firstIndex
        0,         // vertexOffset
        0          // firstInstance
    };

    auto indirectBuffer = vsg::Array<VkDrawIndexedIndirectCommand>::create(1);
    indirectBuffer->set(0, cmd);
    auto draw_indirect = vsg::DrawIndexedIndirect::create(
        indirectBuffer,  // 间接命令缓冲区
        1,              // 绘制命令数量
        sizeof(VkDrawIndexedIndirectCommand) // 命令步长
    );
    draw_indirect->instanceMatrix = instance_buffer;
    drawCommands->addChild(draw_indirect);
    graphicsPipelineConfig->init();

    auto stateGroup = vsg::StateGroup::create();
    graphicsPipelineConfig->copyTo(stateGroup);
    stateGroup->addChild(drawCommands);
    scene->addChild(stateGroup);
}

void CADMesh::buildDynamicLinesData(vsg::ref_ptr<vsg::ShaderSet> model_shaderset, vsg::ref_ptr<vsg::Group> scene)
{
    dynamic_lines.vertices = vsg::vec3Array::create(20000); 
    dynamic_lines.vertices->properties.dataVariance = vsg::DataVariance::DYNAMIC_DATA;
    dynamic_lines.indices = vsg::uintArray::create(20000); 
    dynamic_lines.indices->properties.dataVariance = vsg::DataVariance::DYNAMIC_DATA;
    auto graphicsPipelineConfig = vsg::GraphicsPipelineConfigurator::create(model_shaderset);
    graphicsPipelineConfig->assignTexture("cameraImage", camera_info);
    graphicsPipelineConfig->assignTexture("depthImage", depth_info);
    graphicsPipelineConfig->assignUniform("params", params);//是否半透明判断
    
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

void CADMesh::buildDynamicPointsData(vsg::ref_ptr<vsg::ShaderSet> model_shaderset, vsg::ref_ptr<vsg::Group> scene)
{
    dynamic_points.vertices = vsg::vec3Array::create(20000); 
    dynamic_points.vertices->properties.dataVariance = vsg::DataVariance::DYNAMIC_DATA;
    dynamic_points.indices = vsg::uintArray::create(20000); 
    dynamic_points.indices->properties.dataVariance = vsg::DataVariance::DYNAMIC_DATA;
    auto graphicsPipelineConfig = vsg::GraphicsPipelineConfigurator::create(model_shaderset);
    graphicsPipelineConfig->assignTexture("cameraImage", camera_info);
    graphicsPipelineConfig->assignTexture("depthImage", depth_info);
    graphicsPipelineConfig->assignUniform("params", params);//是否半透明判断
    
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

