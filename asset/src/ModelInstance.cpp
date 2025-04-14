#include "ModelInstance.h"

template <typename T>
using ptr = vsg::ref_ptr<T>;
vsg::ImageInfoList createImageInfo(vsg::ref_ptr<vsg::Data> in_data, VkFormat type, int width, int height, vsg::ref_ptr<vsg::Context> context){
    auto sampler = vsg::Sampler::create();
    sampler->magFilter = VK_FILTER_NEAREST;
    sampler->minFilter = VK_FILTER_NEAREST;

    vsg::ref_ptr<vsg::ImageInfo> imageInfosIBL = vsg::ImageInfo::create(sampler, in_data);
    vsg::ImageInfoList imageInfosListIBL = {imageInfosIBL};
    return imageInfosListIBL;
}

// VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_D32_SFLOAT

void ModelInstance::buildInstance(CADMesh* mesh, vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::ShaderSet> shader, const vsg::dmat4& modelMatrix){
    vsg::ref_ptr<vsg::vec4Value> default_color = vsg::vec4Value::create(vsg::vec4{0.9, 0.9, 0.9, 1.0});
    vsg::ref_ptr<vsg::vec2Array> dummyUV = vsg::vec2Array::create(1);
    vsg::ref_ptr<vsg::PhongMaterialValue> default_material;
    default_color = vsg::vec4Value::create(vsg::vec4{0.9, 0.9, 0.9, 1.0});
    default_material = vsg::PhongMaterialValue::create();
    default_material->value().ambient.set(0.9, 0.9, 0.9, 1.0);
    default_material->value().diffuse.set(0.55, 0.55, 0.55, 1.0);
    default_material->value().specular.set(0.7, 0.7, 0.7, 1.0);
    //std::cout << mat->value().shininess;//Ĭ��ֵΪ100
    default_material->value().shininess = 25;

    auto renderFlatBuffer = RenderFlatBuffer::GetRenderFlatBufferDoc(mesh->builder_out.GetBufferPointer());
    for (int i = 0; i < renderFlatBuffer->Bom()->size(); i++)
    {
        auto instance = renderFlatBuffer->Bom()->Get(i);
        instanceIndex[instance->InstanceID()->str()] = i;
    }

    treeNode top;
    top.transform = vsg::MatrixTransform::create();
    top.transform->matrix = modelMatrix;
    top.originalMatrix = modelMatrix;
    nodePtr[""] = top;

    //�����������нڵ�
    for (int i = 0; i < renderFlatBuffer->Bom()->size(); i++)
    {

        auto instance = renderFlatBuffer->Bom()->Get(i);
        auto protoId = instance->ProtoID()->str();
        auto currentId = instance->InstanceID()->str();
        if (mesh->protoIndex.find(protoId) != mesh->protoIndex.end())
        {
            auto graphicsPipelineConfig = vsg::GraphicsPipelineConfigurator::create(shader);
            vsg::DataList vertexArrays;
            auto drawCommands = vsg::Commands::create();

            //��������������
            bool addTexture = 0;
            auto options = vsg::Options::create();
            vsg::Path textureFile("../asset/data/textures/lz.vsgb");
            if (textureFile && addTexture)
            {
                auto textureData = vsg::read_cast<vsg::Data>(textureFile, options);
                if (!textureData)
                {
                    std::cout << "Could not read texture file : " << textureFile << std::endl;
                }
                graphicsPipelineConfig->assignTexture("diffuseMap", textureData);
            }

            //����ģ�ͼ��β���
            graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, mesh->verticesVector[mesh->protoIndex[instance->ProtoID()->str()]]);
            graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Normal", VK_VERTEX_INPUT_RATE_VERTEX, mesh->normalsVector[mesh->protoIndex[instance->ProtoID()->str()]]);
            //graphicsPipelineConfig->assignArray(vertexArrays, "vsg_TexCoord0", VK_VERTEX_INPUT_RATE_VERTEX, texcoords);

            graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, default_color);
            graphicsPipelineConfig->assignDescriptor("material", default_material);
            //������
            drawCommands->addChild(vsg::BindVertexBuffers::create(graphicsPipelineConfig->baseAttributeBinding, vertexArrays));
            drawCommands->addChild(vsg::BindIndexBuffer::create(mesh->indicesVector[mesh->protoIndex[instance->ProtoID()->str()]]));
            drawCommands->addChild(vsg::DrawIndexed::create(mesh->indicesVector[mesh->protoIndex[instance->ProtoID()->str()]]->size(), 1, 0, 0, 0));

            graphicsPipelineConfig->init();
            auto stateGroup = vsg::StateGroup::create();
            graphicsPipelineConfig->copyTo(stateGroup);
            stateGroup->addChild(drawCommands);

            treeNode node;
            node.transform = vsg::MatrixTransform::create();
            for (int m = 0; m < 4; m++)
                for (int n = 0; n < 3; n++)
                {
                    node.transform->matrix[m][n] = instance->Matrix()->Get(3 * m + n);
                    node.originalMatrix[m][n] = node.transform->matrix[m][n];
                }
            node.transform->addChild(stateGroup);

            nodePtr[currentId] = node;
        }
        else
        {
            vsg::mat4 matrix = vsg::mat4(1.0f);
            for (int m = 0; m < 4; m++)
                for (int n = 0; n < 3; n++)
                {
                    matrix[m][n] = instance->Matrix()->Get(3 * m + n);
                }
            treeNode node;
            node.transform = vsg::MatrixTransform::create();
            node.transform->matrix = matrix;
            node.originalMatrix = matrix;
            nodePtr[currentId] = node;
        }
    }

    //�������нڵ�Ĺ�ϵ
    uint32_t totalTriangleNum = 0;
    std::unordered_set<std::string> added;                                              //��¼���ӵ����еĽڵ�
    for (int i = 0; i < renderFlatBuffer->Bom()->size(); i++)
    {
        auto instance = renderFlatBuffer->Bom()->Get(i);
        auto currentId = instance->InstanceID()->str();
        auto parentId = instance->ParentID()->str();
        auto protoId = instance->ProtoID()->str();
        if (mesh->protoIndex.find(protoId) != mesh->protoIndex.end())
        {
            totalTriangleNum += mesh->protoTriangleNum[protoId];
            while (true)
            {
                if (added.find(currentId) == added.end())
                {
                    nodePtr[parentId].kids.push_back(currentId);
                    nodePtr[parentId].transform->addChild(nodePtr[currentId].transform);
                    added.insert(currentId);
                }
                if (parentId == "")
                    break;
                currentId = parentId;
                parentId = renderFlatBuffer->Bom()->Get(instanceIndex[currentId])->ParentID()->str();
            }
        }
    }

    std::cout << "totalTriangleNum = " << totalTriangleNum << std::endl;
    scene->addChild(nodePtr[""].transform);
}

