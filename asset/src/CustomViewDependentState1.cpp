/**
 * CustomViewDependentState1 实现文件
 *
 * 这是一个轻量级的 ViewDependentState，专门为 view1（合成渲染 pass / synthesis render pass）设计。
 * 它不独立管理灯光和阴影，而是通过复用主 CustomViewDependentState 的 descriptor set，
 * 确保 view1 和主 view 共享相同的 shadow map 和灯光数据。
 *
 * VSG 知识：VulkanSceneGraph 的 scene graph 遍历时，每个 View 都会调用其 ViewDependentState 的
 * traverse() 来更新灯光、阴影等状态。本类的 traverse() 为空，意味着 view1 不会自己生成阴影，
 * 而是直接使用主 view 生成的 shadow map。
 */

#include "CustomViewDependentState1.h"

using namespace vsg;

/**
 * 辅助类：只遍历某个 Node 的子节点，而不遍历 Node 自身
 * VSG 知识：在 VSG 中，Node::traverse() 默认遍历其所有 children。
 * 这个类包装了一个 Node，只在 traverse 时调用原 Node 的 traverse() 来访问子节点。
 * 常用于在 scene graph 中"跳过"某个节点本身，只处理其子树。
 */
namespace vsg
{
    class TraverseChildrenOfNode : public Inherit<Node, TraverseChildrenOfNode>
    {
    public:
        explicit TraverseChildrenOfNode(Node* in_node) :
            node(in_node) {}

        // 使用 observer_ptr 避免循环引用（weak reference 语义）
        observer_ptr<Node> node;

        template<class N, class V>
        static void t_traverse(N& in_node, V& visitor)
        {
            if (auto ref_node = in_node.node.ref_ptr()) ref_node->traverse(visitor);
        }

        // 支持三种 visitor 类型的遍历
        void traverse(Visitor& visitor) override { t_traverse(*this, visitor); }
        void traverse(ConstVisitor& visitor) const override { t_traverse(*this, visitor); }
        void traverse(RecordTraversal& visitor) const override { t_traverse(*this, visitor); }
    };
    VSG_type_name(TraverseChildrenOfNode);

    /**
     * Cpractical - 阴影贴图分割的实用分割公式（Practical Split Scheme）
     *
     * VSG 知识：这是 Cascaded Shadow Maps (CSM) 中用于计算各层级近/远平面的公式。
     * @param n        近平面距离 (near)
     * @param f        远平面距离 (far)
     * @param i        当前级联索引 (cascade index)
     * @param m        总级联数 (total cascades)
     * @param lambda   混合系数（0=均匀分割, 1=对数分割），控制 log 和 uniform 分割的混合比例
     *
     * 对数分割对近处物体精度高，均匀分割对远处更平滑，lambda 控制两者的平衡。
     */
    inline double Cpractical(double n, double f, double i, double m, double lambda)
    {
        double Clog = n * std::pow((f / n), (i / m));
        double Cuniform = n + (f - n) * (i / m);
        return Clog * lambda + Cuniform * (1.0 - lambda);
    };

} // namespace vsg

/**
 * 创建自定义的 shadow map Image 对象
 *
 * VSG 知识：vsg::Image 对应 Vulkan 的 VkImage，表示 GPU 上的一块图像内存。
 * 这里创建的是 2D 类型的 image，使用 array layers（而非 mip levels）来存储多张阴影贴图。
 * 例如，levels=8 意味着一个 image array 中有 8 层，每层对应一个光源的 shadow map。
 *
 * @param width   阴影贴图宽度
 * @param height  阴影贴图高度
 * @param levels  阴影贴图层数（对应光源数量）
 * @param format  像素格式，通常为 VK_FORMAT_D32_SFLOAT（32位深度）
 * @param usage   image 用途标志（如 DEPTH_STENCIL_ATTACHMENT + SAMPLED）
 */
