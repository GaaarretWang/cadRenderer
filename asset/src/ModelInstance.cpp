#include "ModelInstance.h"

void ModelInstance::buildInstance(CADMesh* mesh, vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::ShaderSet> shader, const vsg::dmat4& modelMatrix){
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
    
    //创建纹理或遮罩
    bool addTexture = 0;
    auto options = vsg::Options::create();
    vsg::Path textureFile("./data/textures/lz.vsgb");
    if (textureFile && addTexture)
    {
        auto textureData = vsg::read_cast<vsg::Data>(textureFile, options);
        if (!textureData)
        {
            std::cout << "Could not read texture file : " << textureFile << std::endl;
        }
        graphicsPipelineConfig->assignTexture("diffuseMap", textureData);
    }

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
    auto rs = vsg::RasterizationState::create();
    rs->cullMode = VK_CULL_MODE_BACK_BIT;
    graphicsPipelineConfig->pipelineStates.push_back(rs);

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