void ModelInstance::buildObjInstance(CADMesh* mesh, vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::ShaderSet> shader, const vsg::dmat4& modelMatrix){
    vsg::ref_ptr<vsg::vec4Value> default_color;
    vsg::ref_ptr<vsg::PhongMaterialValue> default_material;
    default_color = vsg::vec4Value::create(vsg::vec4{0.9, 0.9, 0.9, 1.0});
    default_material = vsg::PhongMaterialValue::create();
    default_material->value().ambient.set(0.9, 0.9, 0.9, 1.0);
    default_material->value().diffuse.set(0.55, 0.55, 0.55, 1.0);
    default_material->value().specular.set(0.7, 0.7, 0.7, 1.0);
    //std::cout << mat->value().shininess;//Ĭ��ֵΪ100
    default_material->value().shininess = 25;

    vsg::ref_ptr<vsg::vec4Value> highlighted_color;
    vsg::ref_ptr<vsg::PhongMaterialValue> highlighted_material;
    highlighted_color = vsg::vec4Value::create(vsg::vec4{1.0, 0.0, 0.0, 1.0});
    highlighted_material = vsg::PhongMaterialValue::create();
    highlighted_material->value().ambient.set(1.0, 0.0, 0.0, 1.0);
    highlighted_material->value().diffuse.set(1.0, 0.0, 0.0, 1.0);
    highlighted_material->value().specular.set(1.0, 0.0, 0.0, 1.0);
    //std::cout << mat->value().shininess;//Ĭ��ֵΪ100
    highlighted_material->value().shininess = 25;

    auto graphicsPipelineConfig = vsg::GraphicsPipelineConfigurator::create(shader);

    // Create the graphics pipeline configurator
    vsg::DataList OBJ_vertexArrays;
    // Assign the vertex, normal, and texcoord arrays to the graphics pipeline configurator
    graphicsPipelineConfig->assignArray(OBJ_vertexArrays,"vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, mesh->verticesVector[0]);
    graphicsPipelineConfig->assignArray(OBJ_vertexArrays,"vsg_Normal", VK_VERTEX_INPUT_RATE_VERTEX, mesh->normalsVector[0]);
    graphicsPipelineConfig->assignArray(OBJ_vertexArrays,"vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, default_color);
    // graphicsPipelineConfig->assignArray(OBJ_vertexArrays,"vsg_TexCoord0", VK_VERTEX_INPUT_RATE_VERTEX, verticesUV);

    graphicsPipelineConfig->assignDescriptor("material", default_material);
    //绑定索引
    auto drawCommands = vsg::Commands::create();
    drawCommands->addChild(vsg::BindVertexBuffers::create(graphicsPipelineConfig->baseAttributeBinding, OBJ_vertexArrays));
    drawCommands->addChild(vsg::BindIndexBuffer::create(mesh->indicesVector[0]));
    drawCommands->addChild(vsg::DrawIndexed::create(mesh->indicesVector[0]->size(), 1, 0, 0, 0));

    graphicsPipelineConfig->init();

    auto stateGroup = vsg::StateGroup::create();
    graphicsPipelineConfig->copyTo(stateGroup);
    

    treeNode top;
    top.transform = vsg::MatrixTransform::create();
    stateGroup->addChild(drawCommands);
    top.transform->addChild(stateGroup);
    top.transform->matrix = modelMatrix;
    top.originalMatrix = modelMatrix;
    nodePtr[""] = top;

    scene->addChild(top.transform);
}

void ModelInstance::buildInstanceIBL(CADMesh* mesh, vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> gpc_ibl, vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> gpc_shadow, const vsg::dmat4& modelMatrix){

    vsg::ref_ptr<vsg::vec4Value> default_color = vsg::vec4Value::create(vsg::vec4{1.0, 1.0, 1.0, 1.0});
    vsg::ref_ptr<vsg::vec2Array> dummyUV = vsg::vec2Array::create(1);
    vsg::ref_ptr<vsg::PhongMaterialValue> default_material;
    //default_color = vsg::vec4Value::create(vsg::vec4{0.9, 0.9, 0.9, 1.0});
    default_material = vsg::PhongMaterialValue::create();
    default_material->value().ambient.set(0.9, 0.9, 0.9, 1.0);
    default_material->value().diffuse.set(0.55, 0.55, 0.55, 1.0);
    default_material->value().specular.set(0.7, 0.7, 0.7, 1.0);
    //std::cout << mat->value().shininess;//Ĭ��ֵΪ100
    default_material->value().shininess = 25;

    auto object_mat = vsg::PbrMaterialValue::create();
    object_mat->value().roughnessFactor = 0.1f;
    object_mat->value().metallicFactor = 0.9f;
    object_mat->value().baseColorFactor = vsg::vec4(1.0, 1.0, 1.0, 1.0);

    auto renderFlatBuffer = RenderFlatBuffer::GetRenderFlatBufferDoc(mesh->builder_out.GetBufferPointer());
    for (int i = 0; i < renderFlatBuffer->Bom()->size(); i++)
    {
        auto instance = renderFlatBuffer->Bom()->Get(i);
        instanceIndex[instance->InstanceID()->str()] = i;
    }

    treeNode top;
    top.transform = vsg::MatrixTransform::create();
    top.transform->matrix = modelMatrix;
    top.originalMatrix = modelMatrix;
    nodePtr[""] = top;

    //�����������нڵ�
    for (int i = 0; i < renderFlatBuffer->Bom()->size(); i++)
    {

        auto instance = renderFlatBuffer->Bom()->Get(i);
        auto protoId = instance->ProtoID()->str();
        auto currentId = instance->InstanceID()->str();
        if (mesh->protoIndex.find(protoId) != mesh->protoIndex.end())
        {

            vsg::DataList vertexArrays = {
                mesh->verticesVector[mesh->protoIndex[instance->ProtoID()->str()]],
                mesh->normalsVector[mesh->protoIndex[instance->ProtoID()->str()]],
                dummyUV,
                default_color
            };
            auto drawCommands = vsg::Commands::create();

            //������
            drawCommands->addChild(vsg::BindVertexBuffers::create(gpc_ibl->baseAttributeBinding, vertexArrays));
            drawCommands->addChild(vsg::BindIndexBuffer::create(mesh->indicesVector[mesh->protoIndex[instance->ProtoID()->str()]]));
            drawCommands->addChild(vsg::DrawIndexed::create(mesh->indicesVector[mesh->protoIndex[instance->ProtoID()->str()]]->size(), 1, 0, 0, 0));

            auto PbrStateGroup = vsg::StateGroup::create();
            PbrStateGroup->addChild(drawCommands);

            auto gpc_object = vsg::GraphicsPipelineConfigurator::create(*gpc_ibl);
            //auto gpc_object = gpc_ibl;
            gpc_object->assignDescriptor("material", object_mat);
            gpc_object->init();
            gpc_object->copyTo(PbrStateGroup);
            auto cadMeshSwitch = vsg::Switch::create();
            cadMeshSwitch->addChild(MASK_PBR_FULL, PbrStateGroup);

            treeNode node;
            node.transform = vsg::MatrixTransform::create();
            for (int m = 0; m < 4; m++)
                for (int n = 0; n < 3; n++)
                {
                    node.transform->matrix[m][n] = instance->Matrix()->Get(3 * m + n);
                    node.originalMatrix[m][n] = node.transform->matrix[m][n];
                }
            node.transform->addChild(cadMeshSwitch);

            nodePtr[currentId] = node;
        }
        else
        {
            vsg::mat4 matrix = vsg::mat4(1.0f);
            for (int m = 0; m < 4; m++)
                for (int n = 0; n < 3; n++)
                {
                    matrix[m][n] = instance->Matrix()->Get(3 * m + n);
                }
            treeNode node;
            node.transform = vsg::MatrixTransform::create();
            node.transform->matrix = matrix;
            node.originalMatrix = matrix;
            nodePtr[currentId] = node;
        }
    }

    //�������нڵ�Ĺ�ϵ
    uint32_t totalTriangleNum = 0;
    std::unordered_set<std::string> added;                                              //��¼���ӵ����еĽڵ�
    for (int i = 0; i < renderFlatBuffer->Bom()->size(); i++)
    {
        auto instance = renderFlatBuffer->Bom()->Get(i);
        auto currentId = instance->InstanceID()->str();
        auto parentId = instance->ParentID()->str();
        auto protoId = instance->ProtoID()->str();
        if (mesh->protoIndex.find(protoId) != mesh->protoIndex.end())
        {
            totalTriangleNum += mesh->protoTriangleNum[protoId];
            while (true)
            {
                if (added.find(currentId) == added.end())
                {
                    nodePtr[parentId].kids.push_back(currentId);
                    nodePtr[parentId].transform->addChild(nodePtr[currentId].transform);
                    added.insert(currentId);
                }
                if (parentId == "")
                    break;
                currentId = parentId;
                parentId = renderFlatBuffer->Bom()->Get(instanceIndex[currentId])->ParentID()->str();
            }
        }
    }

    std::cout << "totalTriangleNum = " << totalTriangleNum << std::endl;
    scene->addChild(nodePtr[""].transform);
}

void ModelInstance::buildObjInstanceShadow(CADMesh* mesh, vsg::ref_ptr<vsg::Group> scene, 
    vsg::ref_ptr<vsg::ShaderSet> pbr_shaderset, const vsg::dmat4& modelMatrix, vsg::ImageInfoList camera_info, vsg::ImageInfoList depth_info, vsg::ref_ptr<vsg::floatArray> params){
    for(int i = 0; i < mesh->objIndicesVector.size(); i++){        
        auto graphicsPipelineConfig = vsg::GraphicsPipelineConfigurator::create(pbr_shaderset);
        graphicsPipelineConfig->assignTexture("cameraImage", camera_info);
        graphicsPipelineConfig->assignTexture("depthImage", depth_info);
        graphicsPipelineConfig->assignUniform("params", params);//是否半透明判断
        auto mat = vsg::PbrMaterialValue::create();
        graphicsPipelineConfig->assignDescriptor("material", mat);
        auto colors = vsg::vec4Value::create(vsg::vec4{1.0f, 1.0f, 1.0f, 1.0f});
        vsg::DataList vertexArrays;
        graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, mesh->objVerticesVector[i]);
        graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Normal", VK_VERTEX_INPUT_RATE_VERTEX, mesh->objNormalsVector[i]);
        graphicsPipelineConfig->assignArray(vertexArrays, "vsg_TexCoord0", VK_VERTEX_INPUT_RATE_VERTEX, mesh->objUVVector[i]);
        graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, colors);
        auto drawCommands = vsg::Commands::create();
        drawCommands->addChild(vsg::BindVertexBuffers::create(graphicsPipelineConfig->baseAttributeBinding, vertexArrays));
        drawCommands->addChild(vsg::BindIndexBuffer::create(mesh->objIndicesVector[i]));
        drawCommands->addChild(vsg::DrawIndexed::create(mesh->objIndicesVector[i]->size(), 1, 0, 0, 0));
        graphicsPipelineConfig->init();
        auto stateGroup = vsg::StateGroup::create();
        graphicsPipelineConfig->copyTo(stateGroup);
        auto transform = vsg::MatrixTransform::create(modelMatrix);
        stateGroup->addChild(drawCommands);
        transform->addChild(stateGroup);
        scene->addChild(transform);
    }

    // countNum++;
    // std::cout<<countNum<<std::endl;
    // vsg::ref_ptr<vsg::vec4Value> default_color = vsg::vec4Value::create(vsg::vec4{1.0, 1.0, 1.0, 1.0});
    
    // std::cout<< "objUVVector count :" << mesh->objUVVector[0]->size() << std::endl;
    // std::cout<< "objNormalVector count :" << mesh->objNormalsVector[0]->size() << std::endl;
    // std::cout<< "objVerticesVector count :" << mesh->objVerticesVector[0]->size() << std::endl;
    // std::cout<< "objIndicesVector count :" << mesh->objIndicesVector[0]->size() << std::endl;
    // auto objmaterial = mesh->objMaterialVector[0];
    // auto object_mat = vsg::PbrMaterialValue::create();
    // object_mat->value().roughnessFactor = 0.1f;
    // object_mat->value().metallicFactor = 0.9f;
    // object_mat->value().baseColorFactor = vsg::vec4(1.0f, 1.0f, 0.0f, 1.0f);
    // // object_mat->value() = objmaterial->at(0);

    // std::cout << mesh->objVerticesVector[0]->size() << "**********************************" << std::endl;
    // // Create the graphics pipeline configurator
    // auto options = vsg::Options::create();
    // options->add(vsgXchange::all::create());
    // vsg::Builder builder;
    // vsg::GeometryInfo geomInfo;
    // geomInfo.dx.set(500.0f, 0.0f, 0.0f);
    // geomInfo.dy.set(0.0f, 500.0f, 0.0f);
    // geomInfo.dz.set(0.0f, 0.0f, 500.0f);
    // geomInfo.position.z-=1000;
    // geomInfo.color = vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f};
    // vsg::StateInfo stateinfo;

    // auto sampler = vsg::Sampler::create();
    // sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    // sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    // sampler->minFilter = VK_FILTER_LINEAR; // 线性过滤（平滑纹理）
    // sampler->magFilter = VK_FILTER_LINEAR;


    // std::vector<vsg::ref_ptr<vsg::GraphicsPipelineConfigurator>> gpc_mtrgroup;
    // auto cadMeshSwitch = vsg::Switch::create();
    // treeNode top;
    // top.transform = vsg::MatrixTransform::create();

    // for(int i = 0; i < mesh->objIndicesVector.size(); i++){        
    //     //绑定索引
    //     // vsg::BindVertexBuffers::create(gpc_high_group[i]->baseAttributeBinding, OBJ_vertexArrays);
    //     vsg::DataList OBJ_vertexArrays = {
    //         mesh->objVerticesVector[i],
    //         mesh->objNormalsVector[i],
    //         mesh->objUVVector[i],
    //         default_color
    //     };
    //     auto drawCommands = vsg::Commands::create();
    //     drawCommands->addChild(vsg::BindVertexBuffers::create(gpc_shadow->baseAttributeBinding, OBJ_vertexArrays));
    //     drawCommands->addChild(vsg::BindIndexBuffer::create(mesh->objIndicesVector[i]));//******************* */
    //     drawCommands->addChild(vsg::DrawIndexed::create(mesh->objIndicesVector[i]->size(), 1, 0, 0, 0));//******************* */

    //     auto cadMeshShadowStateGroup = vsg::StateGroup::create();
    //     cadMeshShadowStateGroup->addChild(drawCommands);
    //     gpc_shadow->copyTo(cadMeshShadowStateGroup);

    //     cadMeshSwitch->addChild(MASK_SHADOW_RECEIVER, cadMeshShadowStateGroup);
        
    // }

    // top.transform->addChild(cadMeshSwitch);
    // top.transform->matrix = modelMatrix;
    // top.originalMatrix = modelMatrix;
    // nodePtr[""] = top;

    // scene->addChild(top.transform);
}

