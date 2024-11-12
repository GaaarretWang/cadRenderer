#include "CADMesh.h"
#include <chrono>
#include <iomanip>
#include <vsg/all.h>
#include <communication/dataInterface.h>

int global_argc = 0; 
char** global_argv = nullptr;

template<typename T>
vsg::vec3 CADMesh::toVec3(const flatbuffers::Vector<T>* flat_vector, int begin)
{
    // return vsg::vec3(flat_vector->Get(begin) / 1000, flat_vector->Get(begin + 1) / 1000, flat_vector->Get(begin + 2) / 1000);
    return vsg::vec3(flat_vector->Get(begin), flat_vector->Get(begin + 1), flat_vector->Get(begin + 2));
}

template<typename T>
vsg::vec3 CADMesh::toNewVec3(std::vector<T>* flat_vector, int begin)
{
    // return vsg::vec3(flat_vector->Get(begin) / 1000, flat_vector->Get(begin + 1) / 1000, flat_vector->Get(begin + 2) / 1000);

    //return vsg::vec3(flat_vector[begin], flat_vector[begin + 1], flat_vector[begin + 2]);
    float a = flat_vector->at(begin);
    float b = flat_vector->at(begin + 1);
    float c = flat_vector->at(begin + 2);
    return vsg::vec3(a, b, c);
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
    scene->addChild(plane_transform); //*******************************************
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
    scene->addChild(Env_transform);
}

void CADMesh::drawLine(vsg::vec3& begin, vsg::vec3& end, vsg::ref_ptr<vsg::Group> scene)
{
    vsg::vec3 beginPoint = vsg::vec3(begin.x, begin.y, begin.z);
    if (positionToIndex.count(beginPoint) == 0) //if unique
    {
        positionToIndex[beginPoint] = static_cast<uint32_t>(positions.size());
        positions.push_back(beginPoint);
    }
    indices.push_back(positionToIndex[beginPoint]);

    vsg::vec3 endPoint = vsg::vec3(end.x, end.y, end.z);
    if (positionToIndex.count(endPoint) == 0) //if unique
    {
        positionToIndex[endPoint] = static_cast<uint32_t>(positions.size());
        positions.push_back(endPoint);
    }
    indices.push_back(positionToIndex[endPoint]);
}

