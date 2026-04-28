#include "SSAOPass.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Subpass 1: SSAO 生成（generate）
////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * customSSAOShaderSet — 创建 SSAO 生成阶段的 ShaderSet
 *
 * ShaderSet 封装了 vertex/fragment shader 以及它们需要的 attribute（顶点属性）和 descriptor（描述符）绑定。
 * SSAO 生成阶段需要读取 G-Buffer 的三个附件（color、normal、worldPos）以及噪声纹理。
 *
 * @param options              VSG 选项（包含 shader 搜索路径等）
 * @return                     配置好的 ShaderSet，用于后续 GraphicsPipelineConfigurator
 */
vsg::ref_ptr<vsg::ShaderSet> SSAOPass::customSSAOShaderSet(vsg::ref_ptr<const vsg::Options> options)
{
    vsg::info("Local pbr_ShaderSet(", options, ")");

    // 加载 SSAO 生成的 vertex/fragment shader 文件
    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/ssao.vert", options->paths);
    auto fragShaderFilepath = vsg::findFile("shaders/IBL/ssao.frag", options->paths);
    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexShaderFilepath);
    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderFilepath);

    if (!vertexShader || !fragmentShader)
    {
        vsg::error("ssao_ShaderSet(...) could not find shaders.");
        return {};
    }

#define MATERIAL_DESCRIPTOR_SET 2  // descriptor set 索引 = 2，专门用于材质/后处理相关资源

    // 创建 ShaderSet，将 vertex + fragment shader 组合在一起
    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});

    // 顶点属性绑定：全屏四边形的顶点位置（location 0 in GLSL）
    shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));

    shaderSet->addDescriptorBinding(
        "colorInputAttachment",          // 名称（需和 GLSL 中一致）
        "",                               // 无预编译宏（必启用，因为 subpass 1 必须读）
        MATERIAL_DESCRIPTOR_SET,  // 输入附件专属的 descriptor set
        0,             // binding 索引（和 GLSL 中 input_attachment_index 对应）
        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,  // 类型必须是输入附件！
        1,                                // 数组大小（1 个）
        VK_SHADER_STAGE_FRAGMENT_BIT,     // 仅片段着色器读取
        vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_B8G8R8A8_UNORM})
    );
    shaderSet->addDescriptorBinding(
        "normalInputAttachment",          // 名称（需和 GLSL 中一致）
        "",                               // 无预编译宏（必启用，因为 subpass 1 必须读）
        MATERIAL_DESCRIPTOR_SET,  // 输入附件专属的 descriptor set
        1,             // binding 索引（和 GLSL 中 input_attachment_index 对应）
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,  // 类型必须是输入附件！
        1,                                // 数组大小（1 个）
        VK_SHADER_STAGE_FRAGMENT_BIT,     // 仅片段着色器读取
        vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_B8G8R8A8_UNORM})
    );
    shaderSet->addDescriptorBinding(
        "worldPosInputAttachment",        // 名称（需和 GLSL 中一致）
        "",                               // 无预编译宏
        MATERIAL_DESCRIPTOR_SET,  // 同一 input set
        2,           // binding 索引
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        1,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_B8G8R8A8_UNORM})
    );
    // 噪声纹理（noise texture）：用于随机化 SSAO 采样方向，减少 banding（条带伪影）
    // 使用 Combined Image Sampler 类型，因为需要通过采样器做纹理过滤
    shaderSet->addDescriptorBinding("samplerNoise", "", MATERIAL_DESCRIPTOR_SET, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_B8G8R8A8_UNORM}));

    return shaderSet;
}

/**
 * buildSSAOData — 构建 SSAO 生成阶段的渲染数据
 *
 * 主要工作：创建全屏四边形、生成随机噪声纹理、绑定 G-Buffer 附件，然后将整个渲染命令添加到 scene graph。
 * 在 subpass 1 中执行，读取 G-Buffer 并输出 SSAO 结果到临时纹理。
 *
 * @param options              VSG 选项（包含 shader 搜索路径）
 * @param scene                 场景图根节点（SSAO 渲染命令挂载到此处）
 * @param GBufferView0         G-Buffer 颜色附件（color）
 * @param GBufferView1         G-Buffer 法线附件（normal）
 * @param GBufferView2         G-Buffer 世界坐标附件（worldPos）
 * @param extent               渲染分辨率（用于创建噪声纹理）
 */
