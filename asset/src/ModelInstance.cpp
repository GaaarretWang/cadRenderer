#include "ModelInstance.h"

template <typename T>
using ptr = vsg::ref_ptr<T>;

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
            auto cadMeshShadowStateGroup = vsg::StateGroup::create();
            cadMeshShadowStateGroup->addChild(drawCommands);
            gpc_shadow->copyTo(cadMeshShadowStateGroup);
            auto cadMeshSwitch = vsg::Switch::create();
            cadMeshSwitch->addChild(MASK_MODEL, PbrStateGroup);
            cadMeshSwitch->addChild(MASK_DRAW_SHADOW, cadMeshShadowStateGroup);



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

void ModelInstance::buildObjInstanceIBL(CADMesh* mesh, vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> gpc_ibl,vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> gpc_shadow, const vsg::dmat4& modelMatrix){
    vsg::ref_ptr<vsg::vec4Value> default_color = vsg::vec4Value::create(vsg::vec4{1.0, 1.0, 1.0, 1.0});
    std::cout<< "objUVVector count :" << mesh->objUVVector[0]->size() << std::endl;
    std::cout<< "objNormalVector count :" << mesh->objNormalsVector[0]->size() << std::endl;
    std::cout<< "objVerticesVector count :" << mesh->objVerticesVector[0]->size() << std::endl;
    std::cout<< "objIndicesVector count :" << mesh->objIndicesVector[0][0]->size() << std::endl;
    auto objmaterial = mesh->objMaterialVector[0];
    auto object_mat = vsg::PbrMaterialValue::create();
    object_mat->value().roughnessFactor = 0.1f;
    object_mat->value().metallicFactor = 0.9f;
    object_mat->value().baseColorFactor = vsg::vec4(1.0f, 1.0f, 0.0f, 1.0f);
    // object_mat->value() = objmaterial->at(0);

    std::cout << mesh->objVerticesVector[0]->size() << "**********************************" << std::endl;
    // Create the graphics pipeline configurator
    vsg::DataList OBJ_vertexArrays = {
            mesh->objVerticesVector[0],
            mesh->objNormalsVector[0],
            mesh->objUVVector[0],
            default_color
        };
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


    std::vector<vsg::ref_ptr<vsg::GraphicsPipelineConfigurator>> gpc_group;
    std::vector<vsg::ref_ptr<vsg::GraphicsPipelineConfigurator>> gpc_mtrgroup;
    auto cadMeshSwitch = vsg::Switch::create();
    treeNode top;
    top.transform = vsg::MatrixTransform::create();

    for(int i = 0; i < mesh->objIndicesVector[0].size(); i++){

        auto gpc_obj = vsg::GraphicsPipelineConfigurator::create(*gpc_ibl);
        gpc_group.push_back(gpc_obj);
        
        //绑定纹理
        if(mesh->objTexturePath[0][i]!=""){
            auto textureData = vsg::read_cast<vsg::Data>("../asset/data/obj/Medieval_building/textures/" + mesh->objTexturePath[0][mesh->objMaterialIndice[0][i]], options);
            
            gpc_group[i]->assignTexture("diffuseMap", textureData, sampler);
        }

        //绑定索引
        auto drawCommands = vsg::Commands::create();
        drawCommands->addChild(vsg::BindVertexBuffers::create(gpc_group[i]->baseAttributeBinding, OBJ_vertexArrays));
        drawCommands->addChild(vsg::BindIndexBuffer::create(mesh->objIndicesVector[0][i]));//******************* */
        drawCommands->addChild(vsg::DrawIndexed::create(mesh->objIndicesVector[0][i]->size(), 1, 0, 0, 0));//******************* */

        auto PbrStateGroup = vsg::StateGroup::create();
        PbrStateGroup->addChild(drawCommands);

        auto gpc_object = vsg::GraphicsPipelineConfigurator::create(*gpc_group[i]);
        auto obj_mat = vsg::PbrMaterialValue::create(mesh->objMaterialVector[0]->at(i));
        gpc_object->assignDescriptor("material", obj_mat);//材质在这里修改
        gpc_object->init();
        gpc_object->copyTo(PbrStateGroup);

        auto cadMeshShadowStateGroup = vsg::StateGroup::create();
        cadMeshShadowStateGroup->addChild(drawCommands);
        gpc_shadow->copyTo(cadMeshShadowStateGroup);

        cadMeshSwitch->addChild(MASK_PBR_FULL, PbrStateGroup);
        cadMeshSwitch->addChild(MASK_DRAW_SHADOW, cadMeshShadowStateGroup);
        
        //添加材质物体
        //top.transform->addChild(builder.createSphere(geomInfo, stateinfo));
    }

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

        drawCommands_mtr->addChild(vsg::BindVertexBuffers::create(gpc_mtrgroup[i]->baseAttributeBinding, mtr_array));
        drawCommands_mtr->addChild(vsg::BindIndexBuffer::create(indices));//******************* */
        drawCommands_mtr->addChild(vsg::DrawIndexed::create(indices->size(), 1, 0, 0, 0));//******************* */

        auto mtrStateGroup = vsg::StateGroup::create();
        mtrStateGroup->addChild(drawCommands_mtr);

        auto gpc_mat = vsg::GraphicsPipelineConfigurator::create(*gpc_mtrgroup[i]);
        auto mat = vsg::PbrMaterialValue::create(mtr_mtrVector->at(i));
        auto obj_mat = vsg::PbrMaterialValue::create(mesh->objMaterialVector[0]->at(i));
        gpc_mat->assignDescriptor("material", mat);//材质在这里修改
        gpc_mat->init();
        gpc_mat->copyTo(mtrStateGroup);

        auto materialShadowStateGroup = vsg::StateGroup::create();
        materialShadowStateGroup->addChild(drawCommands_mtr);
        gpc_shadow->copyTo(materialShadowStateGroup);

        cadMeshSwitch->addChild(MASK_PBR_FULL, mtrStateGroup);
        cadMeshSwitch->addChild(MASK_DRAW_SHADOW, materialShadowStateGroup);

    }
    top.transform->addChild(cadMeshSwitch);
    

    // top.transform->addChild(builder.createBox(geomInfo, stateinfo));
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