void CADMesh::buildNewNode(bool fullNormal, vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::ShaderSet> shader)
{

    bool LoadByJson = true; 
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
    
	auto info = datainterface.getRenderInfo();
	auto pmi = datainterface.getPmiInfos(true);
	auto instances = datainterface.getInstances();

    vsg::CommandLine arguments(&global_argc, global_argv);

    auto options = vsg::Options::create();
    options->fileCache = vsg::getEnv("VSG_FILE_CACHE"); //2
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");
    options->sharedObjects = vsg::SharedObjects::create();


    for (int i = 0; i < pmi.size(); i++)
    {
        //std::cout << pmi[i].instanceMatrixList.size() << std::endl;
        if (pmi[i].type == "Diagonal" || pmi[i].type == "Horizontal") {

            vsg::vec3 temp1 = vsg::vec3(pmi[i].points[0][0], pmi[i].points[0][1], pmi[i].points[0][2]);
            vsg::vec3 temp2 = vsg::vec3(pmi[i].points[1][0], pmi[i].points[1][1], pmi[i].points[1][2]);
            vsg::vec3 temp3 = vsg::vec3(pmi[i].points[2][0], pmi[i].points[2][1], pmi[i].points[2][2]);
            vsg::vec3 temp4 = vsg::vec3(pmi[i].points[3][0], pmi[i].points[3][1], pmi[i].points[3][2]);
            vsg::vec3 temp5 = vsg::vec3((3 * pmi[i].points[1][0] + 2 * pmi[i].points[3][0]) / 5, (3 * pmi[i].points[1][1] + 2 * pmi[i].points[3][1]) / 5, (3 * pmi[i].points[1][2] + 2 * pmi[i].points[3][2]) / 5);
            vsg::vec3 temp6 = vsg::vec3((3 * pmi[i].points[3][0] + 2 * pmi[i].points[1][0]) / 5, (3 * pmi[i].points[3][1] + 2 * pmi[i].points[1][1]) / 5, (3 * pmi[i].points[3][2] + 2 * pmi[i].points[1][2]) / 5);
            vsg::vec3 textx = vsg::vec3((pmi[i].points[1][0] + pmi[i].points[3][0]) / 2, (pmi[i].points[1][1] + pmi[i].points[3][1]) / 2, (pmi[i].points[1][2] + pmi[i].points[3][2]) / 2);
            //vsg::vec3 texty = vsg::vec3((pmi[i].points[3][0] + pmi[i].points[1][0]) / 2, (pmi[i].points[3][1] + pmi[i].points[1][1]) / 2, 0);

            vsg::mat4 transforms;
            for (int m = 0; m < 4; m++)
                for (int n = 0; n < 4; n++)
                {
                    transforms[m][n] = pmi[i].instanceMatrixList[0].at(m * 4 + n);
                }
            //temp1 = transforms * temp1;
            //temp2 = transforms * temp2;
            //temp3 = transforms * temp3;
            //temp4 = transforms * temp4;
            //temp5 = transforms * temp5;
            //temp6 = transforms * temp6;
            //textx = transforms * textx;
            //temp1 = 
            auto font_filename = arguments.value(std::string("../data/fonts/times.vsgb"), "-f");
            auto enable_tests = arguments.read("--test");
            auto font = vsg::read_cast<vsg::Font>(font_filename, options);
            auto layout = vsg::StandardLayout::create();
            //layout->
            layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT; //水平居中
            layout->position = textx;                                            //左右，前后，上下（xyz轴）
            layout->horizontal = vsg::vec3(50.0, 0.0, 0.0);                      //水平方向上的偏移
            layout->vertical = vsg::vec3(0.0, 50.0, 0.0);                        //垂直方向上的偏移
            layout->color = vsg::vec4(1.0, 1.0, 1.0, 1.0);                       //红、绿、蓝和alpha通道
            layout->outlineWidth = 0.1;
            layout->billboard = true;

            auto text = vsg::Text::create();
            text->text = vsg::stringValue::create(pmi[i].value);
            text->font = font;
            text->layout = layout;
            text->setup(0, options);
            scene->addChild(text);
            drawLine(temp1, temp2, scene);
            drawLine(temp3, temp4, scene);
            drawLine(temp2, temp5, scene);
            drawLine(temp6, temp4, scene);
            builder->options = options;
            geomInfo.color.set(1.0f, 1.0f, 1.0f, 1.0f);
            geomInfo.cullNode = false;
            stateInfo.wireframe = true;

            scene->addChild(builder->createLine(geomInfo, stateInfo, positions, indices));
        }
        else if (pmi[i].type == "Radius")
        {
            vsg::vec3 temp1 = vsg::vec3(pmi[i].points[0][0], pmi[i].points[0][1], pmi[i].points[0][2]);
            vsg::vec3 temp2 = vsg::vec3(pmi[i].points[1][0], pmi[i].points[1][1], pmi[i].points[1][2]);
            vsg::vec3 temp3 = vsg::vec3(pmi[i].points[2][0], pmi[i].points[2][1], pmi[i].points[2][2]);
            vsg::vec3 temp5 = vsg::vec3((2 * pmi[i].points[0][0] + pmi[i].points[1][0]) / 3, (2 * pmi[i].points[0][1] + pmi[i].points[1][1]) / 3, (2 * pmi[i].points[0][2] + pmi[i].points[1][2]) / 3);
            vsg::vec3 temp6 = vsg::vec3((2 * pmi[i].points[1][0] + pmi[i].points[0][0]) / 3, (2 * pmi[i].points[1][1] + pmi[i].points[0][1]) / 3, (2 * pmi[i].points[1][2] + pmi[i].points[0][2]) / 3);

            vsg::mat4 transforms;
            for (int m = 0; m < 4; m++)
                for (int n = 0; n < 4; n++)
                {
                    transforms[m][n] = pmi[i].instanceMatrixList[0].at(m * 4 + n);
                }
            //temp1 = transforms * temp1;
            //temp2 = transforms * temp2;
            //temp3 = transforms * temp3;
            //temp4 = transforms * temp4;
            //temp5 = transforms * temp5;
            //temp6 = transforms * temp6;
            //textx = transforms * textx;
            //auto font_filename = arguments.value(std::string("D:/APP/1PostGraduate/vsgExamples1/data/fonts/times.vsgb"), "-f");
            auto font_filename = arguments.value(std::string("../data/fonts/times.vsgb"), "-f");
            auto enable_tests = arguments.read("--test");
            auto font = vsg::read_cast<vsg::Font>(font_filename, options);
            auto layout = vsg::StandardLayout::create();
            layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT; //水平居中
            layout->position = temp3;                                            //左右，前后，上下（xyz轴）
            layout->horizontal = vsg::vec3(50.0, 0.0, 0.0);                      //水平方向上的偏移
            layout->vertical = vsg::vec3(0.0, 50.0, 0.0);                        //垂直方向上的偏移
            layout->color = vsg::vec4(1.0, 1.0, 1.0, 1.0);                       //红、绿、蓝和alpha通道
            layout->outlineWidth = 0.1;
            layout->billboard = true;

            auto text = vsg::Text::create();
            text->text = vsg::stringValue::create(pmi[i].value);
            text->font = font;
            text->layout = layout;
            text->setup(0, options);
            scene->addChild(text);

            drawLine(temp1, temp5, scene);
            drawLine(temp6, temp2, scene);
            builder->options = options;
            geomInfo.color.set(1.0f, 1.0f, 1.0f, 1.0f);
            geomInfo.cullNode = false;
            stateInfo.wireframe = true;

            scene->addChild(builder->createLine(geomInfo, stateInfo, positions, indices));
        
        }
        else if (pmi[i].type == "Diameter")
        {
            vsg::vec3 temp1 = vsg::vec3(pmi[i].points[0][0], pmi[i].points[0][1], pmi[i].points[0][2]);
            vsg::vec3 temp2 = vsg::vec3(pmi[i].points[1][0], pmi[i].points[1][1], pmi[i].points[1][2]);
            vsg::vec3 temp3 = vsg::vec3(pmi[i].text[0][0], pmi[i].text[0][1], pmi[i].text[0][2]);
            vsg::vec3 temp5 = vsg::vec3((2 * pmi[i].points[0][0] + pmi[i].points[1][0]) / 3, (2 * pmi[i].points[0][1] + pmi[i].points[1][1]) / 3, (2 * pmi[i].points[0][2] + pmi[i].points[1][2]) / 3);
            vsg::vec3 temp6 = vsg::vec3((2 * pmi[i].points[1][0] + pmi[i].points[0][0]) / 3, (2 * pmi[i].points[1][1] + pmi[i].points[0][1]) / 3, (2 * pmi[i].points[1][2] + pmi[i].points[0][2]) / 3);

            vsg::mat4 transforms;
            for (int m = 0; m < 4; m++)
                for (int n = 0; n < 4; n++)
                {
                    transforms[m][n] = pmi[i].instanceMatrixList[0].at(m * 4 + n);
                }
            //temp1 = transforms * temp1;
            //temp2 = transforms * temp2;
            //temp3 = transforms * temp3;
            //temp4 = transforms * temp4;
            //temp5 = transforms * temp5;
            //temp6 = transforms * temp6;
            //textx = transforms * textx;
            auto font_filename = arguments.value(std::string("../data/fonts/times.vsgb"), "-f");
            auto enable_tests = arguments.read("--test");
            auto font = vsg::read_cast<vsg::Font>(font_filename, options);
            auto layout = vsg::StandardLayout::create();
            layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT; //水平居中
            layout->position = temp3;                                            //左右，前后，上下（xyz轴）
            layout->horizontal = vsg::vec3(50.0, 0.0, 0.0);                      //水平方向上的偏移
            layout->vertical = vsg::vec3(0.0, 50.0, 0.0);                        //垂直方向上的偏移
            layout->color = vsg::vec4(1.0, 1.0, 1.0, 1.0);                       //红、绿、蓝和alpha通道
            layout->outlineWidth = 0.1;
            layout->billboard = true;

            auto text = vsg::Text::create();
            text->text = vsg::stringValue::create(pmi[i].value);
            text->font = font;
            text->layout = layout;
            text->setup(0, options);
            scene->addChild(text);

            drawLine(temp1, temp2, scene);
            drawLine(temp2, temp3, scene);
            //drawLine(temp1, temp5, scene);
            //drawLine(temp6, temp2, scene);
            builder->options = options;
            geomInfo.color.set(1.0f, 1.0f, 1.0f, 1.0f);
            geomInfo.cullNode = false;
            stateInfo.wireframe = true;

            scene->addChild(builder->createLine(geomInfo, stateInfo, positions, indices));
        }
    }
    //drawLine(begin, end, scene);
    builder->options = options;
    geomInfo.color.set(1.0f, 1.0f, 1.0f, 1.0f);
    geomInfo.cullNode = false;
    stateInfo.wireframe = true;


    for (int i = 0; i < lines.size(); i++)
    {
        drawLine(lines[i].begin, lines[i].end, scene);
    }


    scene->addChild(builder->createLine(geomInfo, stateInfo, positions, indices));

    
    uint8_t* buffer_data;
    int buffer_size;
    auto data = info.data();
    for (int o = 0; o < info.size(); o++) {
        std::unordered_map<TinyModelVertex, uint32_t> uniqueVertices; //存储点信息，相同点只存一份
        std::vector<TinyModelVertex> mVertices{};                     //保存点在数组中位置信息
        std::vector<vsg::vec3> mVerticesPos{};                        //保存点在数组中位置信息
        std::vector<vsg::vec3> mVerticesNor{};                        //保存点在数组中位置信息
        std::vector<uint32_t> mIndices{};                             //索引，找点
        
        RenderInfo modelfbs = info[o];
        int num = modelfbs.matrixNum;
        auto matrix = modelfbs.matrix;
        auto type = modelfbs.type;
        auto modelGeo = modelfbs.geo;
        auto modelIndex = modelGeo->getIndex();
        auto position = modelGeo->getPosition();
        auto normal = modelGeo->getNormal();
        auto modelPar = modelfbs.params;
        auto metalness = modelPar->mMetalness;
        auto specular = modelPar->mSpecular;
        auto opacity = modelPar->getOpacity();
        auto color = modelPar->getColor();
        auto uv = modelGeo->getUV();

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
        default_material->value().metallicFactor = metalness;
        default_material->value().diffuseFactor.set(0.55, 0.55, 0.55, 1.0f);
        default_material->value().specularFactor.set(0.7, 0.7, 0.7, 1.0f);
        default_material->value().emissiveFactor.set(0.0, 0.0, 0.0, 0.0);
        default_material->value().roughnessFactor = 0.6f;
        //std::cout << default_color << std::endl;
        /*
        */
        if (type == "face")
        {
            for (int j = 0; j < num; j++)
            {
                vsg::mat4 transforms;
                for (int m = 0; m < 4; m++)
                    for (int n = 0; n < 4; n++)
                    {
                        transforms[m][n] = matrix[j * 16 + m * 4 + n];
                    }
                for (int i = 0; i < modelIndex.size(); i += 1)
                {
                    TinyModelVertex vertex;
                    int index = modelIndex.at(i);
                    vertex.pos = toNewVec3(&position, index * 3);
                    vertex.pos = transforms * vertex.pos;
                    vertex.normal = toNewVec3(&normal, index * 3);

                    if (uniqueVertices.count(vertex) == 0) //if unique 唯一
                    {                                      //push进数组。记录位置
                        uniqueVertices[vertex] = static_cast<uint32_t>(mVertices.size());
                        mVertices.push_back(vertex);
                    }
                    mIndices.push_back(uniqueVertices[vertex]); //根据新proto的数组，索引位置改变
                }
                auto now = std::chrono::system_clock::now();
                // 转换为time_t格式，方便用本地时间表示
                std::time_t now_c = std::chrono::system_clock::to_time_t(now);
                // 输出时间，精确到秒
                //std::cout << std::put_time(std::localtime(&now_c), "%Y-%m-%d %X") << std::endl;

            }

            int Nodenumber = mVertices.size();   //顶点、法向个数
            int Indicesnumber = mIndices.size(); //索引个数

            vsg::ref_ptr<vsg::vec3Array> vertices = vsg::vec3Array::create(Nodenumber); //分配数组空间
            vsg::ref_ptr<vsg::vec3Array> normals = vsg::vec3Array::create(Nodenumber);
            //coordinates = vsg::vec2Array::create(Nodenumber);
            vsg::ref_ptr<vsg::uintArray> indices = vsg::uintArray::create(Indicesnumber);

            //读取顶点，保存成vsg数组形式
            for (int i = 0; i < Nodenumber; i++)
            {
                vertices->at(i) = vsg::vec3(mVertices[i].pos);
                normals->at(i) = vsg::vec3(mVertices[i].normal);
                //vertices->at(i) = mVerticesPos[i];
                //normals->at(i) = mVerticesNor[i];
            }
            //读取索引
            for (int i = 0; i < Indicesnumber; i++)
            {
                indices->at(i) = mIndices[i];
            }
            // 创建独立的PipelineConfigurator和DrawCommand
            auto graphicsPipelineConfig = vsg::GraphicsPipelineConfigurator::create(shader);
            vsg::DataList vertexArrays;
            auto drawCommands = vsg::Commands::create();

            //传入模型几何参数
            graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX,vertices);
            graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Normal", VK_VERTEX_INPUT_RATE_VERTEX,normals);
            graphicsPipelineConfig->assignDescriptor("material", default_material);
            //绑定索引
            drawCommands->addChild(vsg::BindVertexBuffers::create(graphicsPipelineConfig->baseAttributeBinding, vertexArrays));
            drawCommands->addChild(vsg::BindIndexBuffer::create(indices));
            drawCommands->addChild(vsg::DrawIndexed::create(indices->size(), 1, 0, 0, 0));

            graphicsPipelineConfig->init();
            auto stateGroup = vsg::StateGroup::create();
            graphicsPipelineConfig->copyTo(stateGroup);
            stateGroup->addChild(drawCommands);
            scene->addChild(stateGroup);
        }
    }

    //verticesVector.push_back(vertices);
    //normalsVector.push_back(normals);
    //indicesVector.push_back(indices);

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