void SSAOPass::buildSSAOData(vsg::ref_ptr<vsg::Options> options, vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::ImageView> GBufferView0, vsg::ref_ptr<vsg::ImageView> GBufferView1, vsg::ref_ptr<vsg::ImageView> GBufferView2, VkExtent2D extent){
    vsg::ref_ptr<vsg::ShaderSet> model_shaderset = SSAOPass::customSSAOShaderSet(options);

    // 关闭背面剔除（cullMode = NONE），因为全屏四边形需要正反面都渲染
    auto rasterizationState = vsg::RasterizationState::create();
    rasterizationState->cullMode = VK_CULL_MODE_NONE;
    model_shaderset->defaultGraphicsPipelineStates.push_back(rasterizationState);

    // GraphicsPipelineConfigurator 是 VSG 提供的 pipeline 配置助手
    // 它根据 ShaderSet 自动推导出 pipeline layout、descriptor set layout 等信息
    auto graphicsPipelineConfig = vsg::GraphicsPipelineConfigurator::create(model_shaderset);

    // 指定该 pipeline 在 subpass 1 中执行（subpass 0 通常是 G-Buffer 填充）
    graphicsPipelineConfig->subpass = 1;

    // ---- 生成随机噪声纹理（Random Noise Texture）----
    // 噪声纹理用于随机旋转 SSAO 采样核（sampling kernel），消除规则性伪影
    unsigned int seed = 100;
    std::mt19937 generator(seed); // 梅森旋转算法（Mersenne Twister），伪随机数生成器，周期长、分布均匀
    std::uniform_real_distribution<float> distribution(0, 1.0f); // 均匀分布 [0, 1)

    // 创建与屏幕同尺寸的 2D 纹理数据容器
    // ubvec4Array2D = unsigned byte vec4 2D array，即 RGBA8 格式
    auto samplerNoiseData = vsg::ubvec4Array2D::create(extent.width, extent.height);
    for (uint32_t i = 0; i < extent.width; ++i){
        for (uint32_t j = 0; j < extent.height; ++j){
            float randX = distribution(generator); // 随机 x 方向偏移（归一化到 [0, 1]）
            float randY = distribution(generator); // 随机 y 方向偏移（归一化到 [0, 1]）
            // 将 [0, 1] 浮点数映射到 [0, 255] 整数，存入 RG 通道，BA 通道未使用
            samplerNoiseData->set(i, j, vsg::ubvec4(static_cast<uint8_t>(randX * 255.0f),
                    static_cast<uint8_t>(randY * 255.0f), static_cast<uint8_t>(0), static_cast<uint8_t>(0)));
        }
    }
    samplerNoiseData->properties.format = VK_FORMAT_R8G8B8A8_UNORM; // 纹理格式：8位每通道，无符号归一化
    samplerNoiseData->properties.dataVariance = vsg::DYNAMIC_DATA;  // 标记为动态数据，允许运行时修改

    // 创建 nearest（最近邻）采样器，不做插值，保持噪声的随机性
    auto noiseSampler = Utils::createNearestSampler();
    
    // 将噪声纹理绑定到 ShaderSet 中名为 "samplerNoise" 的 descriptor binding
    // assignTexture 会自动匹配 ShaderSet 中注册的 descriptor binding 并创建对应的 descriptor write
    vsg::ImageInfoList noiseImageInfoList = {vsg::ImageInfo::create(noiseSampler, samplerNoiseData)};
    graphicsPipelineConfig->assignTexture("samplerNoise", noiseImageInfoList);

    // 绑定 G-Buffer 的三个附件（attachment）到对应的 descriptor
    // 注意：虽然 descriptor 类型不同（INPUT_ATTACHMENT vs COMBINED_IMAGE_SAMPLER），
    // 但 VSG 内部会根据 ShaderSet 中注册的类型自动处理
    vsg::ref_ptr<vsg::Sampler> in_sampler{nullptr};
    // GBuffer0 = color（颜色），布局为 READ_ONLY_OPTIMAL 表示已经可以被读取
    vsg::ImageInfoList GBufferViewList0 = {vsg::ImageInfo::create(noiseSampler, GBufferView0, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)};
    // GBuffer1 = normal（法线），用于计算 AO 方向
    vsg::ImageInfoList GBufferViewList1 = {vsg::ImageInfo::create(noiseSampler, GBufferView1, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)};
    // GBuffer2 = worldPos（世界坐标），用于计算采样点之间的距离
    vsg::ImageInfoList GBufferViewList2 = {vsg::ImageInfo::create(noiseSampler, GBufferView2, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)};
    graphicsPipelineConfig->assignTexture("colorInputAttachment", GBufferViewList0);
    graphicsPipelineConfig->assignTexture("normalInputAttachment", GBufferViewList1);
    graphicsPipelineConfig->assignTexture("worldPosInputAttachment", GBufferViewList2);

    // ---- 创建全屏四边形（Fullscreen Quad）----
    // 四个顶点覆盖 NDC（标准化设备坐标）空间的 [-1, 1]，z=1.0 表示最远深度
    // 这样 vertex shader 输出的 quad 会覆盖整个屏幕，让每个像素都执行 SSAO fragment shader
    auto vertices = vsg::vec3Array::create({
        vsg::vec3(-1, -1, 1.0f),
        vsg::vec3(1, -1, 1.0f),
        vsg::vec3(1, 1, 1.0f),
        vsg::vec3(-1, 1, 1.0f)
        });

    // 索引数组：定义两个三角形组成四边形（0-1-2 和 2-3-0）
    auto indices = vsg::ushortArray::create(
        {0, 1, 2,
        2, 3, 0});

    // 将顶点数组绑定到 "vsg_Vertex" 属性（VSG 会自动关联到 vertex shader 的 location 0）
    vsg::DataList vertexArrays;
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, vertices);

    // 组装绘制命令：
    // BindVertexBuffers = 绑定顶点缓冲区（告诉 GPU 去哪里读顶点数据）
    // BindIndexBuffer   = 绑定索引缓冲区（告诉 GPU 用什么顺序绘制三角形）
    auto drawCommands = vsg::Commands::create();
    drawCommands->addChild(vsg::BindVertexBuffers::create(graphicsPipelineConfig->baseAttributeBinding, vertexArrays));
    drawCommands->addChild(vsg::BindIndexBuffer::create(indices));

    // 使用 DrawIndexedIndirect 进行间接绘制（indirect draw）
    // 好处：绘制参数（indexCount、instanceCount 等）存在 GPU buffer 中，CPU 不需要逐帧更新
    VkDrawIndexedIndirectCommand cmd = {
        6,      // indexCount：绘制 6 个索引（2 个三角形，每个 3 个顶点）
        1,     // instanceCount：实例数为 1（非 instanced rendering）
        0,         // firstIndex：从索引数组的第 0 个开始
        0,         // vertexOffset：顶点偏移为 0
        0          // firstInstance：第一个实例索引为 0
    };

    // 将绘制命令打包到 indirect buffer 中，交给 GPU 执行
    auto indirectBuffer = vsg::Array<VkDrawIndexedIndirectCommand>::create(1);
    indirectBuffer->set(0, cmd);
    auto draw_indirect = vsg::DrawIndexedIndirect::create(
        indirectBuffer,  // 间接命令缓冲区（存储 VkDrawIndexedIndirectCommand）
        1,              // 命令数量（本例只有 1 条绘制命令）
        sizeof(VkDrawIndexedIndirectCommand) // 每条命令的字节步长
    );
    drawCommands->addChild(draw_indirect);

    // init() 根据 ShaderSet 和已配置的 binding 创建真正的 Vulkan pipeline
    // 必须在所有 descriptor/attribute 绑定完成后调用
    graphicsPipelineConfig->init();

    // StateGroup 是 VSG scene graph 中管理 pipeline 状态的节点
    // 它在子节点绘制前设置 pipeline 状态，绘制后恢复
    // copyTo 将 pipeline 配置（shader、descriptor sets、push constants 等）写入 StateGroup
    auto stateGroup = vsg::StateGroup::create();
    graphicsPipelineConfig->copyTo(stateGroup);
    stateGroup->addChild(drawCommands);  // 添加绘制命令作为子节点
    scene->addChild(stateGroup);         // 将整个状态组添加到 scene graph
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Subpass 2: SSAO 去噪（denoise）
////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * customSSAODenoiseShaderSet — 创建 SSAO 去噪阶段的 ShaderSet
 *
 * 去噪阶段将原始 SSAO 结果与 shadow factor 合并，输出平滑的环境光遮蔽系数。
 * 需要读取 G-Buffer color、Shadow write（阴影因子）、SSAO result 三个附件。
 *
 * @param options              VSG 选项（包含 shader 搜索路径等）
 * @return                     配置好的 ShaderSet，用于后续 GraphicsPipelineConfigurator
 */
