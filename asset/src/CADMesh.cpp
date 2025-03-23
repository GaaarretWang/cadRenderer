#include "CADMesh.h"
#include <chrono>
#include <iomanip>
#include <vsg/all.h>
#include <communication/dataInterface.h>

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

RGB CADMesh::hexToRGB(const std::string& color)
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

    return RGB{r, g, b};
}

void CADMesh::buildPlaneNode(vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::ShaderSet> shader, const vsg::dmat4& modelMatrix)
{
    //2. ���ò��ʲ���
#if (1) //����
    auto plane_colors = vsg::vec4Value::create(vsg::vec4{1.0, 1.0, 1.0, 1.0f});
    auto plane_mat = vsg::PhongMaterialValue::create();
    plane_mat->value().ambient.set(1, 1, 1, 1.0f);
    plane_mat->value().diffuse.set(1, 1, 1, 1.0f);
    plane_mat->value().specular.set(1, 1, 1, 1.0f);
    //std::cout << mat->value().shininess;//Ĭ��ֵΪ100
    plane_mat->value().shininess = 100;

#elif (0) //����
    auto plane_colors = vsg::vec4Value::create(vsg::vec4{0, 0, 0, 1.0f});
    auto plane_mat = vsg::PhongMaterialValue::create();
    plane_mat->value().ambient.set(0, 0, 0, 1.0f);
    plane_mat->value().diffuse.set(0.55, 0.55, 0.55, 1.0f);
    plane_mat->value().specular.set(0.7, 0.7, 0.7, 1.0f);
    plane_mat->value().shininess = 25;

#elif (0) //��ɫ
    auto plane_colors = vsg::vec4Value::create(vsg::vec4{1, 1, 1, 1.0f});
    auto plane_mat = vsg::PhongMaterialValue::create();
    plane_mat->value().ambient.set(1, 0, 0, 1.0f);
    plane_mat->value().diffuse.set(1, 0, 0, 1.0f);
    plane_mat->value().specular.set(1, 0, 0, 1.0f);
    plane_mat->value().shininess = 0.25;
#endif

    //    //3. ���ֶ������ö��㡢���򡢲����������Ϣ��������ɫ��
    float plane_length = 100.0f;
    auto plane_vertices = vsg::vec3Array::create(
        {{-plane_length, -plane_length, 0},
         {plane_length, -plane_length, 0},
         {plane_length, plane_length, 0},
         {-plane_length, plane_length, 0}});

    auto plane_normals = vsg::vec3Array::create(
        {{0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f}});

    auto plane_texcoords = vsg::vec2Array::create(
        {{0.0f, 0.0f},
         {1.0f, 0.0f},
         {1.0f, 1.0f},
         {0.0f, 1.0f}});

    auto plane_indices = vsg::ushortArray::create(
        {
            0,
            1,
            2,
            2,
            3,
            0,
        });

    //---------------------------------------��Ӱģ��------------------------------------------//
    //������д������Ĳ���
    vsg::DataList plane_vertexArrays;
    auto plane_graphicsPipelineConfig = vsg::GraphicsPipelineConfigurator::create(shader); //��Ⱦ���ߴ���
    plane_graphicsPipelineConfig->assignArray(plane_vertexArrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, plane_vertices);
    plane_graphicsPipelineConfig->assignArray(plane_vertexArrays, "vsg_Normal", VK_VERTEX_INPUT_RATE_VERTEX, plane_normals);
    plane_graphicsPipelineConfig->assignArray(plane_vertexArrays, "vsg_TexCoord0", VK_VERTEX_INPUT_RATE_VERTEX, plane_texcoords);
    plane_graphicsPipelineConfig->assignArray(plane_vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, plane_colors);
    plane_graphicsPipelineConfig->assignDescriptor("material", plane_mat);
    // ������
    auto plane_drawCommands = vsg::Commands::create();
    plane_drawCommands->addChild(vsg::BindVertexBuffers::create(plane_graphicsPipelineConfig->baseAttributeBinding, plane_vertexArrays));
    plane_drawCommands->addChild(vsg::BindIndexBuffer::create(plane_indices));
    //cout << mesh.indexes->size() << endl;
    plane_drawCommands->addChild(vsg::DrawIndexed::create(6, 1, 0, 0, 0));

    //˫����ʾ
    auto rs = vsg::RasterizationState::create();
    rs->cullMode = VK_CULL_MODE_NONE;
    plane_graphicsPipelineConfig->pipelineStates.push_back(rs);

    plane_graphicsPipelineConfig->init();

    // create StateGroup as the root of the scene/command graph to hold the GraphicsPipeline, and binding of Descriptors to decorate the whole graph
    auto plane_stateGroup = vsg::StateGroup::create();
    plane_graphicsPipelineConfig->copyTo(plane_stateGroup);
    // set up model transformation node
    auto plane_transform = vsg::MatrixTransform::create(); //����λ�ñ任
    plane_transform->matrix = modelMatrix;
    //transform->subgraphRequiresLocalFrustum = false;
    // add drawCommands to StateGroup
    plane_stateGroup->addChild(plane_drawCommands);
    plane_transform->addChild(plane_stateGroup);
    //scene->addChild(plane_transform); //*******************************************
}

