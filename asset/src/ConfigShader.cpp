#include "ConfigShader.h"
#include <iostream>


/**
 * @brief 构建阴影Pass使用的ShaderSet
 * 
 * @param vert 顶点着色器文件路径
 * @param frag 片元着色器文件路径
 * @return vsg::ref_ptr<vsg::ShaderSet> 配置好的ShaderSet指针
 * 
 * ShaderSet是VulkanSceneGraph中用于管理着色器及其关联状态的核心对象。
 * 它封装了顶点/片元着色器、属性绑定、描述符绑定、推送常量等信息。
 */
vsg::ref_ptr<vsg::ShaderSet> ConfigShader::buildShadowShader(std::string vert, std::string frag)
{
    // 创建Options对象用于配置着色器加载选项
    auto options = vsg::Options::create();
    // 读取顶点着色器文件，read_cast会将文件内容转换为ShaderStage对象
    // vert: 顶点着色器(.vert或.spv)的文件路径
    auto vertexShader = vsg::read_cast<vsg::ShaderStage>(vert, options);
    // 读取片元着色器文件
    // frag: 片元着色器(.frag或.spv)的文件路径
    auto fragmentShader = vsg::read_cast<vsg::ShaderStage>(frag, options);

    // 创建ShaderSet并传入着色器阶段
    // 参数: vsg::ShaderStages - 包含顶点 和片元着色器的容器
    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});

    // 定义描述符集编号
    // VIEW_DESCRIPTOR_SET = 1: 用于视图相关数据(如光源、视口、阴影图)
    // MATERIAL_DESCRIPTOR_SET = 2: 用于材质相关数据(如纹理、材质属性)
    #define VIEW_DESCRIPTOR_SET 1
    #define MATERIAL_DESCRIPTOR_SET 2

    /**
     * @brief 添加顶点属性绑定
     * 
     * addAttributeBinding参数说明:
     * @param name - shader中的attribute名称 (如"vsg_Vertex")
     * @param define - 启用此属性所需的宏定义 (空字符串表示始终启用)
     * @param location - Vulkan location索引 (0-15)
     * @param format - Vulkan格式 (VK_FORMAT_XXX)
     * @param array - 默认数据数组 (用于初始化)
     * 
     * 常用format:
     * - VK_FORMAT_R32G32B32_SFLOAT: vec3 (3xfloat32)
     * - VK_FORMAT_R32G32_SFLOAT: vec2 (2xfloat32)
     * - VK_FORMAT_R32G32B32A32_SFLOAT: vec4 (4xfloat32)
     */
    // vsg_Vertex: 顶点位置 attribute，location=0
    shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    // vsg_Normal: 顶点法线 attribute，location=1
    shaderSet->addAttributeBinding("vsg_Normal", "", 1, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    // vsg_TexCoord0: 纹理坐标 attribute，location=2
    shaderSet->addAttributeBinding("vsg_TexCoord0", "", 2, VK_FORMAT_R32G32_SFLOAT, vsg::vec2Array::create(1));
    // vsg_Color: 顶点颜色 attribute，location=3
    shaderSet->addAttributeBinding("vsg_Color", "", 3, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Array::create(1));
    // vsg_InstanceID: 实例ID attribute，location=4
    shaderSet->addAttributeBinding("vsg_InstanceID", "", 4, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Array::create(1));

    // vsg_position: 实例位置 (当定义VSG_INSTANCE_POSITIONS时启用)
    shaderSet->addAttributeBinding("vsg_position", "VSG_INSTANCE_POSITIONS", 4, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    // vsg_position_scaleDistance: 广告牌位置和缩放 (当定义VSG_BILLBOARD时启用)
    shaderSet->addAttributeBinding("vsg_position_scaleDistance", "VSG_BILLBOARD", 4, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Array::create(1));

    /**
     * @brief 添加描述符绑定 (用于纹理和缓冲区)
     * 
     * addDescriptorBinding参数说明:
     * @param name - shader中的uniform名称 (如"diffuseMap")
     * @param define - 启用此描述符所需的宏定义 (空字符串表示始终启用)
     * @param descriptorSet - 描述符集编号 (1=VIEW, 2=MATERIAL)
     * @param binding - 描述符绑定点编号
     * @param descriptorType - Vulkan描述符类型
     * @param arraySize - 数组元素数量
     * @param shaderStage - 使用的着色器阶段 (VERTEX/FRAGMENT)
     * @param data - 默认数据对象
     * 
     * 常用descriptorType:
     * - VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: 采样器纹理
     * - VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: 统一缓冲区
     * - VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: 存储缓冲区
     * 
     * 常用shaderStage:
     * - VK_SHADER_STAGE_VERTEX_BIT: 顶点着色器
     * - VK_SHADER_STAGE_FRAGMENT_BIT: 片元着色器
     * - VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT: 两者都用
     */

    // === MATERIAL_DESCRIPTOR_SET (set=2) 材质相关描述符 ===
    
    // displacementMap: 位移贴图 (顶点着色器使用)
    shaderSet->addDescriptorBinding("displacementMap", "VSG_DISPLACEMENT_MAP", MATERIAL_DESCRIPTOR_SET, 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT, vsg::floatArray2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}));
    // diffuseMap: 漫反射贴图 (片元着色器使用)
    shaderSet->addDescriptorBinding("diffuseMap", "VSG_DIFFUSE_MAP", MATERIAL_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
    // mrMap: 金属度/粗糙度贴图
    shaderSet->addDescriptorBinding("mrMap", "VSG_METALLROUGHNESS_MAP", MATERIAL_DESCRIPTOR_SET, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec2Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32G32_SFLOAT}));
    // normalMap: 法线贴图
    shaderSet->addDescriptorBinding("normalMap", "VSG_NORMAL_MAP", MATERIAL_DESCRIPTOR_SET, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec3Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32G32B32_SFLOAT}));
    // aoMap: 环境光遮蔽贴图 (使用LIGHTMAP_MAP作为define)
    shaderSet->addDescriptorBinding("aoMap", "VSG_LIGHTMAP_MAP", MATERIAL_DESCRIPTOR_SET, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}));
    // emissiveMap: 自发光贴图
    shaderSet->addDescriptorBinding("emissiveMap", "VSG_EMISSIVE_MAP", MATERIAL_DESCRIPTOR_SET, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
    // specularMap: 高光贴图
    shaderSet->addDescriptorBinding("specularMap", "VSG_SPECULAR_MAP", MATERIAL_DESCRIPTOR_SET, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
    // cameraImage: 相机捕获的图像
    shaderSet->addDescriptorBinding("cameraImage", "", MATERIAL_DESCRIPTOR_SET, 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
    // depthImage: 深度图像
    shaderSet->addDescriptorBinding("depthImage", "", MATERIAL_DESCRIPTOR_SET, 8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R16_UNORM}));
    // instanceModelMatrix: 实例模型矩阵 (存储缓冲区，用于GPU实例化)
    shaderSet->addDescriptorBinding("instanceModelMatrix", "", MATERIAL_DESCRIPTOR_SET, 11, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, vsg::floatArray::create());
    // ConstantBuffer: 常量缓冲区
    shaderSet->addDescriptorBinding("ConstantBuffer", "", MATERIAL_DESCRIPTOR_SET, 12, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray::create());
    // shadowsampler: 阴影采样器
    shaderSet->addDescriptorBinding("shadowsampler", "", MATERIAL_DESCRIPTOR_SET, 13, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec3Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
    // materialArray: 材质数组 (PBR材质数据)
    shaderSet->addDescriptorBinding("materialArray", "", MATERIAL_DESCRIPTOR_SET, 14, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::PbrMaterialArray::create());

    // === VIEW_DESCRIPTOR_SET (set=1) 视图相关描述符 ===
    
    // lightData: 光源数据 (统一缓冲区，64个vec4)
    shaderSet->addDescriptorBinding("lightData", "", VIEW_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array::create(64));
    // viewportData: 视口数据 (x, y, width, height)
    shaderSet->addDescriptorBinding("viewportData", "", VIEW_DESCRIPTOR_SET, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Value::create(0,0, 1280, 1024));
    // shadowMaps: 阴影贴图 (3D纹理数组)
    shaderSet->addDescriptorBinding("shadowMaps", "", VIEW_DESCRIPTOR_SET, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray3D::create(1, 1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}));
    // shadowMapsSampler: 阴影贴图采样器
    shaderSet->addDescriptorBinding("shadowMapsSampler", "", VIEW_DESCRIPTOR_SET, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray3D::create(1, 1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}));

    // optionalDefines: 可选宏定义列表
    // 当shader中定义了这些宏时，对应的功能才会启用
    shaderSet->optionalDefines = {"VSG_GREYSCALE_DIFFUSE_MAP", "VSG_TWO_SIDED_LIGHTING", "VSG_WORKFLOW_SPECGLOSS"};

    /**
     * @brief 添加推送常量范围
     * 
     * addPushConstantRange参数说明:
     * @param name - 推送常量名称 (用于标识)
     * @param define - 启用所需的宏定义
     * @param shaderStage - 使用的着色器阶段
     * @param offset - 起始偏移量 (字节)
     * @param size - 数据大小 (字节)
     * 
     * 推送常量: 通过命令缓冲区直接传递给shader的小型数据，
     * 比uniform缓冲区更快，适合频繁变化的数据
     */
    // pc: 推送常量，256字节 (顶点着色器使用)
    shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 256);

    /**
     * @brief 添加定义数组状态
     * 
     * DefinesArrayState用于根据不同的宏定义组合选择不同的数组状态
     * 参数: std::vector<std::string> - 宏定义组合
     *       vsg::ArrayState::ptr - 对应的数组状态对象
     */
    // 实例位置+位移贴图组合
    shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_INSTANCE_POSITIONS", "VSG_DISPLACEMENT_MAP"}, vsg::PositionAndDisplacementMapArrayState::create()});
    // 仅实例位置
    shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_INSTANCE_POSITIONS"}, vsg::PositionArrayState::create()});
    // 仅位移贴图
    shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_DISPLACEMENT_MAP"}, vsg::DisplacementMapArrayState::create()});
    // 广告牌模式
    shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_BILLBOARD"}, vsg::BillboardArrayState::create()});

    /**
     * @brief 添加视图依赖状态绑定
     * 
     * ViewDependentStateBinding用于管理每个视图的描述符集数据
     * 参数: descriptorSet - 描述符集编号
     */
    shaderSet->customDescriptorSetBindings.push_back(vsg::ViewDependentStateBinding::create(VIEW_DESCRIPTOR_SET));

    // 创建颜色混合状态并添加到默认图形管线状态
    auto colorBlendState = vsg::ColorBlendState::create();
    // 设置4个颜色附件 (RGBA)
    colorBlendState->attachments.resize(4, colorBlendState->attachments[0]); 
    shaderSet->defaultGraphicsPipelineStates.push_back(colorBlendState);

    return shaderSet;
}