void ModelInstance::buildObjInstanceIBL(CADMesh* mesh, vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::ShaderSet> model_shaderset, 
        const vsg::dmat4& modelMatrix, vsg::ImageInfoList camera_info, vsg::ImageInfoList depth_info, vsg::ref_ptr<vsg::floatArray> params){
    auto options = vsg::Options::create();
    options->add(vsgXchange::all::create());
    auto sampler = vsg::Sampler::create();
    sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler->minFilter = VK_FILTER_LINEAR; // 线性过滤（平滑纹理）
    sampler->magFilter = VK_FILTER_LINEAR;

    for(int i = 0; i < mesh->objIndicesVector.size(); i++){        
        auto graphicsPipelineConfig = vsg::GraphicsPipelineConfigurator::create(model_shaderset);
        if(i < mesh->objMaterialIndice[0].size() && mesh->objTexturePath.size() > mesh->objMaterialIndice[0][i]){
            vsg::ref_ptr<vsg::Data> textureData = vsg::read_cast<vsg::Data>("../asset/data/obj/helicopter-engine/tex/" + mesh->objTexturePath[mesh->objMaterialIndice[0][i]][0], options);
            // vsg::ref_ptr<vsg::Data> textureData = vsg::read_cast<vsg::Data>("../asset/data/obj/Medieval_building/textures/" + mesh->objTexturePath[0][mesh->objMaterialIndice[0][i]], options);
            graphicsPipelineConfig->assignTexture("diffuseMap", textureData);
            // std::cout << "../asset/data/obj/helicopter-engine/tex/" << mesh->objTexturePath[mesh->objMaterialIndice[0][i]][1] << std::endl;
            vsg::ref_ptr<vsg::Data> normalData = vsg::read_cast<vsg::Data>("../asset/data/obj/helicopter-engine/tex/" + mesh->objTexturePath[mesh->objMaterialIndice[0][i]][1], options);
            graphicsPipelineConfig->assignTexture("normalMap", normalData, sampler);
            vsg::ref_ptr<vsg::Data> metallicData = vsg::read_cast<vsg::Data>("../asset/data/obj/helicopter-engine/tex/" + mesh->objTexturePath[mesh->objMaterialIndice[0][i]][2], options);
            vsg::ref_ptr<vsg::Data> roughnessData = vsg::read_cast<vsg::Data>("../asset/data/obj/helicopter-engine/tex/" + mesh->objTexturePath[mesh->objMaterialIndice[0][i]][3], options);
            vsg::ref_ptr<vsg::Data> mrData = vsg::ushortArray2D::create(metallicData->width(), metallicData->height(), vsg::Data::Properties{VK_FORMAT_R8G8_UNORM});
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
            graphicsPipelineConfig->assignTexture("mrMap", mrData, sampler);
            graphicsPipelineConfig->assignTexture("cameraImage", camera_info);
            graphicsPipelineConfig->assignTexture("depthImage", depth_info);
            graphicsPipelineConfig->assignUniform("customParams", params);//是否半透明判断
        }
        auto mat = vsg::PbrMaterialValue::create();
        graphicsPipelineConfig->assignDescriptor("material", mat);
        auto colors = vsg::vec4Value::create(vsg::vec4{1.0f, 1.0f, 1.0f, 1.0f});
        vsg::DataList vertexArrays;
        graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, mesh->objVerticesVector[i]);
        graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Normal", VK_VERTEX_INPUT_RATE_VERTEX, mesh->objNormalsVector[i]);
        graphicsPipelineConfig->assignArray(vertexArrays, "vsg_TexCoord0", VK_VERTEX_INPUT_RATE_VERTEX, mesh->objUVVector[i]);
        graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, colors);
        auto drawCommands = vsg::Commands::create();
        drawCommands->addChild(vsg::BindVertexBuffers::create(graphicsPipelineConfig->baseAttributeBinding, vertexArrays));
        drawCommands->addChild(vsg::BindIndexBuffer::create(mesh->objIndicesVector[i]));
        drawCommands->addChild(vsg::DrawIndexed::create(mesh->objIndicesVector[i]->size(), 1, 0, 0, 0));
        graphicsPipelineConfig->init();
        auto stateGroup = vsg::StateGroup::create();
        graphicsPipelineConfig->copyTo(stateGroup);
        auto transform = vsg::MatrixTransform::create(modelMatrix);
        stateGroup->addChild(drawCommands);
        transform->addChild(stateGroup);
        scene->addChild(transform);
    }

    // for(int i = 0; i < mesh->objIndicesVector.size(); i++){
    //     if(i < mesh->objMaterialIndice[0].size() && mesh->objTexturePath.size() > mesh->objMaterialIndice[0][i]){
    //         std::cout << "../asset/data/obj/helicopter-engine/tex/" << mesh->objTexturePath[mesh->objMaterialIndice[0][i]][0] << std::endl;
    //         vsg::ref_ptr<vsg::Data> textureData = vsg::read_cast<vsg::Data>("../asset/data/obj/helicopter-engine/tex/" + mesh->objTexturePath[mesh->objMaterialIndice[0][i]][0], options);
    //         // vsg::ref_ptr<vsg::Data> textureData = vsg::read_cast<vsg::Data>("../asset/data/obj/Medieval_building/textures/" + mesh->objTexturePath[0][mesh->objMaterialIndice[0][i]], options);
    //         gpc_group[i]->assignTexture("diffuseMap", textureData, sampler);
    //         // std::cout << "../asset/data/obj/helicopter-engine/tex/" << mesh->objTexturePath[mesh->objMaterialIndice[0][i]][1] << std::endl;
    //         vsg::ref_ptr<vsg::Data> normalData = vsg::read_cast<vsg::Data>("../asset/data/obj/helicopter-engine/tex/" + mesh->objTexturePath[mesh->objMaterialIndice[0][i]][1], options);
    //         gpc_group[i]->assignTexture("normalMap", normalData, sampler);
    //     //     vsg::ref_ptr<vsg::Data> metallicData = vsg::read_cast<vsg::Data>("../asset/data/obj/helicopter-engine/tex/" + mesh->objTexturePath[mesh->objMaterialIndice[0][i]][2], options);
    //     //     vsg::ref_ptr<vsg::Data> roughnessData = vsg::read_cast<vsg::Data>("../asset/data/obj/helicopter-engine/tex/" + mesh->objTexturePath[mesh->objMaterialIndice[0][i]][3], options);
    //     //     vsg::ref_ptr<vsg::Data> mrData = vsg::ushortArray2D::create(metallicData->width(), metallicData->height(), vsg::Data::Properties{VK_FORMAT_R8G8_UNORM});
    //     //     auto* metallicPtr = static_cast<const uint8_t*>(metallicData->dataPointer());
    //     //     auto* roughnessPtr = static_cast<const uint8_t*>(roughnessData->dataPointer());
    //     //     auto* mrPtr = static_cast<uint8_t*>(mrData->dataPointer());

    //     //     for (size_t i = 0; i < metallicData->dataSize()/4; ++i) {
    //     //         // 假设RGBA顺序，取R通道（每4字节中的第0字节）
    //     //         uint8_t metallic = metallicPtr[i * 4];      // R通道
    //     //         uint8_t roughness = roughnessPtr[i * 4];     // R通道
    //     //         mrPtr[i*2] = metallic; // 组合为双通道
    //     //         mrPtr[i*2+1] = roughness; // 组合为双通道
    //     //     }
    //     //     gpc_group[i]->assignTexture("mrMap", mrData, sampler);
    //     //     gpc_group[i]->assignTexture("cameraImage", cameraImageInfo);
    //     //     gpc_group[i]->assignTexture("depthImage", depthImageInfo);
    //     //     auto params = vsg::floatArray::create(3);
    //     //     params->set(0, 1.f);
    //     //     params->set(1, 640.f * 2);
    //     //     params->set(2, 480.f * 2);
    //     //     gpc_group[i]->assignUniform("customParams", params);//是否半透明判断
    //     }

    //     //绑定索引
    //     // vsg::BindVertexBuffers::create(gpc_high_group[i]->baseAttributeBinding, OBJ_vertexArrays);
    //     auto drawCommands = vsg::Commands::create();
    //     drawCommands->addChild(vsg::BindVertexBuffers::create(gpc_group[i]->baseAttributeBinding, OBJ_vertexArrays));
    //     drawCommands->addChild(vsg::BindIndexBuffer::create(mesh->objIndicesVector[i]));//******************* */
    //     drawCommands->addChild(vsg::DrawIndexed::create(mesh->objIndicesVector[i]->size(), 1, 0, 0, 0));//******************* */

    //     // auto blendState = vsg::ColorBlendState::create();
    //     // blendState->attachments = vsg::ColorBlendState::ColorBlendAttachments{
    //     //     {true,                                        // blendEnable
    //     //      VK_BLEND_FACTOR_SRC_ALPHA,                   // srcColorBlendFactor
    //     //      VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,         // dstColorBlendFactor
    //     //      VK_BLEND_OP_ADD,                             // colorBlendOp
    //     //      VK_BLEND_FACTOR_ZERO,                         // srcAlphaBlendFactor
    //     //      VK_BLEND_FACTOR_ONE,                        // dstAlphaBlendFactor
    //     //      VK_BLEND_OP_ADD,                             // alphaBlendOp
    //     //      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
    //     //      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT} // colorWriteMask
    //     // };
    //     // auto depthState = vsg::DepthStencilState::create();
    //     // depthState->depthWriteEnable = VK_FALSE;

    //     auto PbrStateGroup = vsg::StateGroup::create();
    //     pbr_group.push_back(PbrStateGroup);
    //     // PbrStateGroup->add(blendState);
    //     // PbrStateGroup->add(depthState);
    //     pbr_group[i]->addChild(drawCommands);

    //     auto obj_mat = vsg::PbrMaterialValue::create(mesh->objMaterialVector[0]->at(i));
    //     gpc_group[i]->assignDescriptor("material", obj_mat);//材质在这里修改
    //     gpc_group[i]->init();
    //     gpc_group[i]->copyTo(pbr_group[i]);


    //     cadMeshSwitch->addChild(MASK_PBR_FULL, PbrStateGroup);        
    // }

    // top.transform->addChild(cadMeshSwitch);
    // top.transform->matrix = modelMatrix;
    // top.originalMatrix = modelMatrix;
    // nodePtr[""] = top;

    // scene->addChild(top.transform);
}