void CADMesh::buildIntgNode(vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::ShaderSet> shader, vsg::ref_ptr<vsg::ImageInfo>* imageInfos, vsg::ref_ptr<vsg::Data> real_color, vsg::ref_ptr<vsg::Data> real_depth)
{
#if (1) //����
    auto colors = vsg::vec4Value::create(vsg::vec4{1.0, 1.0, 1.0, 1.0f});
    auto mat = vsg::PhongMaterialValue::create();
    mat->value().ambient.set(0.24725, 0.1995, 0.0745, 1.0f);
    mat->value().diffuse.set(0.75164, 0.60648, 0.22648, 1.0f);
    mat->value().specular.set(0.628281, 0.555802, 0.366065, 1.0f);
    //std::cout << mat->value().shininess;//Ĭ��ֵΪ100
    mat->value().shininess = 100;

#elif (0) //����
    auto colors = vsg::vec4Value::create(vsg::vec4{0, 0, 0, 1.0f});
    auto mat = vsg::PhongMaterialValue::create();
    mat->value().ambient.set(0, 0, 0, 1.0f);
    mat->value().diffuse.set(0.55, 0.55, 0.55, 1.0f);
    mat->value().specular.set(0.7, 0.7, 0.7, 1.0f);
    mat->value().shininess = 25;

#elif (0) //��ɫ
    auto colors = vsg::vec4Value::create(vsg::vec4{1, 1, 1, 1.0f});
    auto mat = vsg::PhongMaterialValue::create();
    mat->value().ambient.set(1, 0, 0, 1.0f);
    mat->value().diffuse.set(1, 0, 0, 1.0f);
    mat->value().specular.set(1, 0, 0, 1.0f);
    mat->value().shininess = 0.25;
#endif

    //    //3. ���ֶ������ö��㡢���򡢲����������Ϣ��������ɫ��
    float length = 5000.0f;
    auto vertices = vsg::vec3Array::create(
        {{-length, -length, -length / 10},
         {length, -length, -length / 10},
         {length, length, -length / 10},
         {-length, length, -length / 10}});

    auto normals = vsg::vec3Array::create(
        {{0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f},
         {0.0f, 0.0f, 1.0f}});

    auto texcoords = vsg::vec2Array::create(
        {{0.0f, 0.0f},
         {1.0f, 0.0f},
         {1.0f, 1.0f},
         {0.0f, 1.0f}});

    auto indices = vsg::ushortArray::create(
        {
            0,
            1,
            2,
            2,
            3,
            0,
        });

    vsg::DataList Env_vertexArrays;
    auto Env_graphicsPipelineConfig = vsg::GraphicsPipelineConfigurator::create(shader); //��Ⱦ���ߴ���
    vsg::ImageInfoList cadColor = {imageInfos[0]};
    vsg::ImageInfoList cadDepth = {imageInfos[1]};
    vsg::ImageInfoList planeColor = {imageInfos[2]};
    vsg::ImageInfoList planeDepth = {imageInfos[3]};
    vsg::ImageInfoList shadowColor = {imageInfos[4]};
    vsg::ImageInfoList shadowDepth = {imageInfos[5]};

    //vsg::Data and imageinfos should be consistent
    Env_graphicsPipelineConfig->assignTexture("cadColor", cadColor);
    Env_graphicsPipelineConfig->assignTexture("cadDepth", cadDepth);
    Env_graphicsPipelineConfig->assignTexture("shadowColor", shadowColor);
    Env_graphicsPipelineConfig->assignTexture("shadowDepth", planeDepth);
    Env_graphicsPipelineConfig->assignTexture("planeColor", real_color);
    Env_graphicsPipelineConfig->assignTexture("planeDepth", real_depth);
    // Env_graphicsPipelineConfig->assignTexture("planeColor", planeColor);
    // Env_graphicsPipelineConfig->assignTexture("planeDepth", planeDepth);

    Env_graphicsPipelineConfig->assignArray(Env_vertexArrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, vertices);
    Env_graphicsPipelineConfig->assignArray(Env_vertexArrays, "vsg_Normal", VK_VERTEX_INPUT_RATE_VERTEX, normals);
    Env_graphicsPipelineConfig->assignArray(Env_vertexArrays, "vsg_TexCoord0", VK_VERTEX_INPUT_RATE_VERTEX, texcoords);
    Env_graphicsPipelineConfig->assignArray(Env_vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, colors);
    Env_graphicsPipelineConfig->assignDescriptor("material", mat);

    //Env_graphicsPipelineConfig->assignTexture("", images[1]->data);
    //Env_graphicsPipelineConfig->assignTexture("", images[2]->data);
    //Env_graphicsPipelineConfig->assignTexture("", images[3]->data);
    //Env_graphicsPipelineConfig->assignTexture("", images[4]->data);
    // ������
    auto Env_drawCommands = vsg::Commands::create();
    Env_drawCommands->addChild(vsg::BindVertexBuffers::create(Env_graphicsPipelineConfig->baseAttributeBinding, Env_vertexArrays));
    Env_drawCommands->addChild(vsg::BindIndexBuffer::create(indices));
    //cout << mesh.indexes->size() << endl;
    Env_drawCommands->addChild(vsg::DrawIndexed::create(6, 1, 0, 0, 0));

    //˫����ʾ
    auto Env_rs = vsg::RasterizationState::create();
    Env_rs->cullMode = VK_CULL_MODE_NONE;
    Env_graphicsPipelineConfig->pipelineStates.push_back(Env_rs);

    Env_graphicsPipelineConfig->init();

    // create StateGroup as the root of the scene/command graph to hold the GraphicsPipeline, and binding of Descriptors to decorate the whole graph
    auto Env_stateGroup = vsg::StateGroup::create();
    Env_graphicsPipelineConfig->copyTo(Env_stateGroup);
    // set up model transformation node
    auto Env_transform = vsg::MatrixTransform::create(); //����λ�ñ任
    //transform->subgraphRequiresLocalFrustum = false;
    // add drawCommands to StateGroup
    Env_stateGroup->addChild(Env_drawCommands);
    Env_transform->addChild(Env_stateGroup);
    //scene->addChild(Env_transform);
}

