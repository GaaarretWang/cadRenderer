#include "CADMeshIBL.h"
#include <iostream>

using namespace vsg;

template <typename T>
using ptr = vsg::ref_ptr<T>;

namespace v2
{
    template<typename T>
    vsg::vec3 toVec3(const flatbuffers::Vector<T>* flat_vector, int begin)
    {
        // return vsg::vec3(flat_vector->Get(begin) / 1000, flat_vector->Get(begin + 1) / 1000, flat_vector->Get(begin + 2) / 1000);
        return vsg::vec3(flat_vector->Get(begin), flat_vector->Get(begin + 1), flat_vector->Get(begin + 2));
    }

    template<typename T>
    vsg::vec2 toVec2(const flatbuffers::Vector<T>* flat_vector, int begin)
    {
        return vsg::vec2(flat_vector->Get(begin), flat_vector->Get(begin + 1));
    }

    vsg::vec3 toVec3(const flatbuffers::String* string_vector)
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

    std::size_t hash_vec2(const vsg::t_vec2<float>& vec) noexcept
    {
        std::hash<float> hasher;
        std::size_t seed = 0;
        for (size_t i = 0; i < 2; ++i)
        {
            seed ^= hasher(vec[i]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }

    std::size_t hash_vec3(const vsg::t_vec3<float>& vec) noexcept
    {
        std::hash<float> hasher;
        std::size_t seed = 0;
        for (size_t i = 0; i < 3; ++i)
        {
            seed ^= hasher(vec[i]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }

    std::size_t hash_vec4(const vsg::t_vec4<float>& vec) noexcept
    {
        std::hash<float> hasher;
        std::size_t seed = 0;
        for (size_t i = 0; i < 4; ++i)
        {
            seed ^= hasher(vec[i]) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
    
    void CADMesh::loadFile(const std::string& path, bool fullNormal)
    {
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

        flatbuffers::FlatBufferBuilder builder;
        builder.PushFlatBuffer(buffer_data, buffer_size);
        auto renderFlatBuffer = RenderFlatBuffer::GetRenderFlatBufferDoc(builder.GetBufferPointer());
        
        if (renderFlatBuffer)
            loadRenderFlatBuffer(renderFlatBuffer, fullNormal);
    }

    void CADMesh::loadRenderFlatBuffer(const RenderFlatBuffer::RenderFlatBufferDoc* renderFlatBuffer, bool fullNormal)
    {
        mProtos.clear();
        mProtoMap.clear();
        mInstances.clear();
        mInstanceMap.clear();

        std::vector<ProtoInfo> _protos;
        std::unordered_map<std::string, uint32_t> _protoMap;

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

            std::unordered_map<TinyModelVertex, uint32_t> uniqueVertices;
            std::vector<TinyModelVertex> mVertices{};
            std::vector<uint32_t> mIndices{};

            for (int m = 0; m < protofbs->Models()->size(); m++)
            {
                const RenderFlatBuffer::Model* modelfbs = protofbs->Models()->Get(m);
                auto modelGeo = modelfbs->Geo();
                //std::cout << "modelfbs->Type()->str()" << modelfbs->Type()->str() << std::endl;
                if (modelfbs->Type()->str() == "mesh")
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
                            {
                                uniqueVertices[vertex] = static_cast<uint32_t>(mVertices.size());
                                mVertices.push_back(vertex);
                            }
                            mIndices.push_back(uniqueVertices[vertex]);
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
            int Nodenumber = mVertices.size();   //顶点、法向个数
            int Indicesnumber = mIndices.size(); //索引个数

            vsg::ref_ptr<vsg::vec3Array> vertices = vsg::vec3Array::create(Nodenumber);
            vsg::ref_ptr<vsg::vec3Array> normals = vsg::vec3Array::create(Nodenumber);
            vsg::ref_ptr<vsg::uintArray> indices = vsg::uintArray::create(Indicesnumber);

            //读取顶点
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

            uint32_t idx = _protos.size();
            ProtoInfo protoInfo = {};
            protoInfo.fbIndex = p;
            protoInfo.arrayIdx = idx;
            protoInfo.triangleNum = static_cast<uint32_t>(mIndices.size() / 3);

            _protos.push_back(std::move(protoInfo));
            _protoMap[protofbs->ProtoID()->str()] = idx;
        }
        std::swap(mProtos, _protos);
        std::swap(mProtoMap, _protoMap);

        std::vector<InstanceInfo> _instances;
        std::unordered_map<std::string, uint32_t> _instanceMap;
        {
            vsg::mat4 matrix = vsg::mat4();
            auto rootTransform = vsg::MatrixTransform::create();

            treeNode rootTreeNode;
            rootTreeNode.transform = rootTransform;
            rootTreeNode.transform->matrix = matrix;
            rootTreeNode.originalMatrix = matrix;            

            treeNodeV2 rootTreeNodeV2 = {};
            rootTreeNodeV2.originalMatrix = matrix;

            InstanceInfo instanceInfo = {};
            instanceInfo.fbIndex = (uint32_t)-1;
            instanceInfo.protoIdx = (uint32_t)-1;
            instanceInfo.instanceId = (uint32_t)-1;
            instanceInfo.parentId = "23101111111113123123";
            instanceInfo.parentIdx = (uint32_t)-1;
            //instanceInfo.nodePtr = rootTreeNode;
            instanceInfo.node = rootTreeNodeV2;
            _instances.push_back(instanceInfo);
            _instanceMap[""] = 0;
        }

        //创建树的所有节点
        for (int i = 0; i < renderFlatBuffer->Bom()->size(); i++)
        {
            auto instance = renderFlatBuffer->Bom()->Get(i);
            auto protoId = instance->ProtoID()->str();
            auto currentId = instance->InstanceID()->str();
           
            if (mProtoMap.find(protoId) != mProtoMap.end()){
                //treeNode node;
                vsg::mat4 matrix = vsg::mat4(1.0f);
                for (int m = 0; m < 4; m++)
                    for (int n = 0; n < 3; n++)
                    {
                        matrix[m][n] = instance->Matrix()->Get(3 * m + n);
                    }
                //node.transform = vsg::MatrixTransform::create();
                //node.transform->matrix = matrix;
                //node.originalMatrix = matrix;

                treeNodeV2 nodeV2 = {};
                nodeV2.originalMatrix = matrix;

                uint32_t idx = _instances.size();
                InstanceInfo instanceInfo = {};
                instanceInfo.instanceId = currentId;
                instanceInfo.parentId = instance->ParentID()->str();
                instanceInfo.fbIndex = i;
                instanceInfo.arrayIdx = idx;
                instanceInfo.protoIdx = mProtoMap[protoId];
                //instanceInfo.nodePtr = node;
                instanceInfo.node = nodeV2;

                _instances.push_back(std::move(instanceInfo));
                _instanceMap[currentId] = idx;
            } else {
                //treeNode node;
                vsg::mat4 matrix = vsg::mat4(1.0f);
                treeNodeV2 nodeV2 = {};
                nodeV2.originalMatrix = matrix;

                uint32_t idx = _instances.size();
                InstanceInfo instanceInfo = {};
                instanceInfo.instanceId = currentId;
                instanceInfo.parentId = instance->ParentID()->str();
                instanceInfo.fbIndex = i;
                instanceInfo.arrayIdx = idx;
                instanceInfo.protoIdx = -1;
                //instanceInfo.nodePtr = node;
                instanceInfo.node = nodeV2;

                _instances.push_back(std::move(instanceInfo));
                _instanceMap[currentId] = idx;
            }
        }

        std::swap(mInstances, _instances);
        std::swap(mInstanceMap, _instanceMap);

        //处理树中节点的关系
        uint32_t totalTriangleNum = 0;
        for (int i = 0; i < instanceCount(); i++)
        {
            InstanceInfo& currentInstance = mInstances[i];

            if (currentInstance.protoIdx != (uint32_t)-1)
                totalTriangleNum += mProtos[currentInstance.protoIdx].triangleNum;
            //while(true)
            //{
                //if (added.find(currentInstance.instanceId) == added.end())
                //{
            if (mInstanceMap.find(currentInstance.parentId) != mInstanceMap.end())
            {
                InstanceInfo& parentInstance = mInstances[mInstanceMap[currentInstance.parentId]];
                std::cout << currentInstance.instanceId << "(idx=" << currentInstance.arrayIdx << "), parent ="
                          << parentInstance.instanceId << "(idx=" << parentInstance.arrayIdx << ")" << std::endl;
                currentInstance.parentIdx = parentInstance.arrayIdx;
                parentInstance.node.children.push_back(currentInstance.arrayIdx);
            }
                    //if (findInstance(currentInstance.parentId, &parentInstance))
                    //{
                    //    mInstances[currentInstance.arrayIdx].parentIdx = parentInstance.arrayIdx;
                    //    parentInstance.node.children.push_back(currentInstance.arrayIdx);
                    //    //added.insert(currentInstance.instanceId);
                    //}
                //}
                //if (currentInstance.instanceId == "")
                //    break;
                //currentInstance = parentInstance;
                //if (!findInstance(parentInstance.parentId, &parentInstance))
                //    break;
            //}
        }
        std::cout << "totalTriangleNum = " << totalTriangleNum << std::endl;
    }

    //void CADMesh::explode()
    //{
    //    auto renderFlatBuffer = RenderFlatBuffer::GetRenderFlatBufferDoc(builder_out.GetBufferPointer());
    //    for (int i = 0; i < renderFlatBuffer->Bom()->size(); i++)
    //    {
    //        auto instance = renderFlatBuffer->Bom()->Get(i);
    //        auto currentId = instance->InstanceID()->str();
    //        auto parentId = instance->ParentID()->str();

    //        vsg::dvec3 center((nodePtr[currentId].bounds.max + nodePtr[currentId].bounds.min) / 2.0);
    //        vsg::dvec3 parentCenter((nodePtr[parentId].bounds.max + nodePtr[parentId].bounds.min) / 2.0);

    //        auto distance = center - parentCenter;
    //        auto moveDistance = distance * 0.2;
    //        //std::cout << "moveDistance" << moveDistance << std::endl;
    //        nodePtr[currentId].transform->matrix = nodePtr[currentId].transform->matrix * vsg::translate(moveDistance);
    //    }
    //}

    //void CADMesh::recover()
    //{
    //    auto renderFlatBuffer = RenderFlatBuffer::GetRenderFlatBufferDoc(builder_out.GetBufferPointer());
    //    for (int i = 0; i < renderFlatBuffer->Bom()->size(); i++)
    //    {
    //        auto instance = renderFlatBuffer->Bom()->Get(i);
    //        auto currentId = instance->InstanceID()->str();
    //        auto parentId = instance->ParentID()->str();

    //        nodePtr[currentId].transform->matrix = nodePtr[currentId].originalMatrix;
    //    }
    //}

    vsg::ref_ptr<vsg::Node> CADMesh::createNodeSeperateDraw(vsg::ref_ptr<GraphicsPipelineConfigurator> gpc)//对单个实例进行绘制
    {
        vsg::ref_ptr<vsg::vec4Value> default_color = vsg::vec4Value::create(vsg::vec4{1.0, 1.0, 1.0, 1.0});
        //vsg::ref_ptr<vsg::PhongMaterialValue> default_material = vsg::PhongMaterialValue::create();
        //default_material->value().ambient.set(0.9, 0.9, 0.9, 1.0);
        //default_material->value().diffuse.set(0.55, 0.55, 0.55, 1.0);
        //default_material->value().specular.set(0.7, 0.7, 0.7, 1.0);
        //default_material->value().shininess = 25;

        //vsg::DataList dummyArrays;
        //gpc->assignArray(dummyArrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, verticesVector[0]);
        //gpc->assignArray(dummyArrays, "vsg_Normal", VK_VERTEX_INPUT_RATE_VERTEX, normalsVector[0]);
        //gpc->assignArray(dummyArrays, "vsg_Color", VK_VERTEX_INPUT_RATE_INSTANCE, default_color);
        //gpc->assignDescriptor("material", default_material);
        //gpc->init();
        vsg::ref_ptr<vsg::vec2Array> dummyUV = vsg::vec2Array::create(1);
        
        std::vector<ptr<MatrixTransform>> instanceTransforms(instanceCount());
        for(auto it = instanceTransforms.begin(); it != instanceTransforms.end(); it++) {//出现非常量引用必须为左值的错误,故修改为auto
            (*it) = MatrixTransform::create();
        }
        ptr<MatrixTransform> rootTransform;
        for(auto instance_it = mInstances.begin(); instance_it != mInstances.end(); instance_it++) {
            auto& thisTransform = instanceTransforms[instance_it->arrayIdx];
            thisTransform->matrix = instance_it->node.originalMatrix;
            if (instance_it->parentIdx == (uint32_t)-1){
                rootTransform = thisTransform;
            } else{
                auto& parentTransform = instanceTransforms[instance_it->parentIdx];
                parentTransform->addChild(thisTransform);
                std::cout << "attach \"" << instance_it->instanceId << "\" to \"" << mInstances[instance_it->parentIdx].instanceId << "\"" << std::endl;
            }
            
            if (instance_it->protoIdx == (uint32_t)-1) continue;
            // create draw command;
            auto drawCommands = vsg::Commands::create();
            vsg::DataList attributes = {
                verticesVector[instance_it->protoIdx],
                normalsVector[instance_it->protoIdx],
                dummyUV,
                default_color
            };

            drawCommands->addChild(vsg::BindVertexBuffers::create(gpc->baseAttributeBinding, attributes));
            drawCommands->addChild(vsg::BindIndexBuffer::create(indicesVector[instance_it->protoIdx]));
            drawCommands->addChild(vsg::DrawIndexed::create(indicesVector[instance_it->protoIdx]->size(), 1, 0, 0, 0));
            thisTransform->addChild(drawCommands);
        }
        auto stateGroup = vsg::StateGroup::create();
        gpc->copyTo(stateGroup);
        //rootTransform->addChild(stateGroup);
        //return rootTransform;
        stateGroup->addChild(rootTransform);
        return stateGroup;
    }

    vsg::ref_ptr<vsg::Node> CADMesh::createDrawCmd(vsg::ref_ptr<GraphicsPipelineConfigurator> gpc)//将绘制命令(drawCommands)部分独立为函数
    {
        vsg::ref_ptr<vsg::vec4Value> default_color = vsg::vec4Value::create(vsg::vec4{1.0, 1.0, 1.0, 1.0});
        vsg::ref_ptr<vsg::vec2Array> dummyUV = vsg::vec2Array::create(1);

        std::vector<ptr<MatrixTransform>> instanceTransforms(instanceCount());
        for (auto it = instanceTransforms.begin(); it != instanceTransforms.end(); it++)
        {
            (*it) = MatrixTransform::create();
        }
        ptr<MatrixTransform> rootTransform;
        for (auto instance_it = mInstances.begin(); instance_it != mInstances.end(); instance_it++)
        {
            auto& thisTransform = instanceTransforms[instance_it->arrayIdx];
            thisTransform->matrix = instance_it->node.originalMatrix;
            if (instance_it->parentIdx == (uint32_t)-1)
            {
                rootTransform = thisTransform;
            }
            else
            {
                auto& parentTransform = instanceTransforms[instance_it->parentIdx];
                parentTransform->addChild(thisTransform);
                std::cout << "attach \"" << instance_it->instanceId << "\" to \"" << mInstances[instance_it->parentIdx].instanceId << "\"" << std::endl;
            }

            if (instance_it->protoIdx == (uint32_t)-1) continue;
            // create draw command;
            auto drawCommands = vsg::Commands::create();
            vsg::DataList attributes = {
                verticesVector[instance_it->protoIdx],
                normalsVector[instance_it->protoIdx],
                dummyUV,
                default_color};

            drawCommands->addChild(vsg::BindVertexBuffers::create(gpc->baseAttributeBinding, attributes));
            drawCommands->addChild(vsg::BindIndexBuffer::create(indicesVector[instance_it->protoIdx]));
            drawCommands->addChild(vsg::DrawIndexed::create(indicesVector[instance_it->protoIdx]->size(), 1, 0, 0, 0));
            thisTransform->addChild(drawCommands);
        }
        return rootTransform;
    }

    vsg::ref_ptr<vsg::Node> CADMesh::test(vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> gpc)
    {
        //2. 设置材质参数
#if (1) //金属
        auto plane_colors = vsg::vec4Value::create(vsg::vec4{1.0, 1.0, 1.0, 1.0f});
        vsg::ref_ptr<vsg::PbrMaterialValue> plane_mat;
        plane_mat = vsg::PbrMaterialValue::create();
        plane_mat->value().roughnessFactor = 0.8f;
        plane_mat->value().metallicFactor = 0.0f;

#elif (0) //塑料
        auto plane_colors = vsg::vec4Value::create(vsg::vec4{0, 0, 0, 1.0f});
        auto plane_mat = vsg::PhongMaterialValue::create();
        plane_mat->value().ambient.set(0, 0, 0, 1.0f);
        plane_mat->value().diffuse.set(0.55, 0.55, 0.55, 1.0f);
        plane_mat->value().specular.set(0.7, 0.7, 0.7, 1.0f);
        plane_mat->value().shininess = 25;

#elif (0) //纯色
        auto plane_colors = vsg::vec4Value::create(vsg::vec4{1, 1, 1, 1.0f});
        auto plane_mat = vsg::PhongMaterialValue::create();
        plane_mat->value().ambient.set(1, 0, 0, 1.0f);
        plane_mat->value().diffuse.set(1, 0, 0, 1.0f);
        plane_mat->value().specular.set(1, 0, 0, 1.0f);
        plane_mat->value().shininess = 0.25;
#endif

        //    //3. （手动）设置顶点、法向、材质坐标等信息，传入着色器
        float plane_length = 5000.0f;
        auto plane_vertices = vsg::vec3Array::create(
            {{-plane_length, -plane_length, -plane_length / 10},
             {plane_length, -plane_length, -plane_length / 10},
             {plane_length, plane_length, -plane_length / 10},
             {-plane_length, plane_length, -plane_length / 10}});

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

        //---------------------------------------阴影模拟------------------------------------------//
        //传入手写立方体的参数
        vsg::DataList plane_vertexArrays = {
            plane_vertices,
            plane_normals,
            plane_texcoords,
            plane_colors
        };

        auto plane_drawCommands = vsg::Commands::create();
        plane_drawCommands->addChild(vsg::BindVertexBuffers::create(gpc->baseAttributeBinding, plane_vertexArrays));
        plane_drawCommands->addChild(vsg::BindIndexBuffer::create(plane_indices));
        //cout << mesh.indexes->size() << endl;
        plane_drawCommands->addChild(vsg::DrawIndexed::create(6, 1, 0, 0, 0));

        auto plane_stateGroup = vsg::StateGroup::create();
        gpc->copyTo(plane_stateGroup);
        //transform->subgraphRequiresLocalFrustum = false;
        // add drawCommands to StateGroup
        plane_stateGroup->addChild(plane_drawCommands);
        return plane_stateGroup;
    }

    vsg::ref_ptr<vsg::Node> CADMesh::testCube(vsg::ref_ptr<vsg::GraphicsPipelineConfigurator> gpc)
    {
        //2. 设置材质参数
        auto plane_colors = vsg::vec4Value::create(vsg::vec4{1.0, 1.0, 1.0, 1.0f});
        vsg::ref_ptr<vsg::PbrMaterialValue> plane_mat;
        plane_mat = vsg::PbrMaterialValue::create();
        plane_mat->value().roughnessFactor = 0.8f;
        plane_mat->value().metallicFactor = 0.0f;

        //    //3. （手动）设置顶点、法向、材质坐标等信息，传入着色器
        auto vertices = vsg::vec3Array::create({// Back
            {-1.0f, -1.0f, -1.0f},
            {1.0f, -1.0f, -1.0f},
            {-1.0f, 1.0f, -1.0f},
            {1.0f, 1.0f, -1.0f},

            // Front
            {-1.0f, -1.0f, 1.0f},
            {1.0f, -1.0f, 1.0f},
            {-1.0f, 1.0f, 1.0f},
            {1.0f, 1.0f, 1.0f},

            // Left
            {-1.0f, -1.0f, -1.0f},
            {-1.0f, -1.0f, 1.0f},
            {-1.0f, 1.0f, -1.0f},
            {-1.0f, 1.0f, 1.0f},

            // Right
            {1.0f, -1.0f, -1.0f},
            {1.0f, -1.0f, 1.0f},
            {1.0f, 1.0f, -1.0f},
            {1.0f, 1.0f, 1.0f},

            // Bottom
            {-1.0f, -1.0f, -1.0f},
            {-1.0f, -1.0f, 1.0f},
            {1.0f, -1.0f, -1.0f},
            {1.0f, -1.0f, 1.0f},

            // Top
            {-1.0f, 1.0f, -1.0f},
            {-1.0f, 1.0f, 1.0f},
            {1.0f, 1.0f, -1.0f},
            {1.0f, 1.0f, 1.0}});

        auto normals = vsg::vec3Array::create({// Back
            {0.0f, 0.0f, -1.0f},
            {0.0f, 0.0f, -1.0f},
            {0.0f, 0.0f, -1.0f},
            {0.0f, 0.0f, -1.0f},

            // Front
            {0.0f, 0.0f, 1.0f},
            {0.0f, 0.0f, 1.0f},
            {0.0f, 0.0f, 1.0f},
            {0.0f, 0.0f, 1.0f},

            // Left
            {-1.0f, 0.0f, 0.0f},
            {-1.0f, 0.0f, 0.0f},
            {-1.0f, 0.0f, 0.0f},
            {-1.0f, 0.0f, 0.0f},

            // Right
            {1.0f, 0.0f, 0.0f},
            {1.0f, 0.0f, 0.0f},
            {1.0f, 0.0f, 0.0f},
            {1.0f, 0.0f, 0.0f},

            // Bottom
            {0.0f, -1.0f, 0.0f},
            {0.0f, -1.0f, 0.0f},
            {0.0f, -1.0f, 0.0f},
            {0.0f, -1.0f, 0.0f},

            // Top
            {0.0f, 1.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 1.0f, 0.0f}
        });

        auto uvs = vsg::vec2Array::create({// Back
            {0.0f, 0.0f},
            {0.0f, 0.0f},
            {0.0f, 0.0f},
            {0.0f, 0.0f},

            // Front
            {0.0f, 0.0f},
            {0.0f, 0.0f},
            {0.0f, 0.0f},
            {0.0f, 0.0f},

            // Left
            {0.0f, 0.0f},
            {0.0f, 0.0f},
            {0.0f, 0.0f},
            {0.0f, 0.0f},

            // Right
            {0.0f, 0.0f},
            {0.0f, 0.0f},
            {0.0f, 0.0f},
            {0.0f, 0.0f},

            // Bottom
            {0.0f, 0.0f},
            {0.0f, 0.0f},
            {0.0f, 0.0f},
            {0.0f, 0.0f},

            // Top
            {0.0f, 0.0f},
            {0.0f, 0.0f},
            {0.0f, 0.0f},
            {0.0f, 0.0f}
        });

        auto indices = vsg::ushortArray::create({// Back
            0, 2, 1,
            1, 2, 3,

            // Front
            6, 4, 5,
            7, 6, 5,

            // Left
            10, 8, 9,
            11, 10, 9,

            // Right
            14, 13, 12,
            15, 13, 14,

            // Bottom
            17, 16, 19,
            19, 16, 18,

            // Top
            23, 20, 21,
            22, 20, 23});

        //---------------------------------------阴影模拟------------------------------------------//
        //传入手写立方体的参数
        vsg::DataList plane_vertexArrays = {
            vertices,
            normals,
            uvs,
            plane_colors};

        auto plane_drawCommands = vsg::Commands::create();
        plane_drawCommands->addChild(vsg::BindVertexBuffers::create(gpc->baseAttributeBinding, plane_vertexArrays));
        plane_drawCommands->addChild(vsg::BindIndexBuffer::create(indices));
        //cout << mesh.indexes->size() << endl;
        plane_drawCommands->addChild(vsg::DrawIndexed::create(36, 1, 0, 0, 0));

        auto plane_stateGroup = vsg::StateGroup::create();
        gpc->copyTo(plane_stateGroup);
        // set up model transformation node
        auto plane_transform = vsg::MatrixTransform::create(); //用于位置变换
        //transform->subgraphRequiresLocalFrustum = false;
        // add drawCommands to StateGroup
        plane_stateGroup->addChild(plane_drawCommands);
        return plane_stateGroup;
    }
}