vsg::ref_ptr<vsg::ShaderSet> SSAOPass::customSSAODenoiseShaderSet(vsg::ref_ptr<const vsg::Options> options)
{
    vsg::info("Local pbr_ShaderSet(", options, ")");

    // 加载去噪 shader 文件
    auto vertexShaderFilepath = vsg::findFile("shaders/IBL/ssao_denoise.vert", options->paths);
    auto fragShaderFilepath = vsg::findFile("shaders/IBL/ssao_denoise.frag", options->paths);
    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexShaderFilepath);
    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragShaderFilepath);

    if (!vertexShader || !fragmentShader)
    {
        vsg::error("ssao_ShaderSet(...) could not find shaders.");
        return {};
    }

#define MATERIAL_DESCRIPTOR_SET 2

    auto shaderSet = vsg::ShaderSet::create(vsg::ShaderStages{vertexShader, fragmentShader});

    // 顶点属性绑定：全屏四边形
    shaderSet->addAttributeBinding("vsg_Vertex", "", 0, VK_FORMAT_R32G32B32_SFLOAT, vsg::vec3Array::create(1));

    // G-Buffer color 附件（input attachment）：从上一个 subpass 直接读取，不需要采样器
    // Input Attachment 的特性：在同一 render pass 内，可以直接读取同一像素位置的值
    shaderSet->addDescriptorBinding(
        "colorInputAttachment",          // 名称（需和 GLSL 中一致）
        "",                               // 无预编译宏（必启用，因为 subpass 2 必须读）
        MATERIAL_DESCRIPTOR_SET,  // 输入附件专属的 descriptor set
        0,             // binding 索引（和 GLSL 中 input_attachment_index 对应）
        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,  // 类型：输入附件（subpass 内直接读取）
        1,                                // 数组大小（1 个）
        VK_SHADER_STAGE_FRAGMENT_BIT,     // 仅片段着色器读取
        vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_B8G8R8A8_UNORM})
    );

    // Shadow 写入附件（input attachment）：从 shadow pass 输出的阴影因子
    shaderSet->addDescriptorBinding(
        "shadowInputAttachment",          // 名称（需和 GLSL 中一致）
        "",                               // 无预编译宏（必启用，因为 subpass 2 必须读）
        MATERIAL_DESCRIPTOR_SET,  // 输入附件专属的 descriptor set
        1,             // binding 索引（和 GLSL 中 input_attachment_index 对应）
        VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,  // 类型：输入附件（subpass 内直接读取）
        1,                                // 数组大小（1 个）
        VK_SHADER_STAGE_FRAGMENT_BIT,     // 仅片段着色器读取
        vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_B8G8R8A8_UNORM})
    );

    // SSAO 结果纹理（combined image sampler）：从 subpass 1 的输出读取
    // 与 input attachment 不同，这里使用采样器，可以做纹理过滤
    shaderSet->addDescriptorBinding("samplerSSAO", "", MATERIAL_DESCRIPTOR_SET, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, vsg::ubvec4Array2D::create(1, 1, vsg::Data::Properties{VK_FORMAT_B8G8R8A8_UNORM}));
    
    
    // 颜色混合状态（Color Blend State）：控制 SSAO 去噪结果如何写入 color attachment
    // 这里使用经典的 alpha blending 公式：result = src.rgb * src.a + dst.rgb * (1 - src.a)
    auto colorBlendState = vsg::ColorBlendState::create();
    colorBlendState->attachments[0] = {
        VK_FALSE,                                      // blendEnable：虽然设置了因子，但设为 VK_FALSE 表示不启用混合（由 shader 内部处理混合逻辑）
        VK_BLEND_FACTOR_SRC_ALPHA,                    // srcColorBlendFactor：源颜色因子，取当前片元的 Alpha 值
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,          // dstColorBlendFactor：目标颜色因子，1 - 源 Alpha（经典半透公式）
        VK_BLEND_OP_ADD,                              // colorBlendOp：颜色混合操作，src*srcAlpha + dst*(1-srcAlpha)
        VK_BLEND_FACTOR_ONE,                          // srcAlphaBlendFactor：源 Alpha 因子
        VK_BLEND_FACTOR_ZERO,                         // dstAlphaBlendFactor：目标 Alpha 因子
        VK_BLEND_OP_ADD,                              // alphaBlendOp：Alpha 混合操作
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT  // 写入所有颜色通道
    };
    // 为第二个 color attachment 复制相同的混合配置（确保所有输出附件行为一致）
    colorBlendState->attachments.resize(2, colorBlendState->attachments[0]);
    shaderSet->defaultGraphicsPipelineStates.push_back(colorBlendState);

    return shaderSet;
}