void CADMesh::buildNewNode(const std::string& path, bool fullNormal, vsg::ref_ptr<vsg::Group> scene)
{

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
        size_t lastSlash = path.find_last_of("/\\");
        std::string fbFilePath = path.substr(0, lastSlash);
        std::string fbFileName = path.substr(lastSlash + 1);
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
	pmi = datainterface.getPmiInfos(false);
	auto instances = datainterface.getInstances();
	auto instanceInfos = datainterface.getInstanceInfos();
	std::string fbModelData = datainterface.getModelFlatbuffersData();

    uint8_t* buffer_data;
    int buffer_size;
    for (auto it = MapInfo.begin(); it != MapInfo.end(); ++it){
        auto info = it->second;
        // std::cout << it->first << std::endl;
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

            std::string testcolor = color.substr(1);
            // 将 hex 转换为 RGB
            int red = std::stoi(testcolor.substr(0, 2), nullptr, 16);
            int green = std::stoi(testcolor.substr(2, 2), nullptr, 16);
            int blue = std::stoi(testcolor.substr(4, 2), nullptr, 16);

            // 将 RGB 转换为 0.0 到 1.0 之间的浮点数
            float r = red / 255.0f;
            float g = green / 255.0f;
            float b = blue / 255.0f;

            //设置材质参数
            vsg::ref_ptr<vsg::PbrMaterialValue> default_material;
            default_material = vsg::PbrMaterialValue::create();

            default_material->value().baseColorFactor.set(r, g, b, opacity);
            default_material->value().metallicFactor = 0.6f;
            default_material->value().diffuseFactor.set(0.1f, 0.1f, 0.1f, 1.0f);
            default_material->value().roughnessFactor = 0.0f;
            default_material->value().specularFactor.set(0.7f, 0.7f, 0.7f, 1.0f);
            default_material->value().emissiveFactor.set(0.0f, 0.0f, 0.0f, 0.0f);
            //default_material->value().roughnessFactor = roughness;
            default_material->value().alphaMaskCutoff = 0.0f;
            /*
            */
            if (type == "face")
            {
                for (int i = 0; i < modelIndex.size(); i += 1)
                {
                    TinyModelVertex vertex;
                    int index = modelIndex.at(i);
                    vertex.pos = toNewVec3(&position, index * 3);
                    vertex.normal = toNewVec3(&normal, index * 3);
                    vertex.uv = toNewVec2(&uv, index * 2);

                    if (uniqueVertices.count(vertex) == 0) //if unique 唯一
                    {                                      //push进数组。记录位置
                        uniqueVertices[vertex] = static_cast<uint32_t>(mVertices.size());
                        mVertices.push_back(vertex);
                    }
                    mIndices.push_back(uniqueVertices[vertex]); //根据新proto的数组，索引位置改变
                }

                int Nodenumber = mVertices.size();   //顶点、法向、UV个数
                int Indicesnumber = mIndices.size(); //索引个数

                vsg::ref_ptr<vsg::vec3Array> vertices = vsg::vec3Array::create(Nodenumber); //分配数组空间
                vsg::ref_ptr<vsg::vec3Array> normals = vsg::vec3Array::create(Nodenumber);
                vsg::ref_ptr<vsg::vec2Array> uvs = vsg::vec2Array::create(Nodenumber);
                vsg::ref_ptr<vsg::uintArray> indices = vsg::uintArray::create(Indicesnumber);

                //读取顶点，保存成vsg数组形式
                for (int i = 0; i < Nodenumber; i++)
                {
                    vertices->at(i) = vsg::vec3(mVertices[i].pos);
                    normals->at(i) = vsg::vec3(mVertices[i].normal);
                    uvs->at(i) = vsg::vec2(mVertices[i].uv);
                }
                //读取索引
                for (int i = 0; i < Indicesnumber; i++)
                {
                    indices->at(i) = mIndices[i];
                }
                //以零件为单位来进行绘制，每个零件都有单独的数据
                verticesVector.push_back(vertices);
                normalsVector.push_back(normals);
                UVVector.push_back(uvs);
                indicesVector.push_back(indices);
                materialVector.push_back(default_material);
                materialNameVector.push_back(material);
                transformVector.push_back(matrix);
                transformNumVector.push_back(num);
            }
        }

    }

    for (int i = 0; i < indicesVector.size(); i++)
    {

        //创建纹理或遮罩
        bool addTexture = 0;
        auto options = vsg::Options::create();
        vsg::Path textureFile("../data/textures/lz.vsgb");
        if (textureFile && addTexture)
        {
            auto textureData = vsg::read_cast<vsg::Data>(textureFile, options);
            if (!textureData)
            {
                std::cout << "Could not read texture file : " << textureFile << std::endl;
            }
            //graphicsPipelineConfig->assignTexture("diffuseMap", textureData);
        }

    }
}

// void CADMesh::transferModel(const std::string& path, bool fullNormal, const vsg::dmat4& modelMatrix){
//     uint8_t* buffer_data;
//     int buffer_size;
//     std::ifstream file(path, std::ios::binary);
//     if (!file.is_open())
//     {
//         std::cerr << "Failed to open the file." << std::endl;
//     }
//     file.seekg(0, std::ios::end);
//     std::streampos fileSize = file.tellg();
//     file.seekg(0, std::ios::beg);
//     std::vector<uint8_t> buffer(fileSize);
//     file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
//     file.close();
//     buffer_data = buffer.data();
//     buffer_size = fileSize;

