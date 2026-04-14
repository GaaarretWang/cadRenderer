/**
 * CustomViewDependentState.cpp
 *
 * 实现级联阴影贴图（CSM）的核心逻辑，包括：
 *   1. init() 初始化 Vulkan 资源：shadow map image、descriptor set、compute pipeline
 *   2. traverse() 每帧更新光源数据、计算 CSM 分割、设置 shadow camera
 *   3. Cpractical() CSM 分割方案（对数/均匀混合）
 *   4. computevertex_shadow.comp compute shader 做 GPU 端 frustum culling
 *
 * 关键 VSG 概念：
 *   - RecordTraversal：每帧的记录遍历，收集所有需要绘制的场景节点
 *   - preRenderCommandGraph：在主渲染之前执行的命令图，用于阴影贴图预渲染
 *   - Switch：VSG 的条件节点，可动态开启/关闭子节点的渲染
 *   - View：代表一个渲染视口，包含 Camera 和场景子树
 *   - RenderGraph：渲染图节点，管理 render pass 的开始和结束
 *   - descriptor set（描述符集合）：Vulkan 中绑定 uniform buffer、texture 等到 shader 的机制
 */

#include "CustomViewDependentState.h"

using namespace vsg;

/**
 * TraverseChildrenOfNode —— VSG 辅助节点
 *
 * 作用：只遍历某个 node 的 children（而非 node 本身）。
 * 在 shadow map 渲染时，用它来把 view 的子树"嫁接"到每个 shadow map 的 View 中，
 * 这样 shadow 渲染可以复用主场景的子节点，但不重复触发 view 节点自身的逻辑。
 */
namespace vsg
{
    class TraverseChildrenOfNode : public Inherit<Node, TraverseChildrenOfNode>
    {
    public:
        explicit TraverseChildrenOfNode(Node* in_node) :
            node(in_node) {}

        observer_ptr<Node> node;

        template<class N, class V>
        static void t_traverse(N& in_node, V& visitor)
        {
            if (auto ref_node = in_node.node.ref_ptr()) ref_node->traverse(visitor);
        }

        void traverse(Visitor& visitor) override { t_traverse(*this, visitor); }
        void traverse(ConstVisitor& visitor) const override { t_traverse(*this, visitor); }
        void traverse(RecordTraversal& visitor) const override { t_traverse(*this, visitor); }
    };
    VSG_type_name(TraverseChildrenOfNode);

    /**
     * Cpractical —— CSM（级联阴影贴图）的混合分割方案
     *
     * 将视锥体从近平面 n 到远平面 f 划分为 m 个级联（cascade），
     * 计算第 i 个分割点的深度值。
     *
     * 采用对数分割（Logarithmic）和均匀分割（Uniform）的线性混合：
     *   Clog     = n * (f/n)^(i/m)         —— 对数分割，近处密、远处疏，符合人眼感知
     *   Cuniform = n + (f - n) * (i/m)     —— 均匀分割，等间距
     *   result   = Clog * lambda + Cuniform * (1 - lambda)
     *
     * 参数说明：
     *   n       —— 近平面距离
     *   f       —— 远平面距离
     *   i       —— 当前级联索引（0 ~ m）
     *   m       —— 总级联数
     *   lambda  —— 混合因子（0=纯均匀，1=纯对数），通常取 0.5 左右
     *
     * 参考：https://learn.microsoft.com/en-us/windows/win32/dxtecharts/cascaded-shadow-maps
     */
    inline double Cpractical(double n, double f, double i, double m, double lambda)
    {
        double Clog = n * std::pow((f / n), (i / m));
        double Cuniform = n + (f - n) * (i / m);
        return Clog * lambda + Cuniform * (1.0 - lambda);
    };

} // namespace vsg

/**
 * createCustomShadowImage —— 创建 shadow map 用的 Vulkan Image
 *
 * 创建一个 2D image，支持 array layers（每一层对应一个 CSM 级联）。
 * VSG 概念：vsg::Image 是 Vulkan VkImage 的封装，用于描述 image 的属性，
 *           实际的 device memory 由 VSG 的 ResourceAllocator 在后续分配。
 *
 * @param width   shadow map 宽度（像素）
 * @param height  shadow map 高度（像素）
 * @param levels  array layer 数量（= 最大级联数）
 * @param format  Vulkan 像素格式，通常为 VK_FORMAT_D32_SFLOAT（32位浮点深度）
 * @param usage   image 用途标志（深度附件 + 采样）
 */
vsg::ref_ptr<vsg::Image> CustomViewDependentState::createCustomShadowImage(uint32_t width, uint32_t height, uint32_t levels, VkFormat format, VkImageUsageFlags usage){
    auto image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = format;
    image->extent = VkExtent3D{width, height, 1};
    image->mipLevels = 1;
    image->arrayLayers = levels;       // CSM 级联数 = array layer 数
    image->samples = VK_SAMPLE_COUNT_1_BIT;
    image->tiling = VK_IMAGE_TILING_OPTIMAL;
    image->usage = usage;
    image->initialLayout = VK_IMAGE_LAYOUT_GENERAL;
    image->flags = 0;
    image->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    return image;
}

/**
 * init() —— 初始化所有 shadow map 和光照相关的 Vulkan 资源
 *
 * VSG 概念：ResourceRequirements 描述了当前场景需要的资源规模（光源数量、shadow map 数量等）。
 *          VSG 的 scene graph 遍历时会收集这些信息，然后在此处据此分配 GPU 资源。
 *
 * 本函数完成以下工作：
 *   1. 创建 lightData uniform buffer（存放所有光源参数）
 *   2. 创建 shadow map depth image（2048x2048, D32_SFLOAT, 最多8层 array）
 *   3. 创建 descriptor set layout 和 descriptor set（绑定 light data + shadow map texture）
 *   4. 创建 compute pipeline 用于 shadow frustum culling
 *   5. 创建 preRenderSwitch 和 preRenderCommandGraph 用于阴影预渲染
 */