void CADMesh::transferModel(const std::string& path, bool fullNormal, vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::ShaderSet> shader, const vsg::dmat4& modelMatrix){
    uint8_t* buffer_data;
    int buffer_size;
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "Failed to open the file." << std::endl;
    }
    file.seekg(0, std::ios::end);
    std::streampos fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(fileSize);
    file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
    file.close();
    buffer_data = buffer.data();
    buffer_size = fileSize;

    builder_out.PushFlatBuffer(buffer_data, buffer_size);
    auto renderFlatBuffer = RenderFlatBuffer::GetRenderFlatBufferDoc(builder_out.GetBufferPointer());
    int emptyProtoNum = 0;

    for (int p = 0; p < renderFlatBuffer->ProtoData()->size(); p++)
    {
        //std::cout << "proto" << p << ":" << std::endl;
        const RenderFlatBuffer::Proto* protofbs = renderFlatBuffer->ProtoData()->Get(p);
        if (protofbs->Models()->size() == 0)
        {
            emptyProtoNum++;
            continue;
        }
        protoIndex[protofbs->ProtoID()->str()] = static_cast<uint32_t>(p - emptyProtoNum);
        //根据protoid找模型的索引，在所有有model的proto中的位置
        std::unordered_map<TinyModelVertex, uint32_t> uniqueVertices;//存储点信息，相同点只存一份
        std::vector<TinyModelVertex> mVertices{};//保存点在数组中位置信息
        std::vector<uint32_t> mIndices{};        //索引，找点

        for (int m = 0; m < protofbs->Models()->size(); m++)//每个零件遍历model
        {
            const RenderFlatBuffer::Model* modelfbs = protofbs->Models()->Get(m);
            auto modelGeo = modelfbs->Geo();
            //std::cout << "modelfbs->Type()->str()" << modelfbs->Type()->str() << std::endl;
            if (modelfbs->Type()->str() == "mesh")//三角面片
            {
                if (fullNormal) 
                {
                    for (int i = 0; i < modelGeo->Index()->size(); i += 1)
                    {
                        TinyModelVertex vertex;
                        int index = modelGeo->Index()->Get(i);
                        vertex.pos = toVec3(modelGeo->Position(), index * 3);
                        vertex.normal = toVec3(modelGeo->Normal(), index * 3);
                        // vertex.uv = toVec2(modelGeo->UV(), index * 2);

                        if (uniqueVertices.count(vertex) == 0) //if unique
                        {//push进数组。记录位置
                            uniqueVertices[vertex] = static_cast<uint32_t>(mVertices.size());
                            mVertices.push_back(vertex);
                        }
                        mIndices.push_back(uniqueVertices[vertex]);//根据新proto的数组，索引位置改变
                    }
                }
                else
                {
                    for (int i = 0; i < modelGeo->Index()->size(); i += 3)
                    {
                        std::vector<TinyModelVertex> vertex = {};
                        for (int j = 0; j < 3; j++)
                        {
                            TinyModelVertex v;
                            int index = modelGeo->Index()->Get(i + j);
                            // std::cout <<index << "\t";
                            v.pos = toVec3(modelGeo->Position(), index * 3);
                            // vertex.uv = toVec2(modelGeo->UV(), index * 2);
                            vertex.push_back(v);
                        }
                        vsg::vec3 n = vsg::normalize(vsg::cross((vertex[1].pos - vertex[0].pos), (vertex[2].pos - vertex[0].pos)));
                        vertex[0].normal = n;
                        vertex[1].normal = n;
                        vertex[2].normal = n;

                        for (int j = 0; j < 3; j++)
                        {
                            if (uniqueVertices.count(vertex[j]) == 0) //if unique
                            {
                                uniqueVertices[vertex[j]] = static_cast<uint32_t>(mVertices.size());
                                mVertices.push_back(vertex[j]);
                            }
                            mIndices.push_back(uniqueVertices[vertex[j]]);
                        }
                    }
                }
            }
        }
        int Nodenumber = mVertices.size();    //顶点、法向个数
        int Indicesnumber = mIndices.size();  //索引个数
        protoTriangleNum[protofbs->ProtoID()->str()] = static_cast<uint32_t>(mIndices.size() / 3);

        vsg::ref_ptr<vsg::vec3Array> vertices = vsg::vec3Array::create(Nodenumber);//分配数组空间
        vsg::ref_ptr<vsg::vec3Array> normals = vsg::vec3Array::create(Nodenumber);
        //coordinates = vsg::vec2Array::create(Nodenumber);
        vsg::ref_ptr<vsg::uintArray> indices = vsg::uintArray::create(Indicesnumber);

        //读取顶点，保存成vsg数组形式
        for (int i = 0; i < Nodenumber; i++)
        {
            vertices->at(i) = vsg::vec3(mVertices[i].pos);
            normals->at(i) = vsg::vec3(mVertices[i].normal);
        }
        //读取索引
        for (int i = 0; i < Indicesnumber; i++)
        {
            indices->at(i) = mIndices[i];
        }
        verticesVector.push_back(vertices);
        normalsVector.push_back(normals);
        indicesVector.push_back(indices);
    }
}

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