//     builder_out.PushFlatBuffer(buffer_data, buffer_size);
//     auto renderFlatBuffer = RenderFlatBuffer::GetRenderFlatBufferDoc(builder_out.GetBufferPointer());
//     int emptyProtoNum = 0;

//     for (int p = 0; p < renderFlatBuffer->ProtoData()->size(); p++)
//     {
//         //std::cout << "proto" << p << ":" << std::endl;
//         const RenderFlatBuffer::Proto* protofbs = renderFlatBuffer->ProtoData()->Get(p);
//         if (protofbs->Models()->size() == 0)
//         {
//             emptyProtoNum++;
//             continue;
//         }
//         protoIndex[protofbs->ProtoID()->str()] = static_cast<uint32_t>(p - emptyProtoNum);
//         std::unordered_map<TinyModelVertex, uint32_t> uniqueVertices;
//         std::vector<TinyModelVertex> mVertices{};
//         std::vector<uint32_t> mIndices{};

//         for (int m = 0; m < protofbs->Models()->size(); m++)
//         {
//             const RenderFlatBuffer::Model* modelfbs = protofbs->Models()->Get(m);
//             auto modelGeo = modelfbs->Geo();
//             //std::cout << "modelfbs->Type()->str()" << modelfbs->Type()->str() << std::endl;
//             if (modelfbs->Type()->str() == "mesh")
//             {
//                 if (fullNormal)
//                 {
//                     for (int i = 0; i < modelGeo->Index()->size(); i += 1)
//                     {
//                         TinyModelVertex vertex;
//                         int index = modelGeo->Index()->Get(i);
//                         vertex.pos = toVec3(modelGeo->Position(), index * 3);
//                         vertex.normal = toVec3(modelGeo->Normal(), index * 3);
//                         // vertex.uv = toVec2(modelGeo->UV(), index * 2);

//                         if (uniqueVertices.count(vertex) == 0) //if unique
//                         {
//                             uniqueVertices[vertex] = static_cast<uint32_t>(mVertices.size());
//                             mVertices.push_back(vertex);
//                         }
//                         mIndices.push_back(uniqueVertices[vertex]);
//                     }
//                 }
//                 else
//                 {
//                     for (int i = 0; i < modelGeo->Index()->size(); i += 3)
//                     {
//                         std::vector<TinyModelVertex> vertex = {};
//                         for (int j = 0; j < 3; j++)
//                         {
//                             TinyModelVertex v;
//                             int index = modelGeo->Index()->Get(i + j);
//                             // std::cout <<index << "\t";
//                             v.pos = toVec3(modelGeo->Position(), index * 3);
//                             // vertex.uv = toVec2(modelGeo->UV(), index * 2);
//                             vertex.push_back(v);
//                         }
//                         vsg::vec3 n = vsg::normalize(vsg::cross((vertex[1].pos - vertex[0].pos), (vertex[2].pos - vertex[0].pos)));
//                         vertex[0].normal = n;
//                         vertex[1].normal = n;
//                         vertex[2].normal = n;

//                         for (int j = 0; j < 3; j++)
//                         {
//                             if (uniqueVertices.count(vertex[j]) == 0) //if unique
//                             {
//                                 uniqueVertices[vertex[j]] = static_cast<uint32_t>(mVertices.size());
//                                 mVertices.push_back(vertex[j]);
//                             }
//                             mIndices.push_back(uniqueVertices[vertex[j]]);
//                         }
//                     }
//                 }
//             }
//         }
//         int Nodenumber = mVertices.size();   //���㡢�������
//         int Indicesnumber = mIndices.size(); //��������
//         protoTriangleNum[protofbs->ProtoID()->str()] = static_cast<uint32_t>(mIndices.size() / 3);

//         vsg::ref_ptr<vsg::vec3Array> vertices = vsg::vec3Array::create(Nodenumber);
//         vsg::ref_ptr<vsg::vec3Array> normals = vsg::vec3Array::create(Nodenumber);
//         //coordinates = vsg::vec2Array::create(Nodenumber);
//         vsg::ref_ptr<vsg::uintArray> indices = vsg::uintArray::create(Indicesnumber);

//         //��ȡ����
//         for (int i = 0; i < Nodenumber; i++)
//         {
//             vertices->at(i) = vsg::vec3(mVertices[i].pos);
//             normals->at(i) = vsg::vec3(mVertices[i].normal);
//         }
//         //��ȡ����
//         for (int i = 0; i < Indicesnumber; i++)
//         {
//             indices->at(i) = mIndices[i];
//         }
//         verticesVector.push_back(vertices);
//         normalsVector.push_back(normals);
//         indicesVector.push_back(indices);
//     }
// }

// void CADMesh::buildInstance(const std::string& path, bool fullNormal, vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::ShaderSet> shader, const vsg::dmat4& modelMatrix){
//     vsg::ref_ptr<vsg::vec4Value> default_color;
//     vsg::ref_ptr<vsg::PhongMaterialValue> default_material;
//     default_color = vsg::vec4Value::create(vsg::vec4{0.9, 0.9, 0.9, 1.0});
//     default_material = vsg::PhongMaterialValue::create();
//     default_material->value().ambient.set(0.9, 0.9, 0.9, 1.0);
//     default_material->value().diffuse.set(0.55, 0.55, 0.55, 1.0);
//     default_material->value().specular.set(0.7, 0.7, 0.7, 1.0);
//     //std::cout << mat->value().shininess;//Ĭ��ֵΪ100
//     default_material->value().shininess = 25;