void ModelInstance::buildTextureSphere(vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> gpc_ibl, 
    vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> gpc_shadow, const vsg::dmat4& modelMatrix)
{
    auto options = vsg::Options::create();
    options->add(vsgXchange::all::create());
    vsg::Builder builder;
    vsg::GeometryInfo geomInfo;
    geomInfo.dx.set(500.0f, 0.0f, 0.0f);
    geomInfo.dy.set(0.0f, 500.0f, 0.0f);
    geomInfo.dz.set(0.0f, 0.0f, 500.0f);
    geomInfo.position.z-=1000;
    geomInfo.color = vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f};
    vsg::StateInfo stateinfo;

    auto sampler = vsg::Sampler::create();
    sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler->minFilter = VK_FILTER_LINEAR; // 线性过滤（平滑纹理）
    sampler->magFilter = VK_FILTER_LINEAR;

    std::vector<vsg::ref_ptr<vsg::GraphicsPipelineConfigurator>> gpc_mtrgroup;
    auto cadMeshSwitch = vsg::Switch::create();
    treeNode top;
    top.transform = vsg::MatrixTransform::create();

    //展示材质
    vsg::ref_ptr<vsg::PbrMaterialArray> mtr_mtrVector = vsg::PbrMaterialArray::create(20);
    vsg::ref_ptr<vsg::vec4Array> mtr_colors = vsg::vec4Array::create(20);
    std::vector<std::string> mtr_path;
    //黄金
    mtr_mtrVector->set(0,
        vsg::PbrMaterial{
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.0f, 0.0f, 0.0f, 1.0f},
                vsg::vec4{0.9f, 0.9f, 0.9f, 1.0f},
                vsg::vec4{0.2f, 0.2f, 0.2f, 1.0f},
                1.0f,
                0.08f
            });
    mtr_colors->set(0, vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f});
    mtr_path.push_back("/gold/Metal034_1K-JPG_Color.jpg");
    //白银
    mtr_mtrVector->set(1,
        vsg::PbrMaterial{
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.0f, 0.0f, 0.0f, 1.0f},
                vsg::vec4{0.9f, 0.9f, 0.9f, 1.0f},
                vsg::vec4{0.2f, 0.2f, 0.2f, 1.0f},
                1.0f,
                0.08f
            });
    mtr_colors->set(1, vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f});
    mtr_path.push_back("/silver/Metal041A_1K-JPG_Color.jpg");
    //铜
    mtr_mtrVector->set(2,
        vsg::PbrMaterial{
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.0f, 0.0f, 0.0f, 1.0f},
                vsg::vec4{0.9f, 0.9f, 0.9f, 1.0f},
                vsg::vec4{0.2f, 0.2f, 0.2f, 1.0f},
                1.0f,
                0.2f
            });
    mtr_colors->set(2, vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f});
    mtr_path.push_back("/bronze/Metal035_1K-JPG_Color.jpg");
    //合金
    mtr_mtrVector->set(3,
        vsg::PbrMaterial{
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.0f, 0.0f, 0.0f, 1.0f},
                vsg::vec4{0.9f, 0.9f, 0.9f, 1.0f},
                vsg::vec4{0.2f, 0.2f, 0.2f, 1.0f},
                0.8f,
                0.3f
            });
    mtr_colors->set(3, vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f});
    mtr_path.push_back("/iron/Metal009_1K-JPG_Color.jpg");
    //石头
    mtr_mtrVector->set(4,
        vsg::PbrMaterial{
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.0f, 0.0f, 0.0f, 1.0f},
                vsg::vec4{0.9f, 0.9f, 0.9f, 1.0f},
                vsg::vec4{0.04f, 0.04f, 0.04f, 1.0f},
                1.0f,
                0.6f
            });
    mtr_colors->set(4, vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f});
    mtr_path.push_back("/stone/Asphalt031_1K-JPG_Color.jpg");
    //塑料
    mtr_mtrVector->set(5,
        vsg::PbrMaterial{
                vsg::vec4{0.95f, 0.2f, 0.2f, 1.0f},
                vsg::vec4{0.0f, 0.0f, 0.0f, 1.0f},
                vsg::vec4{0.95f, 0.2f, 0.2f, 1.0f},
                vsg::vec4{0.04f, 0.04f, 0.04f, 1.0f},
                1.0f,
                0.28f
            });
    mtr_colors->set(5, vsg::vec4{0.95f, 0.2f, 0.2f, 1.0f});
    mtr_path.push_back("/plastic/Plastic007_1K-JPG_Color.jpg");
    //草坪
    mtr_mtrVector->set(6,
        vsg::PbrMaterial{
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.0f, 0.0f, 0.0f, 1.0f},
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.04f, 0.04f, 0.04f, 1.0f},
                1.0f,
                0.8f
            });
    mtr_colors->set(6, vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f});
    mtr_path.push_back("/grass/Grass005_1K-JPG_Color.jpg");
    //皮革
    mtr_mtrVector->set(7,
        vsg::PbrMaterial{
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.0f, 0.0f, 0.0f, 1.0f},
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.2f, 0.2f, 0.2f, 1.0f},
                1.0f,
                0.6f
            });
    mtr_colors->set(7, vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f});
    mtr_path.push_back("/Leather/Leather037_1K-JPG_Color.jpg");
    //木头
    mtr_mtrVector->set(8,
        vsg::PbrMaterial{
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.0f, 0.0f, 0.0f, 1.0f},
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.2f, 0.2f, 0.2f, 1.0f},
                1.0f,
                0.7f
            });
    mtr_colors->set(8, vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f});
    mtr_path.push_back("/Wood/Wood093_1K-JPG_Color.jpg");
    //木头地板
    mtr_mtrVector->set(9,
        vsg::PbrMaterial{
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.0f, 0.0f, 0.0f, 1.0f},
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.2f, 0.2f, 0.2f, 1.0f},
                1.0f,
                0.7f
            });
    mtr_colors->set(9, vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f});
    mtr_path.push_back("/WoodFloor/WoodFloor040_1K-JPG_Color.jpg");
    //大理石
    mtr_mtrVector->set(10,
        vsg::PbrMaterial{
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.0f, 0.0f, 0.0f, 1.0f},
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.2f, 0.2f, 0.2f, 1.0f},
                1.0f,
                0.3f
            });
    mtr_colors->set(10, vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f});
    mtr_path.push_back("/Marble/Marble020_1K-JPG_Color.jpg");
    //金属平面
    mtr_mtrVector->set(11,
        vsg::PbrMaterial{
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.0f, 0.0f, 0.0f, 1.0f},
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.2f, 0.2f, 0.2f, 1.0f},
                1.0f,
                0.2f
            });
    mtr_colors->set(11, vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f});
    mtr_path.push_back("/DiamondPlate/DiamondPlate008C_1K-JPG_Roughness.jpg");
    //纤维
    mtr_mtrVector->set(12,
        vsg::PbrMaterial{
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.0f, 0.0f, 0.0f, 1.0f},
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.2f, 0.2f, 0.2f, 1.0f},
                1.0f,
                0.9f
            });
    mtr_colors->set(12, vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f});
    mtr_path.push_back("/Fabric/Fabric080_1K-JPG_Color.jpg");
    //纤维1
    mtr_mtrVector->set(13,
        vsg::PbrMaterial{
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.0f, 0.0f, 0.0f, 1.0f},
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.2f, 0.2f, 0.2f, 1.0f},
                1.0f,
                0.9f
            });
    mtr_colors->set(13, vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f});
    mtr_path.push_back("/Fabric1/Fabric070_1K-JPG_Roughness.jpg");
    //Tile
    mtr_mtrVector->set(14,
        vsg::PbrMaterial{
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.0f, 0.0f, 0.0f, 1.0f},
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.2f, 0.2f, 0.2f, 1.0f},
                1.0f,
                0.9f
            });
    mtr_colors->set(14, vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f});
    mtr_path.push_back("/Tiles/Tiles101_1K-JPG_Color.jpg");
    //金属1
    mtr_mtrVector->set(15,
        vsg::PbrMaterial{
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.0f, 0.0f, 0.0f, 1.0f},
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.2f, 0.2f, 0.2f, 1.0f},
                1.0f,
                0.6f
            });
    mtr_colors->set(15, vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f});
    mtr_path.push_back("/Metal1/Metal041C_1K-JPG_Color.jpg");
    //木头1
    mtr_mtrVector->set(16,
        vsg::PbrMaterial{
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.0f, 0.0f, 0.0f, 1.0f},
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.2f, 0.2f, 0.2f, 1.0f},
                1.0f,
                0.8f
            });
    mtr_colors->set(16, vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f});
    mtr_path.push_back("/Wood1/Wood025_1K-JPG_Color.jpg");
    //图案金属
    mtr_mtrVector->set(17,
        vsg::PbrMaterial{
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.0f, 0.0f, 0.0f, 1.0f},
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.2f, 0.2f, 0.2f, 1.0f},
                1.0f,
                0.6f
            });
    mtr_colors->set(17, vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f});
    mtr_path.push_back("/PaintedMetal/PaintedMetal009_1K-JPG_Color.jpg");
    //缺陷平面
    mtr_mtrVector->set(18,
        vsg::PbrMaterial{
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.0f, 0.0f, 0.0f, 1.0f},
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.2f, 0.2f, 0.2f, 1.0f},
                1.0f,
                0.6f
            });
    mtr_colors->set(18, vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f});
    mtr_path.push_back("/SurfaceImperfections/SurfaceImperfections007_1K-JPG_Color.jpg");
    //Tiles1
    mtr_mtrVector->set(19,
        vsg::PbrMaterial{
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.0f, 0.0f, 0.0f, 1.0f},
                vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f},
                vsg::vec4{0.2f, 0.2f, 0.2f, 1.0f},
                1.0f,
                0.9f
            });
    mtr_colors->set(19, vsg::vec4{0.95f, 0.95f, 0.95f, 1.0f});
    mtr_path.push_back("/Tiles1/Tiles056_1K-JPG_Color.jpg");


    for (int i = 0; i < 20; i++)
    {
        vsg::ref_ptr<vsg::vec4Value> mtr_color = vsg::vec4Value::create(mtr_colors->at(i));
        if(i%5 == 0){
            geomInfo.position.x-=1000;
            geomInfo.position.y+=4000;
        }
        else {
            geomInfo.position.y-=1000;
        }

        auto gpc_mtr = vsg::GraphicsPipelineConfigurator::create(*gpc_ibl);
        gpc_mtrgroup.push_back(gpc_mtr);

        auto drawCommands_mtr = vsg::Commands::create();
        auto mtr_array = builder.createSphereWithArray(geomInfo, stateinfo);//实现array，indices绑定
        mtr_array.insert(mtr_array.begin()+3, mtr_color);
        vsg::ref_ptr<vsg::Data> lastData = mtr_array.back();
        vsg::ref_ptr<vsg::ushortArray> indices = lastData.cast<vsg::ushortArray>();
        mtr_array.pop_back();
        auto textureData = vsg::read_cast<vsg::Data>("../asset/data/textures" + mtr_path[i], options);
        //textureData.reset();
        gpc_mtrgroup[i]->assignTexture("diffuseMap", textureData, sampler);

        auto blendState = vsg::ColorBlendState::create();
        blendState->attachments = vsg::ColorBlendState::ColorBlendAttachments{
            {true,                                        // blendEnable
             VK_BLEND_FACTOR_SRC_ALPHA,                   // srcColorBlendFactor
             VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,         // dstColorBlendFactor
             VK_BLEND_OP_ADD,                             // colorBlendOp
             VK_BLEND_FACTOR_ONE,                         // srcAlphaBlendFactor
             VK_BLEND_FACTOR_ZERO,                        // dstAlphaBlendFactor
             VK_BLEND_OP_ADD,                             // alphaBlendOp
             VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT} // colorWriteMask
        };
        drawCommands_mtr->addChild(vsg::BindVertexBuffers::create(gpc_mtrgroup[i]->baseAttributeBinding, mtr_array));
        drawCommands_mtr->addChild(vsg::BindIndexBuffer::create(indices));//******************* */
        drawCommands_mtr->addChild(vsg::DrawIndexed::create(indices->size(), 1, 0, 0, 0));//******************* */

        auto mtrStateGroup = vsg::StateGroup::create();
        mtrStateGroup->addChild(drawCommands_mtr);

        auto gpc_mat = vsg::GraphicsPipelineConfigurator::create(*gpc_mtrgroup[i]);
        auto mat = vsg::PbrMaterialValue::create(mtr_mtrVector->at(i));
        gpc_mat->assignDescriptor("material", mat);//材质在这里修改
        auto params = vsg::floatArray::create(3);
        params->set(0, 1.f);
        params->set(1, 640.f * 2);
        params->set(2, 480.f * 2);
        gpc_mat->assignUniform("customParams", params);//是否半透明判断
        gpc_mat->pipelineStates.push_back(blendState);
        gpc_mat->init();
        gpc_mat->copyTo(mtrStateGroup);

        cadMeshSwitch->addChild(MASK_PBR_FULL, mtrStateGroup);

    }
    top.transform->addChild(cadMeshSwitch);
    
    top.transform->matrix = modelMatrix;
    top.originalMatrix = modelMatrix;
    nodePtr[""] = top;

    scene->addChild(top.transform);
}