/**
 * @brief 构建线条渲染使用的ShaderSet
 * 
 * @param vert 顶点着色器文件路径
 * @param frag 片元着色器文件路径
 * @return vsg::ref_ptr<vsg::ShaderSet> 配置好的ShaderSet指针
 * 
 * 与buildShadowShader的区别:
 * - 推送常量大小: 128字节 (vs 256字节) - 线条不需要那么多数据
 * - 使用material uniform代替materialArray - 线条使用单一材质
 * - 不包含shadowsampler - 线条不参与阴影计算
 */
vsg::ref_ptr<vsg::ShaderSet> ConfigShader::buildLineShader(std::string vert, std::string frag)
{
    // 创建Options对象用于配置着色器加载选项
    auto options = vsg::Options::create();
    // 读取顶点着色器文件
    auto vertexShader = vsg::read_cast<vsg::ShaderStage>(vert, options);
    // 读取片元着色器文件
    auto fragmentShader = vsg::read_cast<vsg::ShaderStage>(frag, options);

    // 创建ShaderSet并传入着色器阶段
    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});

    // 定义描述符集编号
    // VIEW_DESCRIPTOR_SET = 1: 用于视图相关数据
    // MATERIAL_DESCRIPTOR_SET = 2: 用于材质相关数据
    #define VIEW_DESCRIPTOR_SET 1
    #define MATERIAL_DESCRIPTOR_SET 2

    /**
     * @brief 添加顶点属性绑定
     * 
     * addAttributeBinding参数说明:
     * @param name - shader中的attribute名称
     * @param define - 启用此属性所需的宏定义 (空字符串表示始终启用)
     * @param location - Vulkan location索引 (0-15)
     * @param format - Vulkan格式 (VK_FORMAT_XXX)
     * @param array - 默认数据数组
     * 
     * 线条shader的attribute:
     * - vsg_Vertex: 顶点位置
     * - vsg_Normal: 顶点法线
     * - vsg_TexCoord0: 纹理坐标
     * - vsg_Color: 顶点颜色
     * - vsg_position: 实例位置 (当定义VSG_INSTANCE_POSITIONS时启用)
     * - vsg_position_scaleDistance: 广告牌 (当定义VSG_BILLBOARD时启用)
     */
    // vsg_Vertex: 顶点位置 attribute，location=0
    shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    // vsg_Normal: 顶点法线 attribute，location=1
    shaderSet->addAttributeBinding("vsg_Normal", "", 1, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    // vsg_TexCoord0: 纹理坐标 attribute，location=2
    shaderSet->addAttributeBinding("vsg_TexCoord0", "", 2, VK_FORMAT_R32G32_SFLOAT, vsg::vec2Array::create(1));
    // vsg_Color: 顶点颜色 attribute，location=3
    shaderSet->addAttributeBinding("vsg_Color", "", 3, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Array::create(1));

    // vsg_position: 实例位置 (当定义VSG_INSTANCE_POSITIONS时启用)
    shaderSet->addAttributeBinding("vsg_position", "VSG_INSTANCE_POSITIONS", 4, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));
    // vsg_position_scaleDistance: 广告牌位置和缩放 (当定义VSG_BILLBOARD时启用)
    shaderSet->addAttributeBinding("vsg_position_scaleDistance", "VSG_BILLBOARD", 4, VK_FORMAT_R32G32B32A32_SFLOAT, vsg::vec4Array::create(1));

    /**
     * @brief 添加描述符绑定 (用于纹理和缓冲区)
     * 
     * addDescriptorBinding参数说明:
     * @param name - shader中的uniform名称
     * @param define - 启用此描述符所需的宏定义
     * @param descriptorSet - 描述符集编号 (1=VIEW, 2=MATERIAL)
     * @param binding - 描述符绑定点编号
     * @param descriptorType - Vulkan描述符类型
     * @param arraySize - 数组元素数量
     * @param shaderStage - 使用的着色器阶段
     * @param data - 默认数据对象
     * 
     * 与buildShadowShader的区别:
     * - binding=10: 使用material uniform代替materialArray (PbrMaterialValue vs PbrMaterialArray)
     * - 不包含: instanceModelMatrix(binding=11), shadowsampler(binding=13)
     */

    // === MATERIAL_DESCRIPTOR_SET (set=2) 材质相关描述符 ===
    
    // displacementMap: 位移贴图 (顶点着色器使用)
    shaderSet->addDescriptorBinding("displacementMap", "VSG_DISPLACEMENT_MAP", MATERIAL_DESCRIPTOR_SET, 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT, vsg::floatArray2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}));
    // diffuseMap: 漫反射贴图 (片元着色器使用)
    shaderSet->addDescriptorBinding("diffuseMap", "VSG_DIFFUSE_MAP", MATERIAL_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
    // mrMap: 金属度/粗糙度贴图
    shaderSet->addDescriptorBinding("mrMap", "VSG_METALLROUGHNESS_MAP", MATERIAL_DESCRIPTOR_SET, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec2Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32G32_SFLOAT}));
    // normalMap: 法线贴图
    shaderSet->addDescriptorBinding("normalMap", "VSG_NORMAL_MAP", MATERIAL_DESCRIPTOR_SET, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec3Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32G32B32_SFLOAT}));
    // aoMap: 环境光遮蔽贴图
    shaderSet->addDescriptorBinding("aoMap", "VSG_LIGHTMAP_MAP", MATERIAL_DESCRIPTOR_SET, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}));
    // emissiveMap: 自发光贴图
    shaderSet->addDescriptorBinding("emissiveMap", "VSG_EMISSIVE_MAP", MATERIAL_DESCRIPTOR_SET, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
    // specularMap: 高光贴图
    shaderSet->addDescriptorBinding("specularMap", "VSG_SPECULAR_MAP", MATERIAL_DESCRIPTOR_SET, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
    // cameraImage: 相机捕获的图像
    shaderSet->addDescriptorBinding("cameraImage", "", MATERIAL_DESCRIPTOR_SET, 7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R8G8B8A8_UNORM}));
    // depthImage: 深度图像
    shaderSet->addDescriptorBinding("depthImage", "", MATERIAL_DESCRIPTOR_SET, 8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_R16_UNORM}));
    // material: 材质uniform (单一PBR材质 vs 材质数组)
    // 注意: 这里使用PbrMaterialValue而不是PbrMaterialArray，适合单一线条材质
    shaderSet->addDescriptorBinding("material", "", MATERIAL_DESCRIPTOR_SET, 10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::PbrMaterialValue::create());
    // ConstantBuffer: 常量缓冲区
    shaderSet->addDescriptorBinding("ConstantBuffer", "", MATERIAL_DESCRIPTOR_SET, 12, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray::create());

    // === VIEW_DESCRIPTOR_SET (set=1) 视图相关描述符 ===
    
    // lightData: 光源数据 (统一缓冲区)
    shaderSet->addDescriptorBinding("lightData", "", VIEW_DESCRIPTOR_SET, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Array::create(64));
    // viewportData: 视口数据
    shaderSet->addDescriptorBinding("viewportData", "", VIEW_DESCRIPTOR_SET, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, vsg::vec4Value::create(0,0, 1280, 1024));
    // shadowMaps: 阴影贴图
    shaderSet->addDescriptorBinding("shadowMaps", "", VIEW_DESCRIPTOR_SET, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray3D::create(1, 1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}));
    // shadowMapsSampler: 阴影贴图采样器
    shaderSet->addDescriptorBinding("shadowMapsSampler", "", VIEW_DESCRIPTOR_SET, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::floatArray3D::create(1, 1, 1, vsg::Data::Properties{VK_FORMAT_R32_SFLOAT}));

    // optionalDefines: 可选宏定义列表
    shaderSet->optionalDefines = {"VSG_GREYSCALE_DIFFUSE_MAP", "VSG_TWO_SIDED_LIGHTING", "VSG_WORKFLOW_SPECGLOSS"};

    /**
     * @brief 添加推送常量范围
     * 
     * 与buildShadowShader的区别:
     * - size = 128字节 (vs 256字节)
     * - 线条渲染不需要那么多推送常量数据
     */
    shaderSet->addPushConstantRange("pc", "", VK_SHADER_STAGE_VERTEX_BIT, 0, 128);

    // 定义数组状态 (与buildShadowShader相同)
    shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_INSTANCE_POSITIONS", "VSG_DISPLACEMENT_MAP"}, vsg::PositionAndDisplacementMapArrayState::create()});
    shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_INSTANCE_POSITIONS"}, vsg::PositionArrayState::create()});
    shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_DISPLACEMENT_MAP"}, vsg::DisplacementMapArrayState::create()});
    shaderSet->definesArrayStates.push_back(vsg::DefinesArrayState{{"VSG_BILLBOARD"}, vsg::BillboardArrayState::create()});

    // 视图依赖状态绑定
    shaderSet->customDescriptorSetBindings.push_back(vsg::ViewDependentStateBinding::create(VIEW_DESCRIPTOR_SET));

    // 注意: buildLineShader没有添加ColorBlendState
    // 线条渲染通常不需要颜色混合 (不透明渲染)

    return shaderSet;
}