//     vsg::ref_ptr<vsg::vec4Value> highlighted_color;
//     vsg::ref_ptr<vsg::PhongMaterialValue> highlighted_material;
//     highlighted_color = vsg::vec4Value::create(vsg::vec4{1.0, 0.0, 0.0, 1.0});
//     highlighted_material = vsg::PhongMaterialValue::create();
//     highlighted_material->value().ambient.set(1.0, 0.0, 0.0, 1.0);
//     highlighted_material->value().diffuse.set(1.0, 0.0, 0.0, 1.0);
//     highlighted_material->value().specular.set(1.0, 0.0, 0.0, 1.0);
//     //std::cout << mat->value().shininess;//Ĭ��ֵΪ100
//     highlighted_material->value().shininess = 25;

//     auto renderFlatBuffer = RenderFlatBuffer::GetRenderFlatBufferDoc(builder_out.GetBufferPointer());
//     for (int i = 0; i < renderFlatBuffer->Bom()->size(); i++)
//     {
//         auto instance = renderFlatBuffer->Bom()->Get(i);
//         instanceIndex[instance->InstanceID()->str()] = i;
//     }

//     treeNode top;
//     top.transform = vsg::MatrixTransform::create();
//     top.transform->matrix = modelMatrix;
//     top.originalMatrix = modelMatrix;
//     nodePtr[""] = top;

//     //�����������нڵ�
//     for (int i = 0; i < renderFlatBuffer->Bom()->size(); i++)
//     {

//         auto instance = renderFlatBuffer->Bom()->Get(i);
//         auto protoId = instance->ProtoID()->str();
//         auto currentId = instance->InstanceID()->str();
//         if (protoIndex.find(protoId) != protoIndex.end())
//         {
//             auto graphicsPipelineConfig = vsg::GraphicsPipelineConfigurator::create(shader);
//             vsg::DataList vertexArrays;
//             auto drawCommands = vsg::Commands::create();

//             //��������������
//             bool addTexture = 0;
//             auto options = vsg::Options::create();
//             vsg::Path textureFile("../asset/data/textures/lz.vsgb");
//             if (textureFile && addTexture)
//             {
//                 auto textureData = vsg::read_cast<vsg::Data>(textureFile, options);
//                 if (!textureData)
//                 {
//                     std::cout << "Could not read texture file : " << textureFile << std::endl;
//                 }
//                 graphicsPipelineConfig->assignTexture("diffuseMap", textureData);
//             }

//             //����ģ�ͼ��β���
//             graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, verticesVector[protoIndex[instance->ProtoID()->str()]]);
//             graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Normal", VK_VERTEX_INPUT_RATE_VERTEX, normalsVector[protoIndex[instance->ProtoID()->str()]]);
//             //graphicsPipelineConfig->assignArray(vertexArrays, "vsg_TexCoord0", VK_VERTEX_INPUT_RATE_VERTEX, texcoords);

//             graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, default_color);
//             graphicsPipelineConfig->assignDescriptor("material", default_material);
//             //������
//             drawCommands->addChild(vsg::BindVertexBuffers::create(graphicsPipelineConfig->baseAttributeBinding, vertexArrays));
//             drawCommands->addChild(vsg::BindIndexBuffer::create(indicesVector[protoIndex[instance->ProtoID()->str()]]));
//             drawCommands->addChild(vsg::DrawIndexed::create(indicesVector[protoIndex[instance->ProtoID()->str()]]->size(), 1, 0, 0, 0));

//             graphicsPipelineConfig->init();
//             auto stateGroup = vsg::StateGroup::create();
//             graphicsPipelineConfig->copyTo(stateGroup);
//             stateGroup->addChild(drawCommands);

//             treeNode node;
//             node.transform = vsg::MatrixTransform::create();
//             for (int m = 0; m < 4; m++)
//                 for (int n = 0; n < 3; n++)
//                 {
//                     node.transform->matrix[m][n] = instance->Matrix()->Get(3 * m + n);
//                     node.originalMatrix[m][n] = node.transform->matrix[m][n];
//                 }
//             node.transform->addChild(stateGroup);

//             nodePtr[currentId] = node;
//         }
//         else
//         {
//             vsg::mat4 matrix = vsg::mat4(1.0f);
//             for (int m = 0; m < 4; m++)
//                 for (int n = 0; n < 3; n++)
//                 {
//                     matrix[m][n] = instance->Matrix()->Get(3 * m + n);
//                 }
//             treeNode node;
//             node.transform = vsg::MatrixTransform::create();
//             node.transform->matrix = matrix;
//             node.originalMatrix = matrix;
//             nodePtr[currentId] = node;
//         }
//     }

//     //�������нڵ�Ĺ�ϵ
//     uint32_t totalTriangleNum = 0;
//     std::unordered_set<std::string> added;                                              //��¼���ӵ����еĽڵ�
//     for (int i = 0; i < renderFlatBuffer->Bom()->size(); i++)
//     {
//         auto instance = renderFlatBuffer->Bom()->Get(i);
//         auto currentId = instance->InstanceID()->str();
//         auto parentId = instance->ParentID()->str();
//         auto protoId = instance->ProtoID()->str();
//         if (protoIndex.find(protoId) != protoIndex.end())
//         {
//             totalTriangleNum += protoTriangleNum[protoId];
//             while (true)
//             {
//                 if (added.find(currentId) == added.end())
//                 {
//                     nodePtr[parentId].kids.push_back(currentId);
//                     nodePtr[parentId].transform->addChild(nodePtr[currentId].transform);
//                     added.insert(currentId);
//                 }
//                 if (parentId == "")
//                     break;
//                 currentId = parentId;
//                 parentId = renderFlatBuffer->Bom()->Get(instanceIndex[currentId])->ParentID()->str();
//             }
//         }
//     }

//     for(int i = 0; i < nodePtr[""].kids.size(); i ++){
//         std::cout << nodePtr[""].kids[i] << std::endl;
//     }