void ModelInstance::drawLine(vsg::vec3& begin, vsg::vec3& end, vsg::ref_ptr<vsg::Group> scene)
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

void ModelInstance::buildFbInstance(CADMesh* mesh, vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::ShaderSet> pbriblShaderSet, vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> gpc_shadow, const vsg::dmat4& modelMatrix, vsg::ref_ptr<vsg::Options> options, std::string rendering_path){
    auto gpc_ibl = vsg::GraphicsPipelineConfigurator::create(pbriblShaderSet);
    vsg::ref_ptr<vsg::Group> scenegraph = vsg::Group::create();
    vsg::ref_ptr<vsg::Group> text_scenegraph = vsg::Group::create();

    auto builder = vsg::Builder::create();
    options->sharedObjects = vsg::SharedObjects::create();
    vsg::Path font_filename = vsg::findFile(vsg::Path("fonts/times.vsgt"), options->paths);
    auto font = vsg::read_cast<vsg::Font>(font_filename, options);
    if(!font){
        std::cout << "failed to read font" << std::endl;
    }
    auto pmi = mesh->pmi;
    auto geomInfo = mesh->geomInfo;
    auto stateInfo = mesh->stateInfo;

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
            auto layout = vsg::StandardLayout::create();
            layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT; //水平居中
            layout->position = textx;                                            //左右，前后，上下（xyz轴）
            layout->horizontal = vsg::vec3(0.01, 0.0, 0.0);                      //水平方向上的偏移
            layout->vertical = vsg::vec3(0.0, 0.01, 0.0);                        //垂直方向上的偏移
            layout->color = vsg::vec4(1.0, 1.0, 1.0, 1.0);                       //红、绿、蓝和alpha通道
            layout->outlineWidth = 0.0;
            layout->billboard = true;

            auto text = vsg::Text::create();
            text->text = vsg::stringValue::create(pmi[i].value);
            text->font = font;
            text->layout = layout;
            text->setup(0, options);
            text_scenegraph->addChild(text);
            drawLine(temp1, temp2, scenegraph);
            drawLine(temp3, temp4, scenegraph);
            drawLine(temp2, temp5, scenegraph);
            drawLine(temp6, temp4, scenegraph);
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
            auto layout = vsg::StandardLayout::create();
            layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT; //水平居中
            layout->position = temp3;                                            //左右，前后，上下（xyz轴）
            layout->horizontal = vsg::vec3(0.01, 0.0, 0.0);                      //水平方向上的偏移
            layout->vertical = vsg::vec3(0.0, 0.01, 0.0);                        //垂直方向上的偏移
            layout->color = vsg::vec4(1.0, 1.0, 1.0, 1.0);                       //红、绿、蓝和alpha通道
            layout->outlineWidth = 0.0; 
            layout->billboard = true;

            auto text = vsg::Text::create();
            text->text = vsg::stringValue::create(pmi[i].value);
            text->font = font;
            text->layout = layout;
            text->setup(0, options);
            text_scenegraph->addChild(text);

            drawLine(temp1, temp5, scenegraph);
            drawLine(temp6, temp2, scenegraph);
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
            auto layout = vsg::StandardLayout::create();
            layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT; //水平居中
            layout->position = temp3;                                            //左右，前后，上下（xyz轴）
            layout->horizontal = vsg::vec3(0.01, 0.0, 0.0);                      //水平方向上的偏移
            layout->vertical = vsg::vec3(0.0, 0.01, 0.0);                        //垂直方向上的偏移
            layout->color = vsg::vec4(1.0, 1.0, 1.0, 1.0);                       //红、绿、蓝和alpha通道
            layout->outlineWidth = 0.0;
            layout->billboard = true;

            auto text = vsg::Text::create();
            text->text = vsg::stringValue::create(pmi[i].value);
            text->font = font;
            text->layout = layout;
            text->setup(0, options);
            text_scenegraph->addChild(text);

            drawLine(temp1, temp2, scenegraph);
            drawLine(temp2, temp3, scenegraph);
            //drawLine(temp1, temp5, scenegraph);
            //drawLine(temp6, temp2, scenegraph);
        }
    }

    builder->options = options;
    geomInfo.color.set(1.0f, 1.0f, 1.0f, 1.0f);
    geomInfo.cullNode = false;
    stateInfo.wireframe = true;

    for (int i = 0; i < lines.size(); i++)
    {
        drawLine(lines[i].begin, lines[i].end, scenegraph);
    }

    scenegraph->addChild(builder->createLine(geomInfo, stateInfo, positions, indices));

    meshIndice = mesh->meshIndice;
    for(int i = 0; i < mesh->indicesVector.size(); i++){
        // 创建独立的DrawCommand
        // vsg::ref_ptr<vsg::vec2Array> dummyUV = vsg::vec2Array::create(mesh->verticesVector[i]->size());
        vsg::ref_ptr<vsg::vec4Value> default_color = vsg::vec4Value::create(vsg::vec4{1.0, 1.0, 1.0, 1.0});
        vsg::DataList vertexArrays = {
            mesh->verticesVector[i],
            mesh->normalsVector[i],
            mesh->UVVector[i],
            default_color
        };
        auto drawCommands = vsg::Commands::create();

        auto gpc_temp = vsg::GraphicsPipelineConfigurator::create(*gpc_ibl);
        gpc_temp_group.push_back(gpc_temp);
        auto gpc_high = vsg::GraphicsPipelineConfigurator::create(*gpc_ibl);
        gpc_high_group.push_back(gpc_high);
        auto gpc_fb = vsg::GraphicsPipelineConfigurator::create(*gpc_ibl);
        gpc_group.push_back(gpc_fb);
        auto options = vsg::Options::create();
        options->add(vsgXchange::all::create());
        auto sampler = vsg::Sampler::create();
        sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler->minFilter = VK_FILTER_LINEAR; // 线性过滤（平滑纹理）
        sampler->magFilter = VK_FILTER_LINEAR;
        
        std::string name = "Metal032";// 纹理材质，华云那边接口有点问题还没有改，没问题了之后有上面注释掉的
        std::string mtr_path;
        std::string normal_path;
        if(mesh->materialVector[i]->value().baseColorFactor.r > 0.9 && 
           mesh->materialVector[i]->value().baseColorFactor.g > 0.9 &&
           mesh->materialVector[i]->value().baseColorFactor.b > 0.9 ){
            mtr_path = rendering_path + "/asset/data/textures/gold/Metal034_1K-JPG_Color.jpg";
            normal_path = rendering_path + "/asset/data/textures/gold/isotropic.png";
        }else{
            mtr_path = rendering_path + "/asset/data/textures/"+name+"/"+name+"_2K_Color.png";
            normal_path = rendering_path + "/asset/data/textures/"+name+"/"+ "Metal032_2K_NormalGL.png";
        }
        vsg::ImageInfoList textureImageInfo;
        vsg::ImageInfoList normalTextureImageInfo;
        if(texturemap_map.find(mtr_path) == texturemap_map.end()){
            vsg::ref_ptr<vsg::Data> textureData = vsg::read_cast<vsg::Data>(mtr_path, options);
            textureImageInfo = createImageInfo(textureData, VK_FORMAT_R8G8B8A8_UNORM, textureData->width(), 
                                                textureData->height(), context);
            texturemap_map[mtr_path] = textureImageInfo;
        }else{
            textureImageInfo = texturemap_map[mtr_path];
        }
        if(normalmap_map.find(mtr_path) == normalmap_map.end()){
            vsg::ref_ptr<vsg::Data> textureData = vsg::read_cast<vsg::Data>(normal_path, options);
            normalTextureImageInfo = createImageInfo(textureData, VK_FORMAT_R8G8B8A8_UNORM, textureData->width(), 
                                                textureData->height(), context);
            normalmap_map[mtr_path] = normalTextureImageInfo;
        }else{
            normalTextureImageInfo = normalmap_map[mtr_path];
        }

        gpc_group[i]->assignTexture("diffuseMap", textureImageInfo);
        gpc_group[i]->assignTexture("normalMap", normalTextureImageInfo);
        std::cout << "texture done!";
        auto params = vsg::floatArray::create(3);
        params->set(0, 1.f);
        params->set(1, 640.f * 2);
        params->set(2, 480.f * 2);
        gpc_group[i]->assignUniform("customParams", params);//是否半透明判断

        //绑定索引
        vsg::BindVertexBuffers::create(gpc_high_group[i]->baseAttributeBinding, vertexArrays);
        vsg::BindVertexBuffers::create(gpc_temp_group[i]->baseAttributeBinding, vertexArrays);
        drawCommands->addChild(vsg::BindVertexBuffers::create(gpc_group[i]->baseAttributeBinding, vertexArrays));
        drawCommands->addChild(vsg::BindIndexBuffer::create(mesh->indicesVector[i]));
        drawCommands->addChild(vsg::DrawIndexed::create(mesh->indicesVector[i]->size(), 1, 0, 0, 0));

        auto blendState = vsg::ColorBlendState::create();
        blendState->attachments = vsg::ColorBlendState::ColorBlendAttachments{
            {true,                                        // blendEnable
             VK_BLEND_FACTOR_SRC_ALPHA,                   // srcColorBlendFactor
             VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,         // dstColorBlendFactor
             VK_BLEND_OP_ADD,                             // colorBlendOp
             VK_BLEND_FACTOR_ONE,                         // srcAlphaBlendFactor
             VK_BLEND_FACTOR_ZERO,                        // dstAlphaBlendFactor
             VK_BLEND_OP_ADD,                             // alphaBlendOp
             VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT} // colorWriteMask
        };
        auto depthState = vsg::DepthStencilState::create();
        depthState->depthWriteEnable = VK_FALSE;

        auto PbrStateGroup = vsg::StateGroup::create();
        pbr_group.push_back(PbrStateGroup);
        pbr_group[i]->addChild(drawCommands);
        gpc_group[i]->assignDescriptor("material", mesh->materialVector[i]);
        gpc_high_group[i]->assignDescriptor("material", vsg::PbrMaterialValue::create(vsg::PbrMaterial{
            vsg::vec4{1.0f, 0.0f, 0.0f, 1.0f},
            vsg::vec4{0.0f, 0.0f, 0.0f, 1.0f},
            vsg::vec4{1.0f, 0.0f, 0.0f, 1.0f},
            vsg::vec4{0.04f, 0.04f, 0.04f, 1.0f},
            1.0f,
            0.28f
        }));//材质在这里修改
        gpc_high_group[i]->init();
        gpc_group[i]->init();
        gpc_group[i]->copyTo(pbr_group[i]);
        gpc_group[i]->pipelineStates.push_back(blendState);
        gpc_group[i]->pipelineStates.push_back(depthState);
        auto cadMeshSwitch = vsg::Switch::create();
        cadMeshSwitch->addChild(MASK_PBR_FULL, PbrStateGroup);
        
        for (int j = 0; j < mesh->transformNumVector[i]; j++)
        {
            auto transforms = vsg::MatrixTransform::create();
            vsg::mat4 transforms_matrix;
            for (int m = 0; m < 4; m++)
                for (int n = 0; n < 4; n++)
                    transforms->matrix[m][n] = mesh->transformVector[i][j * 16 + m * 4 + n];
            transforms->addChild(cadMeshSwitch);
            scenegraph->addChild(transforms);
        }
    }

    treeNode top;
    top.transform = vsg::MatrixTransform::create();
    top.transform->addChild(scenegraph);
    top.transform->addChild(text_scenegraph);
    top.transform->matrix = modelMatrix;
    top.originalMatrix = modelMatrix;
    nodePtr[""] = top;

    scene->addChild(top.transform);
}