void CustomViewDependentState::init(ResourceRequirements& requirements)
{
    // 防止重复初始化（lightData 非空说明已经初始化过）
    if (lightData) return;

    uint32_t maxNumberLights = 64;  // 最大光源数
    uint32_t maxViewports = 1;      // 最大视口数（通常为1）

    uint32_t shadowWidth = 2048;    // shadow map 宽度
    uint32_t shadowHeight = 2048;   // shadow map 高度
    uint32_t maxShadowMaps = 8;     // 最大 shadow map 数（= 最大 CSM 级联数）

    // 从 ResourceRequirements 获取当前 view 检测到的光源和 shadow map 信息
    auto& viewDetails = requirements.views[view];

    // 如果 view 启用了 features，则根据场景实际光源数调整上限
    if (view->features != 0)
    {
        uint32_t numLights = static_cast<uint32_t>(viewDetails.lights.size());
        uint32_t numShadowMaps = 0;
        // 统计所有光源需要的 shadow map 数量之和
        for (auto& light : viewDetails.lights)
        {
            numShadowMaps += light->shadowMaps;
        }

        // 将实际光源数 clamp 到 [min, max] 范围
        if (numLights < requirements.numLightsRange[0])
            maxNumberLights = requirements.numLightsRange[0];
        else if (numLights > requirements.numLightsRange[1])
            maxNumberLights = requirements.numLightsRange[1];
        else
            maxNumberLights = numLights;

        if (numShadowMaps < requirements.numShadowMapsRange[0])
            maxShadowMaps = requirements.numShadowMapsRange[0];
        else if (numShadowMaps > requirements.numShadowMapsRange[1])
            maxShadowMaps = requirements.numShadowMapsRange[1];
        else
            maxShadowMaps = numShadowMaps;

        shadowWidth = requirements.shadowMapSize.x;
        shadowHeight = requirements.shadowMapSize.y;
    }
    else
    {
        maxNumberLights = 0;
        maxShadowMaps = 0;
    }

    // lightData 布局（每个元素为 vec4）：
    //   [0]      : 光源计数（ambient, directional, point, spot 各一个 float）
    //   [1..N]   : 各光源参数（directional 每盏占 3 个 vec4，point 占 2 个，spot 占 3 个）
    //   [N+1..M] : 每个 shadow map 的 texture generation matrix（4x4 矩阵，占 4 个 vec4）
    uint32_t lightDataSize = 4 + maxNumberLights * 16 + maxShadowMaps * 16;

#if 0
    if (active)
    {
        info("void ViewDependentState::init(ResourceRequirements& requirements) view = ", view, ", active = ", active);
        info("    viewDetails.indices.size() = ", viewDetails.indices.size());
        info("    viewDetails.bins.size() = ", viewDetails.bins.size());
        info("    viewDetails.lights.size() = ", viewDetails.lights.size());
        info("    maxViewports = ", maxViewports);
        info("    maxNumberLights = ", maxNumberLights);
        info("    maxShadowMaps = ", maxShadowMaps);
        info("    lightDataSize = ", lightDataSize);
        info("    shadowWidth = ", shadowWidth);
        info("    shadowHeight = ", shadowHeight);
        info("    requirements.numLightsRange = ", requirements.numLightsRange);
        info("    requirements.numShadowMapsRange = ", requirements.numShadowMapsRange);
        info("    requirements.shadowMapSize = ", requirements.shadowMapSize);
    }
#endif

    // 创建光源数据 uniform buffer
    // VSG 概念：vec4Array 是 VSG 的 float 数组类型，properties.dataVariance = DYNAMIC 表示每帧都会更新
    lightData = vec4Array::create(lightDataSize);
    lightData->properties.dataVariance = DYNAMIC_DATA_TRANSFER_AFTER_RECORD;
    lightDataBufferInfo = BufferInfo::create(lightData.get());

    // 创建视口数据 uniform buffer（用于传递 viewport 相关参数）
    viewportData = vec4Array::create(maxViewports);
    viewportData->properties.dataVariance = DYNAMIC_DATA_TRANSFER_AFTER_RECORD;
    viewportDataBufferInfo = BufferInfo::create(viewportData.get());

    // 将 lightData 和 viewportData 打包成 DescriptorBuffer，绑定到 descriptor set 的 binding 0
    // VSG 概念：DescriptorBuffer 是 uniform buffer 类型的 descriptor，用于向 shader 传递数据
    descriptor = DescriptorBuffer::create(BufferInfoList{lightDataBufferInfo, viewportDataBufferInfo}, 0); // hardwired position for now

    // 创建 shadow map 采样器
    // 两个 sampler：一个开启 hardware PCF compare，一个不开启（用于手动 PCF）
    // VSG 概念：Sampler 封装了 VkSampler，控制纹理采样的过滤、寻址模式和比较模式
    auto shadowMapSampler = Sampler::create();
    auto shadowMapSamplerNoCompare = Sampler::create();
// #define HARDWARE_PCF 1
// HARDWARE_PCF=1 时使用线性过滤 + hardware PCF（GPU 硬件自动做深度比较）
// HARDWARE_PCF=0 时使用 nearest 过滤（手动在 shader 中做 PCF 采样）
#if HARDWARE_PCF == 1
    shadowMapSampler->minFilter = VK_FILTER_LINEAR;
    shadowMapSampler->magFilter = VK_FILTER_LINEAR;
    shadowMapSampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    shadowMapSampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSampler->compareEnable = VK_TRUE;
    shadowMapSampler->compareOp = VK_COMPARE_OP_GREATER;  // 反转深度：采样深度 > 片段深度时返回 1（在阴影中）

    shadowMapSamplerNoCompare->minFilter = VK_FILTER_LINEAR;
    shadowMapSamplerNoCompare->magFilter = VK_FILTER_LINEAR;
    shadowMapSamplerNoCompare->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    shadowMapSamplerNoCompare->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSamplerNoCompare->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSamplerNoCompare->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
#else
    shadowMapSampler->minFilter = VK_FILTER_NEAREST;
    shadowMapSampler->magFilter = VK_FILTER_NEAREST;
    shadowMapSampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    shadowMapSampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSampler->compareEnable = VK_TRUE;
    shadowMapSampler->compareOp = VK_COMPARE_OP_GREATER;  // 反转深度比较

    shadowMapSamplerNoCompare->minFilter = VK_FILTER_NEAREST;
    shadowMapSamplerNoCompare->magFilter = VK_FILTER_NEAREST;
    shadowMapSamplerNoCompare->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    shadowMapSamplerNoCompare->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSamplerNoCompare->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSamplerNoCompare->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
#endif

    if (maxShadowMaps > 0)
    {
        // 创建 shadow map depth image（D32_SFLOAT 格式，2D array，每个 layer 对应一个 CSM 级联）
        shadowDepthImage = createCustomShadowImage(shadowWidth, shadowHeight, maxShadowMaps, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

        // 创建 depth image view（2D array 类型，覆盖所有 layer）
        // VSG 概念：ImageView 是 VkImageView 的封装，描述如何"看待"一个 Image 的某个子资源范围
        auto depthImageView = ImageView::create(shadowDepthImage, VK_IMAGE_ASPECT_DEPTH_BIT);
        depthImageView->viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        depthImageView->subresourceRange.baseMipLevel = 0;
        depthImageView->subresourceRange.levelCount = 1;
        depthImageView->subresourceRange.baseArrayLayer = 0;
        depthImageView->subresourceRange.layerCount = maxShadowMaps;

        // binding 2：带 hardware compare 功能的 shadow map（用于 sampler2DArrayShadow）
        auto depthImageInfo = ImageInfo::create(shadowMapSampler, depthImageView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        shadowMapImages = DescriptorImage::create(ImageInfoList{depthImageInfo}, 2);

        // binding 3：不带 compare 的 shadow map（用于手动 PCF 采样）
        auto depthImageSamplerInfo = ImageInfo::create(shadowMapSamplerNoCompare, depthImageView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        shadowMapSamplerImages = DescriptorImage::create(ImageInfoList{depthImageSamplerInfo}, 3);
    }
    else
    {
        // 无 shadow map 时的 fallback：创建 1x1 的占位 depth image
        // 保证 descriptor set layout 不变，避免 shader 编译失败
        //
        Data::Properties properties;
        properties.format = VK_FORMAT_D32_SFLOAT;
        properties.imageViewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;

        auto shadowMapData = floatArray3D::create(1, 1, 1, 0.0f, properties);
        shadowDepthImage = Image::create(shadowMapData);
        shadowDepthImage->usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        auto depthImageView = ImageView::create(shadowDepthImage, VK_IMAGE_ASPECT_DEPTH_BIT);
        depthImageView->viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        depthImageView->subresourceRange.baseMipLevel = 0;
        depthImageView->subresourceRange.levelCount = 1;
        depthImageView->subresourceRange.baseArrayLayer = 0;
        depthImageView->subresourceRange.layerCount = 1;

        auto depthImageInfo = ImageInfo::create(shadowMapSampler, depthImageView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        shadowMapImages = DescriptorImage::create(ImageInfoList{depthImageInfo}, 2);

        auto depthImageSamplerInfo = ImageInfo::create(shadowMapSamplerNoCompare, depthImageView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        shadowMapSamplerImages = DescriptorImage::create(ImageInfoList{depthImageSamplerInfo}, 3);
    }

    // 定义 descriptor set layout（描述 shader 中每个 binding 的类型和可见阶段）
    // VSG 概念：DescriptorSetLayout 定义了 descriptor set 的"结构"，告诉 Vulkan shader 需要哪些资源
    DescriptorSetLayoutBindings descriptorBindings{
        VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}, // lightData（光源数据）
        VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}, // viewportData（视口数据）
        VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},                      // shadow map（带 hardware compare）
        VkDescriptorSetLayoutBinding{3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},                      // shadow map（无 compare，手动 PCF）
    };

    descriptorSetLayout = DescriptorSetLayout::create(descriptorBindings);
    descriptorSet = DescriptorSet::create(descriptorSetLayout, Descriptors{descriptor, shadowMapImages, shadowMapSamplerImages});

    // 如果没有 shadow map，则跳过 compute pipeline 的创建
    if (maxShadowMaps == 0) return;

    {
        // 设置 compute command graph 的提交顺序为 -1（在主渲染命令之前执行）
        // VSG 概念：submitOrder 控制 CommandGraph 的提交顺序，负数表示先于主渲染
        computeCommandGraphShadow->submitOrder = -1;

        // 定义 compute shader 的 descriptor set layout（8 个 storage buffer binding）
        // binding 0-5: draw indirect buffer, full buffer, instance buffer, highlight buffer,
        //              output instance buffer, global model matrix buffer
        // binding 6:   上一帧的全局模型矩阵（用于增量更新）
        // binding 7:   上一帧的 instance 数据（用于增量更新）
        vsg::DescriptorSetLayoutBindings descriptorBindings{
            {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        };
        auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);
        auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, vsg::PushConstantRanges{});
        {
            // 加载 compute shader 并创建 compute pipeline
            // VSG 概念：ComputePipeline 封装了 VkComputePipeline，绑定一个 compute shader stage
            // computevertex_shadow.comp 负责 GPU 端的 frustum culling：判断哪些 instance 在 shadow frustum 内
            auto shaderPath = vsg::findFile("shaders/computevertex_shadow.comp", options->paths);
            auto computeShader = vsg::read_cast<vsg::ShaderStage>(shaderPath, options);
            auto pipeline = vsg::ComputePipeline::create(pipelineLayout, computeShader);
            auto bindPipeline = vsg::BindComputePipeline::create(pipeline);
            computeCommandGraphShadow->addChild(bindPipeline);

            // 创建 pipeline barrier，确保 compute 完成后 indirect draw 命令和 instance 数据可见
            // VSG 概念：PipelineBarrier 是 Vulkan pipeline barrier 的封装，
            //           用于同步不同阶段之间的内存访问
            auto ShadowPipelineBarrier = vsg::PipelineBarrier::create(
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,  // dstStageMask
                0
            );
            // 为每个 proto（原型几何体）创建 descriptor set 并 dispatch compute shader
            // VSG 概念：CADMesh::proto_id_to_data_map 存储了所有原型几何体的数据，
            //          每个 proto 有自己的 instance buffer 和 indirect draw buffer
            for(auto& proto_data_itr : CADMesh::proto_id_to_data_map){
                ProtoData* proto_data = proto_data_itr.second;
                auto storageBuffer = vsg::DescriptorBuffer::create(vsg::BufferInfoList{proto_data->draw_indirect->bufferInfo, proto_data->indirect_full_buffer_info,
                                                                                    proto_data->input_instance_buffer_info, proto_data->input_highlight_buffer_info,
                                                                                    proto_data->output_instance_buffer_info, CADMesh::global_model_matrix_buffer_info}, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
                auto lastGlobalModelBuffer = vsg::DescriptorBuffer::create(vsg::BufferInfoList{CADMesh::last_global_model_matrix_buffer_info}, 6, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
                auto lastProtoBuffer = vsg::DescriptorBuffer::create(vsg::BufferInfoList{proto_data->last_instance_buffer_info}, 7, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
                auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{storageBuffer, lastGlobalModelBuffer, lastProtoBuffer});
                auto bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, descriptorSet);
                computeCommandGraphShadow->addChild(bindDescriptorSet);
                // dispatch compute shader（workgroup 数量为 1,1,1，实际 workgroup 大小由 shader 内部决定）
                // VSG 概念：Dispatch 对应 vkCmdDispatch，启动 compute shader 的执行
                computeCommandGraphShadow->addChild(vsg::Dispatch::create(1, 1, 1));

                // 创建 buffer memory barrier，确保 compute shader 写入的数据在后续 indirect draw 中可见
                // VSG 概念：BufferMemoryBarrier 是 VkBufferMemoryBarrier 的封装，
                //           用于同步 buffer 在不同 pipeline 阶段之间的访问
                auto indirect_draw_barrier = vsg::BufferMemoryBarrier::create(
                    VK_ACCESS_NONE,
                    VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                    VK_QUEUE_FAMILY_IGNORED,
                    VK_QUEUE_FAMILY_IGNORED,
                    proto_data->draw_indirect->bufferInfo->buffer,
                    0,
                    VK_WHOLE_SIZE
                );
                auto instance_data_barrier = vsg::BufferMemoryBarrier::create(
                    VK_ACCESS_NONE,
                    VK_ACCESS_UNIFORM_READ_BIT, 
                    VK_QUEUE_FAMILY_IGNORED,
                    VK_QUEUE_FAMILY_IGNORED,
                    proto_data->output_instance_buffer_info->buffer,
                    0,
                    VK_WHOLE_SIZE
                );
                ShadowPipelineBarrier->add(indirect_draw_barrier);
                ShadowPipelineBarrier->add(instance_data_barrier);
            }
            computeCommandGraphShadow->addChild(ShadowPipelineBarrier);
        }
    }


    // 创建 Switch 节点，用于动态开启/关闭每个 shadow map layer 的渲染
    // VSG 概念：Switch 是条件渲染节点，每个子节点有一个 mask，遍历时根据 mask 决定是否绘制
    preRenderSwitch = Switch::create();

    // 创建预渲染命令图：先执行 compute culling，再渲染 shadow map
    // VSG 概念：preRenderCommandGraph 在主渲染之前执行（submitOrder = -1），
    //          确保 shadow map 在主 pass 使用之前已经渲染完成
    preRenderCommandGraph = CommandGraph::create();
    preRenderCommandGraph->submitOrder = -1;
    preRenderCommandGraph->addChild(computeCommandGraphShadow);  // 先做 frustum culling
    preRenderCommandGraph->addChild(preRenderSwitch);            // 再渲染 shadow map

    // 创建一个辅助节点，只遍历 view 的 children（不触发 view 本身）
    auto tcon = TraverseChildrenOfNode::create(view);

    Mask shadowMask = 0x1; // shadow 渲染的 mask（TODO: 是否从主场景继承？）

    // 为每个 CSM 级联创建独立的 View + RenderGraph
    // VSG 概念：
    //   - View：代表一个渲染视口，有自己的 Camera（投影矩阵 + 观察矩阵）
    //   - RenderGraph：管理一个 render pass 的生命周期（开始/结束 render pass）
    //   每个 shadowMap 包含一个 View（渲染到 shadow map 的某个 array layer）和一个 RenderGraph
    ref_ptr<View> first_view;
    shadowMaps.resize(maxShadowMaps);
    for (auto& shadowMap : shadowMaps)
    {
        // 第一个 View 用 RECORD_BASE 创建，后续的 View 拷贝第一个的配置
        if (first_view)
        {
            shadowMap.view = View::create(*first_view);
        }
        else
        {
            first_view = View::create(RECORD_BASE);
            shadowMap.view = first_view;
        }

        shadowMap.view->mask = shadowMask;
        shadowMap.view->camera = Camera::create();       // 每个级联有独立的 Camera（后续在 traverse 中设置矩阵）
        shadowMap.view->addChild(tcon);                  // 将主场景子树嫁接到 shadow view

        shadowMap.renderGraph = RenderGraph::create();
        shadowMap.renderGraph->addChild(shadowMap.view);
        preRenderSwitch->addChild(MASK_DRAW, shadowMap.renderGraph);  // 添加到 Switch，初始为关闭状态
    }
    // 计算虚拟场景的世界空间包围盒（用于 CSM 正交投影范围）
    // VSG 概念：ComputeBounds 是一个 Visitor，遍历场景树计算所有几何体的 AABB 包围盒
    vsg::ComputeBounds computeSceneBounds_virtual;
    computeSceneBounds_virtual.traversalMask = MASK_PBR_FULL;       // 只统计 PBR 完整渲染的物体
    view->accept(computeSceneBounds_virtual);
    scene_bound_ws_virtual = computeSceneBounds_virtual.bounds;

    // 计算真实场景（接收阴影的物体）的世界空间包围盒
    // 用于限制 shadow far plane，确保接收阴影的物体一定被 shadow map 覆盖
    vsg::ComputeBounds computeSceneBounds_real;
    computeSceneBounds_real.traversalMask = MASK_SHADOW_RECEIVER;   // 只统计接收阴影的物体
    view->accept(computeSceneBounds_real);
    scene_bound_ws_real = computeSceneBounds_real.bounds;


}

/**
 * traverse() —— 每帧调用的核心函数，由 RecordTraversal 驱动
 *
 * VSG 概念：RecordTraversal 是 VSG 的记录遍历器，每帧遍历整个 scene graph，
 *          收集所有需要绘制的命令。ViewDependentState 的 traverse 在此过程中被调用，
 *          用于更新与当前 View 相关的 per-frame 数据（光源、阴影等）。
 *
 * 本函数完成以下工作：
 *   1. 更新场景包围盒（仅在位姿变化时重算，否则使用缓存）
 *   2. 填充 lightData uniform buffer（所有光源参数）
 *   3. 为每个 directional light 计算 CSM 分割并设置 shadow camera
 *   4. 计算 shadow map 的 texture generation matrix（从世界坐标到 shadow map UV 的变换）
 *   5. 触发 preRenderCommandGraph 的执行（compute culling + shadow map 渲染）
 */
void CustomViewDependentState::traverse(RecordTraversal& rt) const
{
    if (!view->features) return;         // 未启用 features 则跳过
    if (!draw_shadow_light && !draw_shadow_pose) return;  // 无变化则跳过

    // 仅在模型位姿变化时重新计算包围盒（光照变化不需要重算包围盒）
    if (draw_shadow_pose) {
        vsg::ComputeBounds computeSceneBounds_virtual;
        computeSceneBounds_virtual.traversalMask = MASK_PBR_FULL;
        view->accept(computeSceneBounds_virtual);
        scene_bound_ws_virtual = computeSceneBounds_virtual.bounds;
    }
    // 否则使用缓存的 scene_bound_ws_virtual

    // useful reference : https://learn.microsoft.com/en-us/windows/win32/dxtecharts/cascaded-shadow-maps
    // PCF filtering : https://github.com/SaschaWillems/Vulkan/issues/231
    // sampler2DArrayShadow
    // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VkPipelineDepthStencilStateCreateInfo.html
    // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkCmdSetDepthBoundsTestEnable.html
    //
    // Game industry SIGGRAPH presentation
    // https://www.realtimeshadows.com/sites/default/files/Playing%20with%20Real-Time%20Shadows_0.pdf
    //
    // Soft shadows:
    // https://ogldev.org/www/tutorial42/tutorial42.html
    // https://developer.download.nvidia.com/shaderlibrary/docs/shadow_PCSS.pdf
    // https://andrew-pham.blog/2019/08/03/percentage-closer-soft-shadows/
    // https://github.com/vsgopenmw-dev/vsgopenmw/blob/master/files/shaders/lib/view/shadow.glsl

    bool requiresPerRenderShadowMaps = false;
    uint32_t shadowMapIndex = 0;
    uint32_t numShadowMaps = static_cast<uint32_t>(shadowMaps.size());
    // 先关闭所有 shadow map 的渲染（后续按需开启）
    if (preRenderSwitch)
        preRenderSwitch->setAllChildren(false);
    else
        numShadowMaps = 0;

    // lambda: 计算视锥体在世界空间中的包围盒
    // 将 clip space 的 8 个角点（近平面4个 + 远平面4个）变换到世界空间
    // 注意：Vulkan clip space Z 范围为 [0,1]，但这里使用 [-1,1] 的 NDC XY + 自定义 Z
    auto computeFrustumBounds = [&](double n, double f, const dmat4& clipToWorld) -> dbox {
        dbox bounds;
        bounds.add(clipToWorld * dvec3(-1.0, -1.0, n));
        bounds.add(clipToWorld * dvec3(-1.0, 1.0, n));
        bounds.add(clipToWorld * dvec3(1.0, -1.0, n));
        bounds.add(clipToWorld * dvec3(1.0, 1.0, n));
        bounds.add(clipToWorld * dvec3(-1.0, -1.0, f));
        bounds.add(clipToWorld * dvec3(-1.0, 1.0, f));
        bounds.add(clipToWorld * dvec3(1.0, -1.0, f));
        bounds.add(clipToWorld * dvec3(1.0, 1.0, f));

        return bounds;
    };

    // lambda: 计算世界空间包围盒在光照空间中的 AABB
    // 将包围盒的 8 个角点变换到 light view space，用于设置正交投影的范围
    auto computeLightSpaceBounds = [&](const dbox& wsBound, const dmat4& viewMatrix) -> dbox {
        dbox bounds;
        bounds.add(viewMatrix * dvec3(wsBound.min.x, wsBound.min.y, wsBound.min.z));
        bounds.add(viewMatrix * dvec3(wsBound.min.x, wsBound.min.y, wsBound.max.z));
        bounds.add(viewMatrix * dvec3(wsBound.min.x, wsBound.max.y, wsBound.min.z));
        bounds.add(viewMatrix * dvec3(wsBound.min.x, wsBound.max.y, wsBound.max.z));
        bounds.add(viewMatrix * dvec3(wsBound.max.x, wsBound.min.y, wsBound.min.z));
        bounds.add(viewMatrix * dvec3(wsBound.max.x, wsBound.min.y, wsBound.max.z));
        bounds.add(viewMatrix * dvec3(wsBound.max.x, wsBound.max.y, wsBound.min.z));
        bounds.add(viewMatrix * dvec3(wsBound.max.x, wsBound.max.y, wsBound.max.z));
        return bounds;
    };

    // info("\n\nViewDependentState::traverse(", &rt, ", ", &view, ") numShadowMaps = ", numShadowMaps);

    // === 填充 lightData uniform buffer ===
    // lightData 按顺序包含：光源计数、环境光、方向光、点光源、聚光灯、shadow map 矩阵
    auto light_itr = lightData->begin();
    lightData->dirty();  // 标记数据已修改，需要重新上传到 GPU

    // 第一个 vec4：各类光源的数量（shader 用这些值来定位各光源数据的偏移）
    (*light_itr++) = vec4(static_cast<float>(ambientLights.size()),
                          static_cast<float>(directionalLights.size()),
                          static_cast<float>(pointLights.size()),
                          static_cast<float>(spotLights.size()));

    // lightData 内存布局（每个元素为 vec4）：
    // = [计数] + [ambient * 1] + [directional * 3] + [point * 2] + [spot * 3] + [shadow_map * 4]

    // 写入环境光数据（每盏占 1 个 vec4：rgb + intensity）
    for (auto& entry : ambientLights)
    {
        auto light = entry.second;
        (*light_itr++).set(light->color.r, light->color.g, light->color.b, light->intensity);
    }

    // 遍历所有方向光（directional light），每盏占 3 个 vec4
    // VSG 概念：directionalLights 存储了 (modelView矩阵, 光源指针) 的列表，
    //          modelView 矩阵用于将光源方向从模型空间变换到 eye space
    for (auto& [mv, light] : directionalLights)
    {
        // 将光源方向从模型空间变换到 eye space（用于 shader 中的光照计算）
        auto eye_direction = normalize(light->direction * inverse_3x3(mv));
        (*light_itr++).set(light->color.r, light->color.g, light->color.b, light->intensity);  // vec4: 颜色 + 强度
        (*light_itr++).set(static_cast<float>(eye_direction.x), static_cast<float>(eye_direction.y), static_cast<float>(eye_direction.z), light->area);  // vec4: 方向 + 面积

        // 计算该光源实际使用的 shadow map 数量（不超过剩余可用数量）
        uint32_t activeNumShadowMaps = std::min(light->shadowMaps, numShadowMaps - shadowMapIndex);
        (*light_itr++).set(static_cast<float>(activeNumShadowMaps), 0.0f, 0.0f, 0.0f); // vec4: 该光源的 shadow map 数量

        if (activeNumShadowMaps == 0) continue;  // 无 shadow map 则跳过后续矩阵计算

        // 标记需要执行阴影预渲染
        requiresPerRenderShadowMaps = true;

        // === 计算光照空间的坐标轴 ===
        // 目标：构建 shadow camera 的 view matrix，使得 shadow map 从光源方向观察场景
        auto projectionMatrix = view->camera->projectionMatrix->transform();
        auto viewMatrix = view->camera->viewMatrix->transform();
        auto inverse_viewMatrix = inverse(viewMatrix);

        // 相机的 view direction 和 up vector（世界空间）
        // 注意：Vulkan 的 view space 中 -Z 是观察方向，这里变换到世界空间
        auto view_direction = normalize(dvec3(0.0, 0.0, -1.0) * (projectionMatrix * viewMatrix));
        auto view_up = normalize(dvec3(0.0000000001, -1.0, 0.0000000001) * (projectionMatrix * viewMatrix));

        // 光源方向（世界空间）
        // 从模型空间经过 modelView 和 inverse_viewMatrix 变换到世界空间
        auto light_direction = normalize(light->direction * (inverse_3x3(mv * inverse_viewMatrix)));
#if 0
        info("   directional light : light direction in world = ", light_direction, ", light->shadowMaps = ", light->shadowMaps);
        info("      light->direction in model = ", light->direction);
        info("      view_direction in world = ", view_direction);
        info("      view_up in world = ", view_up);
#endif
        // 构建光照空间的坐标系 (light_x, light_y, light_z)
        // light_z = 光源方向（shadow camera 的观察方向）
        // light_x = 垂直于 light_z 和 view 的某个方向（选择更长的叉积以避免退化）
        // light_y = light_z × light_x（向上方向）
        // 这样可以减少 shadow map 在相机旋转时的"游泳"伪影（swimming artifact）
        auto light_x_direction = cross(light_direction, view_direction);
        auto light_x_up = cross(light_direction, view_up);

        auto light_x = (length(light_x_direction) > length(light_x_up)) ? normalize(light_x_direction) : normalize(light_x_up);
        auto light_y = cross(light_x, light_direction);
        auto light_z = light_direction;

        // 从 clip space 反推 eye space 的近/远平面距离
        // 注意：使用反转深度（reversed depth），near 对应 z=1，far 对应 z=0
        auto clipToEye = inverse(projectionMatrix);

        auto n = -(clipToEye * dvec3(0.0, 0.0, 1.0)).z;  // 近平面距离（reversed depth 中 z=1 为最近）
        auto f = -(clipToEye * dvec3(0.0, 0.0, 0.0)).z;  // 远平面距离（reversed depth 中 z=0 为最远）

        // clamp the near and far values
        if (n > maxShadowDistance)
        {
            // near plane further than maximum shadow distance so no need to generate shadow maps
            continue;
        }
        if (f > maxShadowDistance)
        {
            f = maxShadowDistance;
        }

        /**
         * updateCamera —— 为单个 CSM 级联设置 shadow camera 并写入 shadow map 矩阵
         *
         * 流程：
         *   1. 启用该级联的 Switch 子节点（preRenderSwitch）
         *   2. 设置 shadow camera 的 LookAt（eye/center/up）和正交投影
         *   3. 计算 texture generation matrix（从世界坐标到 shadow map UV 的 4x4 变换）
         *   4. 将矩阵写入 lightData（shader 通过这些矩阵采样 shadow map）
         */
        auto updateCamera = [&](double clip_near_z, double clip_far_z, const dmat4& clipToWorld) -> void {
            const auto& shadowMap = shadowMaps[shadowMapIndex];
            // 启用该级联的渲染（之前被 setAllChildren(false) 关闭了）
            preRenderSwitch->children[shadowMapIndex].mask = MASK_ALL;

            // 获取 shadow camera，确保有 LookAt 和 Orthographic 矩阵
            // VSG 概念：LookAt 定义相机的位置/朝向（eye, center, up），
            //          Orthographic 定义正交投影的范围（left, right, bottom, top, near, far）
            const auto& camera = shadowMap.view->camera;
            auto lookAt = camera->viewMatrix.cast<LookAt>();
            auto ortho = camera->projectionMatrix.cast<Orthographic>();

            if (!lookAt) camera->viewMatrix = lookAt = LookAt::create();
            if (!ortho) camera->projectionMatrix = ortho = Orthographic::create();

            // 计算该级联对应的视锥体切片在世界空间中的包围盒
            auto ws_bounds = computeFrustumBounds(clip_near_z, clip_far_z, clipToWorld);
            // shadow camera 的 eye 位置：包围盒中心沿光源反方向偏移一半对角线距离
            // 这样确保 shadow camera 能看到整个包围盒
            auto sm_eye = (ws_bounds.min + ws_bounds.max) * 0.5 - light_z * (0.5 * length(ws_bounds.max - ws_bounds.min));

            lookAt->eye = sm_eye;
            lookAt->center = sm_eye + light_z;
            lookAt->up = light_y;

            // auto ls_bounds = computeFrustumBounds(clip_near_z, clip_far_z, lookAt->transform() * clipToWorld);
            // 计算虚拟场景和真实场景在光照空间中的包围盒
            auto ls_bounds_virtual = computeLightSpaceBounds(scene_bound_ws_virtual, lookAt->transform());
            auto ls_bounds_real = computeLightSpaceBounds(scene_bound_ws_real, lookAt->transform());

            // 用虚拟场景的 XY 范围设置正交投影（确保所有虚拟物体都在 shadow map 范围内）
            ortho->left = ls_bounds_virtual.min.x;
            ortho->right = ls_bounds_virtual.max.x;
            ortho->bottom = ls_bounds_virtual.min.y;
            ortho->top = ls_bounds_virtual.max.y;
            ortho->nearDistance = -ls_bounds_virtual.max.z;  // 反转深度：near = -maxZ

            // === 计算 shadow far plane ===
            // 策略：将包围盒的 8 个角点沿光照方向投射到世界坐标 z=-2 平面，
            //       取所有交点中最远的一个作为 far plane 的候选值
            // 目的：扩展 far plane 以覆盖可能被遮挡的远处物体（如地面）
            double target_world_z = -2.0;            // 目标平面的世界 Z 坐标
            double max_far_distance = ortho->nearDistance;  // 初始值为 near distance

            if (std::abs(light_z.z) > 1e-6) {
                // 遍历包围盒的8个角点
                for (int i = 0; i < 8; ++i) {
                    dvec3 corner(
                        (i & 1) ? ws_bounds.max.x : ws_bounds.min.x,
                        (i & 2) ? ws_bounds.max.y : ws_bounds.min.y,
                        (i & 4) ? ws_bounds.max.z : ws_bounds.min.z
                    );

                    // 计算从角点沿光照方向到目标平面的参数t
                    double t = (target_world_z - corner.z) / light_z.z;

                    if (t > 0.0) {
                        // 计算世界坐标交点
                        dvec3 intersection_world = corner + light_z * t;

                        // 变换到光照空间
                        dvec4 intersection_light = lookAt->transform() * dvec4(intersection_world, 1.0);

                        // 更新最远距离
                        max_far_distance = std::max(max_far_distance, -intersection_light.z);
                    }
                }
            }

            // 限制最大距离
            double max_additional_distance = (ls_bounds_virtual.max.z - ls_bounds_virtual.min.z) * 2.0;
            ortho->farDistance = std::min(max_far_distance, ortho->nearDistance + max_additional_distance);

            // 确保 far plane 包含真实场景的最近点（避免阴影丢失）
            if(!std::isinf(ls_bounds_real.min.z))
                ortho->farDistance = std::max(-ls_bounds_real.min.z, ortho->farDistance);

            // === 计算 texture generation matrix（shadow map 矩阵）===
            // 将世界坐标变换到 shadow map 的 [0,1] UV 范围：
            //   1. shadowMapProjView: 世界 → clip space（shadow camera 的 view * proj）
            //   2. scale(0.5, 0.5, 1.0) * translate(1.0, 1.0, 0.0): clip space → [0,1] UV
            //      这是 NDC [-1,1] → [0,1] 的标准变换
            dmat4 shadowMapProjView = camera->projectionMatrix->transform() * camera->viewMatrix->transform();
            dmat4 shadowMapTM = scale(0.5, 0.5, 1.0) * translate(1.0, 1.0, 0.0) * shadowMapProjView;

            // convert tex gen matrix to float matrix and assign to light data
            mat4 m(shadowMapTM);

            (*light_itr++) = m[0];
            (*light_itr++) = m[1];
            (*light_itr++) = m[2];
            (*light_itr++) = m[3];

            // info("m = ", m);

            // advance to the next shadowMap
            shadowMapIndex++;
        };

#if 0
        info("     light_x = ", light_x);
        info("     light_y = ", light_y);
        info("     light_z = ", light_z);
#endif

#if 0
        double range = f - n;
        info("    n = ", n, ", f = ", f, ", range = ", range);
#endif
        auto clipToWorld = inverse(projectionMatrix * viewMatrix);

        // === CSM 分割：将视锥体按 Cpractical 方案分成多个级联 ===
        if (activeNumShadowMaps > 1)
        {
            // 多级联模式：使用 Cpractical() 计算每个级联的近/远边界
            // lambda 定义了对数分割和均匀分割的混合比例
            double m = static_cast<double>(activeNumShadowMaps);
            for (double i = 0; i < m; i += 1.0)
            {
                // 计算第 i 个级联的近/远平面（eye space Z 坐标）
                dvec3 eye_near(0.0, 0.0, -Cpractical(n, f, i, m, lambda));
                dvec3 eye_far(0.0, 0.0, -Cpractical(n, f, i + 1.0, m, lambda));

                auto clip_near = projectionMatrix * eye_near;
                auto clip_far = projectionMatrix * eye_far;

                updateCamera(clip_near.z, clip_far.z, clipToWorld);
            }
        }
        else
        {
            // 单 shadow map 模式：覆盖整个近-远范围（不分级联）
            dvec3 eye_near(0.0, 0.0, -n);
            dvec3 eye_far(0.0, 0.0, -f);

            auto clip_near = projectionMatrix * eye_near;
            auto clip_far = projectionMatrix * eye_far;

            updateCamera(clip_near.z, clip_far.z, clipToWorld);
        }
    }

    // 写入点光源数据（每盏占 2 个 vec4：颜色+强度，位置+0）
    // VSG 概念：pointLights 存储了 (modelView矩阵, 光源指针) 的列表
    for (auto& [mv, light] : pointLights)
    {
        auto eye_position = mv * light->position;  // 将位置变换到 eye space
        (*light_itr++).set(light->color.r, light->color.g, light->color.b, light->intensity);
        (*light_itr++).set(static_cast<float>(eye_position.x), static_cast<float>(eye_position.y), static_cast<float>(eye_position.z), 0.0f);
    }

    // 写入聚光灯数据（每盏占 3 个 vec4：颜色+强度，位置+内锥角余弦，方向+外锥角余弦）
    // VSG 概念：spotLights 存储了 (modelView矩阵, 光源指针) 的列表
    for (auto& [mv, light] : spotLights)
    {
        auto eye_position = mv * light->position;
        auto eye_direction = normalize(light->direction * inverse_3x3(mv));
        float cos_innerAngle = static_cast<float>(cos(light->innerAngle));  // 内锥角余弦（用于平滑衰减）
        float cos_outerAngle = static_cast<float>(cos(light->outerAngle));  // 外锥角余弦
        (*light_itr++).set(light->color.r, light->color.g, light->color.b, light->intensity);
        (*light_itr++).set(static_cast<float>(eye_position.x), static_cast<float>(eye_position.y), static_cast<float>(eye_position.z), cos_innerAngle);
        (*light_itr++).set(static_cast<float>(eye_direction.x), static_cast<float>(eye_direction.y), static_cast<float>(eye_direction.z), cos_outerAngle);
    }

    // 如果需要 shadow map，触发 preRenderCommandGraph 的执行
    // VSG 概念：accept(rt) 将 RecordTraversal 传递给 preRenderCommandGraph，
    //          使其在主渲染命令之前被记录和提交到 GPU
    // 执行顺序：compute culling → shadow map 渲染 → 主场景渲染
    if (requiresPerRenderShadowMaps && preRenderCommandGraph)
    {
        preRenderCommandGraph->accept(rt);
    }

    // 重置标记，等待下一次变化
    draw_shadow_light = false;
    draw_shadow_pose = false;
}