//     std::cout << "totalTriangleNum = " << totalTriangleNum << std::endl;
//     //scene->addChild(nodePtr[""].transform);
// }

// void CADMesh::buildnode(const std::string& path, bool fullNormal, vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::ShaderSet> shader, const vsg::dmat4& modelMatrix)
// {
//     vsg::ref_ptr<vsg::vec4Value> default_color;
//     vsg::ref_ptr<vsg::PhongMaterialValue> default_material;
//     default_color = vsg::vec4Value::create(vsg::vec4{0.9, 0.9, 0.9, 1.0});
//     default_material = vsg::PhongMaterialValue::create();
//     default_material->value().ambient.set(0.9, 0.9, 0.9, 1.0);
//     default_material->value().diffuse.set(0.55, 0.55, 0.55, 1.0);
//     default_material->value().specular.set(0.7, 0.7, 0.7, 1.0);
//     //std::cout << mat->value().shininess;//Ĭ��ֵΪ100
//     default_material->value().shininess = 25;

//     vsg::ref_ptr<vsg::vec4Value> highlighted_color;
//     vsg::ref_ptr<vsg::PhongMaterialValue> highlighted_material;
//     highlighted_color = vsg::vec4Value::create(vsg::vec4{1.0, 0.0, 0.0, 1.0});
//     highlighted_material = vsg::PhongMaterialValue::create();
//     highlighted_material->value().ambient.set(1.0, 0.0, 0.0, 1.0);
//     highlighted_material->value().diffuse.set(1.0, 0.0, 0.0, 1.0);
//     highlighted_material->value().specular.set(1.0, 0.0, 0.0, 1.0);
//     //std::cout << mat->value().shininess;//Ĭ��ֵΪ100
//     highlighted_material->value().shininess = 25;

//     uint8_t* buffer_data;
//     int buffer_size;
//     std::ifstream file(path, std::ios::binary);
//     if (!file.is_open())
//     {
//         std::cerr << "Failed to open the file." << std::endl;
//     }
//     file.seekg(0, std::ios::end);
//     std::streampos fileSize = file.tellg();
//     file.seekg(0, std::ios::beg);
//     std::vector<uint8_t> buffer(fileSize);
//     file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
//     file.close();
//     buffer_data = buffer.data();
//     buffer_size = fileSize;

//     builder_out.PushFlatBuffer(buffer_data, buffer_size);
//     auto renderFlatBuffer = RenderFlatBuffer::GetRenderFlatBufferDoc(builder_out.GetBufferPointer());
//     int emptyProtoNum = 0;

//     for (int p = 0; p < renderFlatBuffer->ProtoData()->size(); p++)
//     {
//         //std::cout << "proto" << p << ":" << std::endl;
//         const RenderFlatBuffer::Proto* protofbs = renderFlatBuffer->ProtoData()->Get(p);
//         if (protofbs->Models()->size() == 0)
//         {
//             emptyProtoNum++;
//             continue;
//         }
//         protoIndex[protofbs->ProtoID()->str()] = static_cast<uint32_t>(p - emptyProtoNum);
//         std::unordered_map<TinyModelVertex, uint32_t> uniqueVertices;
//         std::vector<TinyModelVertex> mVertices{};
//         std::vector<uint32_t> mIndices{};

//         for (int m = 0; m < protofbs->Models()->size(); m++)
//         {
//             const RenderFlatBuffer::Model* modelfbs = protofbs->Models()->Get(m);
//             auto modelGeo = modelfbs->Geo();
//             //std::cout << "modelfbs->Type()->str()" << modelfbs->Type()->str() << std::endl;
//             if (modelfbs->Type()->str() == "mesh")
//             {
//                 if (fullNormal)
//                 {
//                     for (int i = 0; i < modelGeo->Index()->size(); i += 1)
//                     {
//                         TinyModelVertex vertex;
//                         int index = modelGeo->Index()->Get(i);
//                         vertex.pos = toVec3(modelGeo->Position(), index * 3);
//                         vertex.normal = toVec3(modelGeo->Normal(), index * 3);
//                         // vertex.uv = toVec2(modelGeo->UV(), index * 2);

//                         if (uniqueVertices.count(vertex) == 0) //if unique
//                         {
//                             uniqueVertices[vertex] = static_cast<uint32_t>(mVertices.size());
//                             mVertices.push_back(vertex);
//                         }
//                         mIndices.push_back(uniqueVertices[vertex]);
//                     }
//                 }
//                 else
//                 {
//                     for (int i = 0; i < modelGeo->Index()->size(); i += 3)
//                     {
//                         std::vector<TinyModelVertex> vertex = {};
//                         for (int j = 0; j < 3; j++)
//                         {
//                             TinyModelVertex v;
//                             int index = modelGeo->Index()->Get(i + j);
//                             // std::cout <<index << "\t";
//                             v.pos = toVec3(modelGeo->Position(), index * 3);
//                             // vertex.uv = toVec2(modelGeo->UV(), index * 2);
//                             vertex.push_back(v);
//                         }
//                         vsg::vec3 n = vsg::normalize(vsg::cross((vertex[1].pos - vertex[0].pos), (vertex[2].pos - vertex[0].pos)));
//                         vertex[0].normal = n;
//                         vertex[1].normal = n;
//                         vertex[2].normal = n;