//     //创建树的所有节点
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

//             //创建纹理或遮罩
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

//             //传入模型几何参数
//             graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, verticesVector[protoIndex[instance->ProtoID()->str()]]);
//             graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Normal", VK_VERTEX_INPUT_RATE_VERTEX, normalsVector[protoIndex[instance->ProtoID()->str()]]);
//             //graphicsPipelineConfig->assignArray(vertexArrays, "vsg_TexCoord0", VK_VERTEX_INPUT_RATE_VERTEX, texcoords);

//             graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, default_color);
//             graphicsPipelineConfig->assignDescriptor("material", default_material);
//             //绑定索引
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
//     scene->addChild(nodePtr[""].transform);
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
//     scene->addChild(nodePtr[""].transform);
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

void CADMesh::buildObjNode(const char* model_path, const char* material_path,vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::ShaderSet> shader, const vsg::dmat4& modelMatrix)
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

    std::unordered_map num = objLoader.vertex_count(model_path);

    auto vertices = vsg::vec3Array::create(num["vertices"]); 
    auto normals = vsg::vec3Array::create(num["normals"]);
    auto materials = vsg::PhongMaterialArray::create();

    std::cout <<"success creating obj"<<std::endl;

    vsg::ref_ptr<vsg::uintArray> indices = vsg::uintArray::create(num["indices"]);
    vsg::ref_ptr<vsg::vec2Array> verticesUV = vsg::vec2Array::create(num["uvs"]);
    vsg::ref_ptr<vsg::vec3Array> colors;
    objLoader.load_obj(model_path, material_path, vertices, normals, verticesUV, colors, materials, indices);
    std::cout <<"success Loading obj"<<std::endl;
    verticesVector.push_back(vertices);
    normalsVector.push_back(normals);
    indicesVector.push_back(indices);
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