void ModelInstance::buildFbInstance(CADMesh* mesh, vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> gpc_ibl, vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> gpc_shadow, const vsg::dmat4& modelMatrix, vsg::ref_ptr<vsg::Options> options){
    
    vsg::ref_ptr<vsg::Group> scenegraph = vsg::Group::create();
    vsg::ref_ptr<vsg::Group> text_scenegraph = vsg::Group::create();

    auto builder = vsg::Builder::create();
    options->sharedObjects = vsg::SharedObjects::create();
    std::string font_filename = vsg::findFile("fonts/times.vsgt", options->paths);
    auto font = vsg::read_cast<vsg::Font>(font_filename, options);
    if(!font){
        std::cout << "failed to read font" << std::endl;
    }
    auto pmi = mesh->pmi;
    auto geomInfo = mesh->geomInfo;
    auto stateInfo = mesh->stateInfo;
    std::string a;

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

    for(int i = 0; i < mesh->indicesVector.size(); i++){
        // 创建独立的DrawCommand
        // vsg::ref_ptr<vsg::vec2Array> dummyUV = vsg::vec2Array::create(mesh->verticesVector[i]->size());
        vsg::ref_ptr<vsg::vec4Value> default_color = vsg::vec4Value::create(
            mesh->materialVector[i]->value().baseColorFactor);
        vsg::DataList vertexArrays = {
            mesh->verticesVector[i],
            mesh->normalsVector[i],
            mesh->indicesVector[i],
            default_color
        };
        auto drawCommands = vsg::Commands::create();

        auto gpc_fb = vsg::GraphicsPipelineConfigurator::create(*gpc_ibl);
        auto options = vsg::Options::create();
        options->add(vsgXchange::all::create());
        auto sampler = vsg::Sampler::create();
        sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler->minFilter = VK_FILTER_LINEAR; // 线性过滤（平滑纹理）
        sampler->magFilter = VK_FILTER_LINEAR;
        // std::string name = mesh->materialNameVector[i];
        std::string name = "Metal032";// 纹理材质，华云那边接口有点问题还没有改，没问题了之后有上面注释掉的
        std::string mtr_path = "../asset/data/textures/"+name+"/"+name+"_2K_Color.png";
        std::cout << mtr_path << std::endl;
        auto textureData = vsg::read_cast<vsg::Data>(mtr_path, options);
        gpc_fb->assignTexture("diffuseMap", textureData, sampler);

        //绑定索引
        drawCommands->addChild(vsg::BindVertexBuffers::create(gpc_fb->baseAttributeBinding, vertexArrays));
        drawCommands->addChild(vsg::BindIndexBuffer::create(mesh->indicesVector[i]));
        drawCommands->addChild(vsg::DrawIndexed::create(mesh->indicesVector[i]->size(), 1, 0, 0, 0));

        auto PbrStateGroup = vsg::StateGroup::create();
        PbrStateGroup->addChild(drawCommands);
        auto gpc_object = vsg::GraphicsPipelineConfigurator::create(*gpc_fb);
        gpc_object->assignDescriptor("material", mesh->materialVector[i]);
        gpc_object->init();
        gpc_object->copyTo(PbrStateGroup);
        auto cadMeshShadowStateGroup = vsg::StateGroup::create();
        cadMeshShadowStateGroup->addChild(drawCommands);
        gpc_shadow->copyTo(cadMeshShadowStateGroup);
        auto cadMeshSwitch = vsg::Switch::create();
        cadMeshSwitch->addChild(MASK_MODEL, PbrStateGroup);
        cadMeshSwitch->addChild(MASK_DRAW_SHADOW, cadMeshShadowStateGroup);
        
        //std::cout<<i<<std::endl;
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