void ModelInstance::repaint(int judge){
    std::cout<<"开始重新绘制"<<std::endl;
    switch (judge)
    {
    case 0:
        for(int i=0; i<gpc_group.size(); i++){
            gpc_high_group[i]->copyTo(pbr_group[i]);
        }
        break;
    
    case 1:
        for(int i=0; i<gpc_group.size(); i++){
            gpc_group[i]->copyTo(pbr_group[i]);
        }
        break;
    
    case 2:
        for(int i=0; i<gpc_group.size(); i++){
            gpc_temp_group[i]->assignDescriptor("material", vsg::PbrMaterialValue::create(vsg::PbrMaterial{
                vsg::vec4{0.0f, 0.0f, 1.0f, 1.0f},
                vsg::vec4{0.0f, 0.0f, 0.0f, 1.0f},
                vsg::vec4{0.0f, 0.0f, 1.0f, 1.0f},
                vsg::vec4{0.04f, 0.04f, 0.04f, 1.0f},
                1.0f,
                0.28f
            }));
            gpc_temp_group[i]->init();
            gpc_temp_group[i]->copyTo(pbr_group[i]);
        }
        break;
    
    default:
        break;
    }
}


void ModelInstance::repaint(std::string componentName, int judge, vsg::PbrMaterial pbr){
    auto it = meshIndice.find(componentName);
    if(it != meshIndice.end()){
        int i = meshIndice[componentName];
        switch (judge)
        {
        case 0:
            gpc_high_group[i]->copyTo(pbr_group[i]);
            break;

        case 1:
            gpc_group[i]->copyTo(pbr_group[i]);
            break;

        case 2:
            gpc_high_group[i]->assignDescriptor("material", vsg::PbrMaterialValue::create(vsg::PbrMaterial{
                vsg::vec4{0.0f, 0.0f, 1.0f, 1.0f},
                vsg::vec4{0.0f, 0.0f, 0.0f, 1.0f},
                vsg::vec4{1.0f, 0.0f, 0.0f, 1.0f},
                vsg::vec4{0.04f, 0.04f, 0.04f, 1.0f},
                1.0f,
                0.28f
            }));
            gpc_high_group[i]->copyTo(pbr_group[i]);
            break;
        
        default:
            break;
        }
    }
}