//                         for (int j = 0; j < 3; j++)
//                         {
//                             if (uniqueVertices.count(vertex[j]) == 0) //if unique
//                             {
//                                 uniqueVertices[vertex[j]] = static_cast<uint32_t>(mVertices.size());
//                                 mVertices.push_back(vertex[j]);
//                             }
//                             mIndices.push_back(uniqueVertices[vertex[j]]);
//                         }
//                     }
//                 }
//             }
//         }
//         int Nodenumber = mVertices.size();   //���㡢�������
//         int Indicesnumber = mIndices.size(); //��������
//         protoTriangleNum[protofbs->ProtoID()->str()] = static_cast<uint32_t>(mIndices.size() / 3);

//         vsg::ref_ptr<vsg::vec3Array> vertices = vsg::vec3Array::create(Nodenumber);
//         vsg::ref_ptr<vsg::vec3Array> normals = vsg::vec3Array::create(Nodenumber);
//         //coordinates = vsg::vec2Array::create(Nodenumber);
//         vsg::ref_ptr<vsg::uintArray> indices = vsg::uintArray::create(Indicesnumber);

//         //��ȡ����
//         for (int i = 0; i < Nodenumber; i++)
//         {
//             vertices->at(i) = vsg::vec3(mVertices[i].pos);
//             normals->at(i) = vsg::vec3(mVertices[i].normal);
//         }
//         //��ȡ����
//         for (int i = 0; i < Indicesnumber; i++)
//         {
//             indices->at(i) = mIndices[i];
//         }
//         verticesVector.push_back(vertices);
//         normalsVector.push_back(normals);
//         indicesVector.push_back(indices);
//     }

//     for (int i = 0; i < renderFlatBuffer->Bom()->size(); i++)
//     {
//         auto instance = renderFlatBuffer->Bom()->Get(i);
//         instanceIndex[instance->InstanceID()->str()] = i;
//     }

//     vsg::mat4 matrix = vsg::mat4(1.0f);
//     treeNode top;
//     top.transform = vsg::MatrixTransform::create();
//     top.transform->matrix = modelMatrix;
//     top.originalMatrix = modelMatrix;
//     nodePtr[""] = top;

//     //�����������нڵ�
//     for (int i = 0; i < renderFlatBuffer->Bom()->size(); i++)
//     {

//         auto instance = renderFlatBuffer->Bom()->Get(i);
//         auto protoId = instance->ProtoID()->str();
//         auto currentId = instance->InstanceID()->str();
//         if (protoIndex.find(protoId) != protoIndex.end())
//         {
//             auto graphicsPipelineConfig = vsg::GraphicsPipelineConfigurator::create(shader);
//             vsg::DataList vertexArrays;
//             auto drawCommands = vsg::Commands::create();

//             //��������������
//             bool addTexture = 0;
//             auto options = vsg::Options::create();
//             vsg::Path textureFile("../asset/data/textures/lz.vsgb");
//             if (textureFile && addTexture)
//             {
//                 auto textureData = vsg::read_cast<vsg::Data>(textureFile, options);
//                 if (!textureData)
//                 {
//                     std::cout << "Could not read texture file : " << textureFile << std::endl;
//                 }
//                 graphicsPipelineConfig->assignTexture("diffuseMap", textureData);
//             }

//             //����ģ�ͼ��β���
//             graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, verticesVector[protoIndex[instance->ProtoID()->str()]]);
//             graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Normal", VK_VERTEX_INPUT_RATE_VERTEX, normalsVector[protoIndex[instance->ProtoID()->str()]]);
//             //graphicsPipelineConfig->assignArray(vertexArrays, "vsg_TexCoord0", VK_VERTEX_INPUT_RATE_VERTEX, texcoords);

//             graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, default_color);
//             graphicsPipelineConfig->assignDescriptor("material", default_material);
//             //������
//             drawCommands->addChild(vsg::BindVertexBuffers::create(graphicsPipelineConfig->baseAttributeBinding, vertexArrays));
//             drawCommands->addChild(vsg::BindIndexBuffer::create(indicesVector[protoIndex[instance->ProtoID()->str()]]));
//             drawCommands->addChild(vsg::DrawIndexed::create(indicesVector[protoIndex[instance->ProtoID()->str()]]->size(), 1, 0, 0, 0));

//             graphicsPipelineConfig->init();
//             auto stateGroup = vsg::StateGroup::create();
//             graphicsPipelineConfig->copyTo(stateGroup);
//             stateGroup->addChild(drawCommands);

//             treeNode node;
//             node.transform = vsg::MatrixTransform::create();
//             for (int m = 0; m < 4; m++)
//                 for (int n = 0; n < 3; n++)
//                 {
//                     node.transform->matrix[m][n] = instance->Matrix()->Get(3 * m + n);
//                     node.originalMatrix[m][n] = node.transform->matrix[m][n];
//                 }
//             node.transform->addChild(stateGroup);

//             nodePtr[currentId] = node;
//         }
//         else
//         {
//             vsg::mat4 matrix = vsg::mat4(1.0f);
//             for (int m = 0; m < 4; m++)
//                 for (int n = 0; n < 3; n++)
//                 {
//                     matrix[m][n] = instance->Matrix()->Get(3 * m + n);
//                 }
//             treeNode node;
//             node.transform = vsg::MatrixTransform::create();
//             node.transform->matrix = matrix;
//             node.originalMatrix = matrix;
//             nodePtr[currentId] = node;
//         }
//     }