vsg::ref_ptr<vsg::Image> CustomViewDependentState1::createCustomShadowImage(uint32_t width, uint32_t height, uint32_t levels, VkFormat format, VkImageUsageFlags usage)
{
    auto image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = format;
    image->extent = VkExtent3D{width, height, 1};
    image->mipLevels = 1;
    image->arrayLayers = levels;
    image->samples = VK_SAMPLE_COUNT_1_BIT;
    image->tiling = VK_IMAGE_TILING_OPTIMAL;
    image->usage = usage;
    image->initialLayout = VK_IMAGE_LAYOUT_GENERAL;
    image->flags = 0;
    image->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    return image;
}

/**
 * init() - 初始化 ViewDependentState 的 descriptor set
 *
 * 关键设计：这个 init() 的前半部分和主 CustomViewDependentState::init() 几乎完全一样，
 * 会创建自己的 lightData、viewportData、shadow map 等资源和 descriptor set layout。
 * 但是在最后两行，它会用 pre_depth_pass（主 View 的 CustomViewDependentState）的
 * descriptorSetLayout 和 descriptorSet 覆盖掉自己创建的。
 *
 * 这样做的目的：view1（合成 pass）不需要独立生成阴影或管理灯光，
 * 它只需要使用和主 view 完全相同的 descriptor set，确保 shader 中读到的 shadow map
 * 和灯光数据是一致的。两个 pass 渲染同一个场景的不同部分时，阴影必须完全对齐。
 *
 * VSG 知识：
 * - DescriptorSet：Vulkan 中绑定到 pipeline 的资源集合（uniform buffer、image sampler 等）
 * - DescriptorSetLayout：描述 descriptor set 中每个 binding 的类型和用途
 * - ResourceRequirements：VSG 在编译场景时收集的资源需求信息（灯光数量、阴影贴图数量等）
 */