/**
 * buildSSAODenoiseData — 构建 SSAO 去噪阶段的渲染数据
 *
 * 与 subpass 1 类似，创建全屏四边形，但绑定的附件不同：
 *   - GBuffer0 (color) + ShadowWrite (shadow) 作为 input attachment（subpass 内直接读取）
 *   - SSAO result 作为 combined image sampler（需要采样器读取纹理）
 * 在 subpass 2 中执行，将去噪后的 SSAO 结果写入最终 color attachment。
 *
 * @param options              VSG 选项（包含 shader 搜索路径）
 * @param scene                 场景图根节点（去噪渲染命令挂载到此处）
 * @param GBufferView0         G-Buffer 颜色附件（作为 input attachment 读取）
 * @param ShadowWriteView       阴影写入附件（shadow factor，作为 input attachment 读取）
 * @param SSAOResultImageView   SSAO 生成阶段的结果纹理（作为 sampler 读取）
 */
void SSAOPass::buildSSAODenoiseData(vsg::ref_ptr<vsg::Options> options, vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::ImageView> GBufferView0, vsg::ref_ptr<vsg::ImageView> ShadowWriteView, vsg::ref_ptr<vsg::ImageView> SSAOResultImageView){
    vsg::ref_ptr<vsg::ShaderSet> model_shaderset = SSAOPass::customSSAODenoiseShaderSet(options);

    // 关闭背面剔除（全屏四边形需要正反面都渲染）
    auto rasterizationState = vsg::RasterizationState::create();
    rasterizationState->cullMode = VK_CULL_MODE_NONE;
    model_shaderset->defaultGraphicsPipelineStates.push_back(rasterizationState);

    // 创建 pipeline 配置器，指定在 subpass 2 中执行
    auto graphicsPipelineConfig = vsg::GraphicsPipelineConfigurator::create(model_shaderset);
    graphicsPipelineConfig->subpass = 2;  // 去噪在 subpass 2（SSAO 生成在 subpass 1）

    auto noiseSampler = Utils::createNearestSampler();

    // 绑定三个附件到对应的 descriptor binding：
    // GBuffer0 = color（颜色附件），作为 input attachment 供 shader 读取原始颜色
    vsg::ImageInfoList GBufferViewList0 = {vsg::ImageInfo::create(noiseSampler, GBufferView0, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)};
    // ShadowWrite = shadow（阴影因子附件），作为 input attachment 供 shader 读取阴影信息
    vsg::ImageInfoList GBufferViewListShadow = {vsg::ImageInfo::create(noiseSampler, ShadowWriteView, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)};
    // SSAOResult = subpass 1 输出的 SSAO 结果，作为 combined image sampler 采样
    vsg::ImageInfoList GBufferViewList1 = {vsg::ImageInfo::create(noiseSampler, SSAOResultImageView, VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL)};
    graphicsPipelineConfig->assignTexture("colorInputAttachment", GBufferViewList0);
    graphicsPipelineConfig->assignTexture("shadowInputAttachment", GBufferViewListShadow);
    graphicsPipelineConfig->assignTexture("samplerSSAO", GBufferViewList1);

    // 创建全屏四边形（与 subpass 1 结构相同：4 个顶点，2 个三角形）
    auto vertices = vsg::vec3Array::create({
        vsg::vec3(-1, -1, 1.0f),
        vsg::vec3(1, -1, 1.0f),
        vsg::vec3(1, 1, 1.0f),
        vsg::vec3(-1, 1, 1.0f)
        });

    auto indices = vsg::ushortArray::create(
        {0, 1, 2,
        2, 3, 0});

    vsg::DataList vertexArrays;
    graphicsPipelineConfig->assignArray(vertexArrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX, vertices);

    // 组装绘制命令（与 subpass 1 相同的模式）
    auto drawCommands = vsg::Commands::create();
    drawCommands->addChild(vsg::BindVertexBuffers::create(graphicsPipelineConfig->baseAttributeBinding, vertexArrays));
    drawCommands->addChild(vsg::BindIndexBuffer::create(indices));

    // 间接绘制命令：6 个索引 = 2 个三角形 = 1 个全屏四边形
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

    drawCommands->addChild(draw_indirect);

    // 创建 Vulkan pipeline 并将所有配置写入 StateGroup
    // 流程与 subpass 1 完全一致：init() 创建 pipeline，copyTo 写入 StateGroup，添加到 scene graph
    graphicsPipelineConfig->init();

    auto stateGroup = vsg::StateGroup::create();
    graphicsPipelineConfig->copyTo(stateGroup);
    stateGroup->addChild(drawCommands);
    scene->addChild(stateGroup);
}