//     //�������нڵ�Ĺ�ϵ
//     uint32_t totalTriangleNum = 0;
//     std::unordered_set<std::string> added;                                              //��¼���ӵ����еĽڵ�
//     for (int i = 0; i < renderFlatBuffer->Bom()->size(); i++)
//     {
//         auto instance = renderFlatBuffer->Bom()->Get(i);
//         auto currentId = instance->InstanceID()->str();
//         auto parentId = instance->ParentID()->str();
//         auto protoId = instance->ProtoID()->str();
//         if (protoIndex.find(protoId) != protoIndex.end())
//         {
//             totalTriangleNum += protoTriangleNum[protoId];
//             while (true)
//             {
//                 if (added.find(currentId) == added.end())
//                 {
//                     nodePtr[parentId].kids.push_back(currentId);
//                     nodePtr[parentId].transform->addChild(nodePtr[currentId].transform);
//                     added.insert(currentId);
//                 }
//                 if (parentId == "")
//                     break;
//                 currentId = parentId;
//                 parentId = renderFlatBuffer->Bom()->Get(instanceIndex[currentId])->ParentID()->str();
//             }
//         }
//     }

//     std::cout << "totalTriangleNum = " << totalTriangleNum << std::endl;
//     //scene->addChild(nodePtr[""].transform);
// #ifdef EXPLODE
//     //����bounding box
//     for (int i = 0; i < renderFlatBuffer->Bom()->size(); i++)
//     {
//         auto instance = renderFlatBuffer->Bom()->Get(i);
//         auto currentId = instance->InstanceID()->str();
//         nodePtr[currentId].bounds = vsg::visit<vsg::ComputeBounds>(nodePtr[currentId].transform).bounds;
//     }
//     nodePtr[""].bounds = vsg::visit<vsg::ComputeBounds>(nodePtr[""].transform).bounds;
// #endif
// }

void CADMesh::buildObjNode(const char* model_path, const char* material_path, const vsg::dmat4& modelMatrix)
{
    assert(model_path != nullptr);
    OBJLoader objLoader;
    //设置材质参数
    vsg::ref_ptr<vsg::vec4Value> default_color;
    vsg::ref_ptr<vsg::PhongMaterialValue> default_material;
    default_color = vsg::vec4Value::create(vsg::vec4{ 0.9, 0.9, 0.9, 1.0 });
    default_material = vsg::PhongMaterialValue::create();
    default_material->value().ambient.set(0.9, 0.9, 0.9, 1.0);
    default_material->value().diffuse.set(0.55, 0.55, 0.55, 1.0);
    default_material->value().specular.set(0.7, 0.7, 0.7, 1.0);
    //std::cout << mat->value().shininess;//默认值为100
    default_material->value().shininess = 25;

    std::unordered_map<std::string, int> num = objLoader.vertex_count(model_path);

    auto vertices = vsg::vec3Array::create(num["vertices"]); 
    auto normals = vsg::vec3Array::create(num["normals"]);
    auto materials = vsg::PbrMaterialArray::create(100);

    auto options = vsg::Options::create();
    options->add(vsgXchange::all::create());
    auto textureData = vsg::read_cast<vsg::Data>("../asset/data/textures/env_outdoor.png", options);
    if(!textureData){
        std::cout << "Could not read texture file " << std::endl;
    }

    std::cout <<"success creating obj"<<std::endl;

    std::vector<vsg::ref_ptr<vsg::uintArray>> indices;// = vsg::uintArray::create(num["indices"]);
    vsg::ref_ptr<vsg::vec2Array> verticesUV = vsg::vec2Array::create(num["uvs"]);
    vsg::ref_ptr<vsg::vec3Array> colors;
    std::vector<std::string> textures;
    std::vector<int> mtr_ids;
    objLoader.load_obj(model_path, material_path, vertices, normals, verticesUV, colors, materials, indices, textures, mtr_ids);
    //std::cout <<"success Loading obj "<<material_path<<std::endl;
    for(int i = 0; i < vertices->size(); i++){
        vertices->at(i) *= 200;
        vertices->at(i).y -= 2000;
    }
    for(int i=0;i<verticesUV->size();i++){
        //std::cout<<verticesUV->at(i).x<<"  ";
    }
    objVerticesVector.push_back(vertices);
    objNormalsVector.push_back(normals);
    objUVVector.push_back(verticesUV);
    objIndicesVector.push_back(indices);
    objMaterialVector.push_back(materials);
    objTexturePath.push_back(textures);
    objMaterialIndice.push_back(mtr_ids);
    //std::cout << material << std::endl;
}

// void CADMesh::explode()
// {
//     auto renderFlatBuffer = RenderFlatBuffer::GetRenderFlatBufferDoc(builder_out.GetBufferPointer());
//     for (int i = 0; i < renderFlatBuffer->Bom()->size(); i++)
//     {
//         auto instance = renderFlatBuffer->Bom()->Get(i);
//         auto currentId = instance->InstanceID()->str();
//         auto parentId = instance->ParentID()->str();

//         vsg::dvec3 center((nodePtr[currentId].bounds.max + nodePtr[currentId].bounds.min) / 2.0);
//         vsg::dvec3 parentCenter((nodePtr[parentId].bounds.max + nodePtr[parentId].bounds.min) / 2.0);

//         auto distance = center - parentCenter;
//         auto moveDistance = distance * 0.2;
//         //std::cout << "moveDistance" << moveDistance << std::endl;
//         nodePtr[currentId].transform->matrix = nodePtr[currentId].transform->matrix * vsg::translate(moveDistance);
//     }
// }

// void CADMesh::recover()
// {
//     auto renderFlatBuffer = RenderFlatBuffer::GetRenderFlatBufferDoc(builder_out.GetBufferPointer());
//     for (int i = 0; i < renderFlatBuffer->Bom()->size(); i++)
//     {
//         auto instance = renderFlatBuffer->Bom()->Get(i);
//         auto currentId = instance->InstanceID()->str();
//         auto parentId = instance->ParentID()->str();

//         nodePtr[currentId].transform->matrix = nodePtr[currentId].originalMatrix;
//     }
// }