void CustomViewDependentState1::init(ResourceRequirements& requirements)
{
    // 检查是否已经初始化过（VSG 中的惯例：避免重复初始化）
    if (lightData) return;

    // 最大灯光数、视口数、阴影贴图尺寸等默认值
    uint32_t maxNumberLights = 64;
    uint32_t maxViewports = 1;

    // 阴影贴图的默认分辨率和最大级联数
    uint32_t shadowWidth = 2048;
    uint32_t shadowHeight = 2048;
    uint32_t maxShadowMaps = 8;

    auto& viewDetails = requirements.views[view];

    if (view->features != 0)
    {
        uint32_t numLights = static_cast<uint32_t>(viewDetails.lights.size());
        uint32_t numShadowMaps = 0;
        for (auto& light : viewDetails.lights)
        {
            numShadowMaps += light->shadowMaps;
        }

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

    // 创建灯光数据 uniform buffer（vec4 数组，包含灯光方向/位置/颜色等信息）
    // VSG 知识：DYNAMIC_DATA_TRANSFER_AFTER_RECORD 表示数据在录制命令之后才上传到 GPU
    lightData = vec4Array::create(lightDataSize);
    lightData->properties.dataVariance = DYNAMIC_DATA_TRANSFER_AFTER_RECORD;
    lightDataBufferInfo = BufferInfo::create(lightData.get());

    // 创建视口数据 uniform buffer（用于多视口渲染）
    viewportData = vec4Array::create(maxViewports);
    viewportData->properties.dataVariance = DYNAMIC_DATA_TRANSFER_AFTER_RECORD;
    viewportDataBufferInfo = BufferInfo::create(viewportData.get());

    // 将 lightData 和 viewportData 打包成一个 DescriptorBuffer，绑定到 set 的第 0 个 binding
    descriptor = DescriptorBuffer::create(BufferInfoList{lightDataBufferInfo, viewportDataBufferInfo}, 0); // hardwired position for now

    // 创建阴影贴图的 sampler（采样器）
    // shadowMapSampler：带 depth compare 的采样器，用于硬件 PCF（percentage closer filtering）
    // shadowMapSamplerNoCompare：不带 depth compare 的采样器，用于 shader 中手动做阴影比较
    auto shadowMapSampler = Sampler::create();
    auto shadowMapSamplerNoCompare = Sampler::create();
// #define HARDWARE_PCF 1
#if HARDWARE_PCF == 1
    shadowMapSampler->minFilter = VK_FILTER_LINEAR;
    shadowMapSampler->magFilter = VK_FILTER_LINEAR;
    shadowMapSampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    shadowMapSampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSampler->compareEnable = VK_TRUE;
    shadowMapSampler->compareOp = VK_COMPARE_OP_GREATER;

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
    shadowMapSampler->compareOp = VK_COMPARE_OP_GREATER;

    shadowMapSamplerNoCompare->minFilter = VK_FILTER_NEAREST;
    shadowMapSamplerNoCompare->magFilter = VK_FILTER_NEAREST;
    shadowMapSamplerNoCompare->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    shadowMapSamplerNoCompare->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSamplerNoCompare->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSamplerNoCompare->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
#endif

    if (maxShadowMaps > 0)
    {
        shadowDepthImage = createCustomShadowImage(shadowWidth, shadowHeight, maxShadowMaps, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);

        auto depthImageView = ImageView::create(shadowDepthImage, VK_IMAGE_ASPECT_DEPTH_BIT);
        depthImageView->viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        depthImageView->subresourceRange.baseMipLevel = 0;
        depthImageView->subresourceRange.levelCount = 1;
        depthImageView->subresourceRange.baseArrayLayer = 0;
        depthImageView->subresourceRange.layerCount = maxShadowMaps;

        auto depthImageInfo = ImageInfo::create(shadowMapSampler, depthImageView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        shadowMapImages = DescriptorImage::create(ImageInfoList{depthImageInfo}, 2);

        auto depthImageSamplerInfo = ImageInfo::create(shadowMapSamplerNoCompare, depthImageView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        shadowMapSamplerImages = DescriptorImage::create(ImageInfoList{depthImageSamplerInfo}, 3);
    }
    else
    {
        //
        // fallback to provide a descriptor image to use when the ViewDependentState shadow map generation is not active
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

    // 定义 descriptor set layout 的 4 个 binding：
    //   binding 0: uniform buffer - 灯光数据 (lightData)
    //   binding 1: uniform buffer - 视口数据 (viewportData)
    //   binding 2: combined image sampler - shadow map（带 depth compare）
    //   binding 3: combined image sampler - shadow map（不带 depth compare，手动比较用）
    DescriptorSetLayoutBindings descriptorBindings{
        VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}, // lightData
        VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}, // viewportData
        VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},                      // shadow map 2D texture array
        VkDescriptorSetLayoutBinding{3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},                      // shadow map 2D texture array
    };

    // 先创建自己的 descriptorSetLayout 和 descriptorSet
    descriptorSetLayout = DescriptorSetLayout::create(descriptorBindings);
    descriptorSet = DescriptorSet::create(descriptorSetLayout, Descriptors{descriptor, shadowMapImages, shadowMapSamplerImages});

    // 【关键】覆盖为主 View（pre_depth_pass）的 descriptor set
    // 这样 view1 的 shader 就会使用和主 view 完全相同的灯光数据和 shadow map，
    // 确保两个 pass 的阴影和光照效果完全一致
    descriptorSetLayout = pre_depth_pass->descriptorSetLayout;
    descriptorSet = pre_depth_pass->descriptorSet;

}

/**
 * traverse() - 空实现
 *
 * VSG 知识：在场景图遍历（RecordTraversal）时，每个 View 的 ViewDependentState::traverse()
 * 会被调用，通常用来更新灯光位置、计算阴影矩阵、上传 uniform 数据等。
 *
 * 这里故意留空，因为 CustomViewDependentState1 不需要独立管理任何状态。
 * 主 View 的 CustomViewDependentState 已经完成了所有灯光和阴影的准备工作，
 * 本类通过共享其 descriptor set 直接使用这些数据。
 */
void CustomViewDependentState1::traverse(RecordTraversal& rt) const
{}

// void CustomViewDependentState1::compile(Context& context)
// {}