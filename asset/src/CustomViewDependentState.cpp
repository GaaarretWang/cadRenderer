#include "CustomViewDependentState.h"

using namespace vsg;

//////////////////////////////////////
//
// TraverseChildrenOfNode
//
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

    inline double Cpractical(double n, double f, double i, double m, double lambda)
    {
        double Clog = n * std::pow((f / n), (i / m));
        double Cuniform = n + (f - n) * (i / m);
        return Clog * lambda + Cuniform * (1.0 - lambda);
    };

} // namespace vsg

vsg::ref_ptr<vsg::Image> CustomViewDependentState::createCustomShadowImage(uint32_t width, uint32_t height, uint32_t levels, VkFormat format, VkImageUsageFlags usage){
    auto image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = format;
    image->extent = VkExtent3D{width, height, 1};
    image->mipLevels = 1;
    image->arrayLayers = levels;
    image->samples = VK_SAMPLE_COUNT_1_BIT;
    image->tiling = VK_IMAGE_TILING_OPTIMAL;
    image->usage = usage;
    image->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image->flags = 0;
    image->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    return image;
}

// to override descriptorset layout
void CustomViewDependentState::init(ResourceRequirements& requirements)
{
    // check if ViewDependentState has already been initialized
    if (lightData) return;

    uint32_t maxNumberLights = 64;
    uint32_t maxViewports = 1;

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

    lightData = vec4Array::create(lightDataSize);
    lightData->properties.dataVariance = DYNAMIC_DATA_TRANSFER_AFTER_RECORD;
    lightDataBufferInfo = BufferInfo::create(lightData.get());

    viewportData = vec4Array::create(maxViewports);
    viewportData->properties.dataVariance = DYNAMIC_DATA_TRANSFER_AFTER_RECORD;
    viewportDataBufferInfo = BufferInfo::create(viewportData.get());

    descriptor = DescriptorBuffer::create(BufferInfoList{lightDataBufferInfo, viewportDataBufferInfo}, 0); // hardwired position for now

    viewMatrixData = mat4Array::create(4);
    viewMatrixData->properties.dataVariance = DYNAMIC_DATA_TRANSFER_AFTER_RECORD;
    viewMatrixDataBufferInfo = BufferInfo::create(viewMatrixData.get());
    auto descriptor1 = DescriptorBuffer::create(BufferInfoList{viewMatrixDataBufferInfo}, 3); // hardwired position for now



    // set up ShadowMaps
    auto shadowMapSampler = Sampler::create();
#define HARDWARE_PCF 1
#if HARDWARE_PCF == 1
    shadowMapSampler->minFilter = VK_FILTER_LINEAR;
    shadowMapSampler->magFilter = VK_FILTER_LINEAR;
    shadowMapSampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    shadowMapSampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSampler->compareEnable = VK_TRUE;
    shadowMapSampler->compareOp = VK_COMPARE_OP_LESS;
#else
    shadowMapSampler->minFilter = VK_FILTER_NEAREST;
    shadowMapSampler->magFilter = VK_FILTER_NEAREST;
    shadowMapSampler->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    shadowMapSampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    shadowMapSampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
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
    }

    DescriptorSetLayoutBindings descriptorBindings{
        VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}, // lightData
        VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}, // viewportData
        VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},                      // shadow map 2D texture array
        VkDescriptorSetLayoutBinding{3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}, // viewportData
    };

    descriptorSetLayout = DescriptorSetLayout::create(descriptorBindings);
    descriptorSet = DescriptorSet::create(descriptorSetLayout, Descriptors{descriptor, shadowMapImages, descriptor1});

    // if not active then don't enable shadow maps
    if (maxShadowMaps == 0) return;

    // create a switch to toggle on/off the render to texture subgraphs for each shadowmap layer
    preRenderSwitch = Switch::create();

    preRenderCommandGraph = CommandGraph::create();
    preRenderCommandGraph->submitOrder = -1;
    preRenderCommandGraph->addChild(preRenderSwitch);

    auto tcon = TraverseChildrenOfNode::create(view);

    Mask shadowMask = 0x1; // TODO: do we inherit from main scene? how?

    ref_ptr<View> first_view;
    shadowMaps.resize(maxShadowMaps);
    for (auto& shadowMap : shadowMaps)
    {
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
        shadowMap.view->camera = Camera::create();
        shadowMap.view->addChild(tcon);

        shadowMap.renderGraph = RenderGraph::create();
        shadowMap.renderGraph->addChild(shadowMap.view);
        preRenderSwitch->addChild(MASK_ALL, shadowMap.renderGraph);
    }
    vsg::ComputeBounds computeSceneBounds_virtual;
    computeSceneBounds_virtual.traversalMask = MASK_PBR_FULL;
    view->accept(computeSceneBounds_virtual);
    //auto ws_bounds = computeFrustumBounds(clip_near_z, clip_far_z, clipToWorld);
    scene_bound_ws_virtual = computeSceneBounds_virtual.bounds;

    vsg::ComputeBounds computeSceneBounds_real;
    computeSceneBounds_real.traversalMask = MASK_SHADOW_RECEIVER;
    view->accept(computeSceneBounds_real);
    //auto ws_bounds = computeFrustumBounds(clip_near_z, clip_far_z, clipToWorld);
    scene_bound_ws_real = computeSceneBounds_real.bounds;


}

// to save viewMatrix and inverseViewMatrix
void CustomViewDependentState::traverse(RecordTraversal& rt) const
{
    if (!view->features) return;
    
    auto& vsgViewMatrix = view->camera->viewMatrix;
    dmat4 viewMat = vsgViewMatrix->transform();
    dmat4 invViewMat = vsgViewMatrix->inverse();

    auto viewMatrixItr = viewMatrixData->begin();
    (*viewMatrixItr++) = mat4(viewMat);
    (*viewMatrixItr++) = mat4(invViewMat);
    if (view->camera->viewMatrix->is_compatible(typeid(vsg::LookAt))){
        LookAt* lookAt = dynamic_cast<LookAt*>(view->camera->viewMatrix.get());
        mat4 data = {};
        data[0] = vec4(lookAt->eye, 0.0f);
        (*viewMatrixItr++) = data;
    }
    viewMatrixData->dirty();

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
    if (preRenderSwitch)
        preRenderSwitch->setAllChildren(false);
    else
        numShadowMaps = 0;

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

    // set up the light data
    auto light_itr = lightData->begin();
    lightData->dirty();

    (*light_itr++) = vec4(static_cast<float>(ambientLights.size()),
                          static_cast<float>(directionalLights.size()),
                          static_cast<float>(pointLights.size()),
                          static_cast<float>(spotLights.size()));

    // lightData requirements = vec4 * (num_ambientLights + 3 * num_directionLights + 3 * num_pointLights + 4 * num_spotLights + 4 * num_shadow_maps)

    for (auto& entry : ambientLights)
    {
        auto light = entry.second;
        (*light_itr++).set(light->color.r, light->color.g, light->color.b, light->intensity);
    }

    for (auto& [mv, light] : directionalLights)
    {
        // info("   light ", light->className(), ", light->shadowMaps = ", light->shadowMaps);

        // assign basic direction light settings to light data
        auto eye_direction = normalize(light->direction * inverse_3x3(mv));
        (*light_itr++).set(light->color.r, light->color.g, light->color.b, light->intensity);
        (*light_itr++).set(static_cast<float>(eye_direction.x), static_cast<float>(eye_direction.y), static_cast<float>(eye_direction.z), 0.0f);

        uint32_t activeNumShadowMaps = std::min(light->shadowMaps, numShadowMaps - shadowMapIndex);
        (*light_itr++).set(static_cast<float>(activeNumShadowMaps), 0.0f, 0.0f, 0.0f); // shadow map setting

        if (activeNumShadowMaps == 0) continue;

        // set up shadow map rendering backend
        requiresPerRenderShadowMaps = true;

        // compute directional light space
        auto projectionMatrix = view->camera->projectionMatrix->transform();
        auto viewMatrix = view->camera->viewMatrix->transform();
        auto inverse_viewMatrix = inverse(viewMatrix);

        // view direction in world coords
        auto view_direction = normalize(dvec3(0.0, 0.0, -1.0) * (projectionMatrix * viewMatrix));
        auto view_up = normalize(dvec3(0.0000000001, -1.0, 0.0000000001) * (projectionMatrix * viewMatrix));

        // light direction in world coords
        auto light_direction = normalize(light->direction * (inverse_3x3(mv * inverse_viewMatrix)));
#if 0
        info("   directional light : light direction in world = ", light_direction, ", light->shadowMaps = ", light->shadowMaps);
        info("      light->direction in model = ", light->direction);
        info("      view_direction in world = ", view_direction);
        info("      view_up in world = ", view_up);
#endif
        auto light_x_direction = cross(light_direction, view_direction);
        auto light_x_up = cross(light_direction, view_up);

        auto light_x = (length(light_x_direction) > length(light_x_up)) ? normalize(light_x_direction) : normalize(light_x_up);
        auto light_y = cross(light_x, light_direction);
        auto light_z = light_direction;

        auto clipToEye = inverse(projectionMatrix);

        auto n = -(clipToEye * dvec3(0.0, 0.0, 1.0)).z;
        auto f = -(clipToEye * dvec3(0.0, 0.0, 0.0)).z;

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

        auto updateCamera = [&](double clip_near_z, double clip_far_z, const dmat4& clipToWorld) -> void {
            const auto& shadowMap = shadowMaps[shadowMapIndex];
            preRenderSwitch->children[shadowMapIndex].mask = MASK_ALL;

            const auto& camera = shadowMap.view->camera;
            auto lookAt = camera->viewMatrix.cast<LookAt>();
            auto ortho = camera->projectionMatrix.cast<Orthographic>();

            if (!lookAt) camera->viewMatrix = lookAt = LookAt::create();
            if (!ortho) camera->projectionMatrix = ortho = Orthographic::create();

            auto ws_bounds = computeFrustumBounds(clip_near_z, clip_far_z, clipToWorld);
            auto sm_eye = (ws_bounds.min + ws_bounds.max) * 0.5 - light_z * (0.5 * length(ws_bounds.max - ws_bounds.min));

            lookAt->eye = sm_eye;
            lookAt->center = sm_eye + light_z;
            lookAt->up = light_y;

            // auto ls_bounds = computeFrustumBounds(clip_near_z, clip_far_z, lookAt->transform() * clipToWorld);
            auto ls_bounds_virtual = computeLightSpaceBounds(scene_bound_ws_virtual, lookAt->transform());
            auto ls_bounds_real = computeLightSpaceBounds(scene_bound_ws_real, lookAt->transform());

            ortho->left = ls_bounds_virtual.min.x;
            ortho->right = ls_bounds_virtual.max.x;
            ortho->bottom = ls_bounds_virtual.min.y;
            ortho->top = ls_bounds_virtual.max.y;
            ortho->nearDistance = -ls_bounds_virtual.max.z;
            ortho->farDistance = -ls_bounds_real.min.z;

            dmat4 shadowMapProjView = camera->projectionMatrix->transform() * camera->viewMatrix->transform();
            dmat4 shadowMapTM = scale(0.5, 0.5, 1.0) * translate(1.0, 1.0, shadowMapBias) * shadowMapProjView * inverse_viewMatrix;

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

        if (activeNumShadowMaps > 1)
        {
            double m = static_cast<double>(activeNumShadowMaps);
            for (double i = 0; i < m; i += 1.0)
            {
                dvec3 eye_near(0.0, 0.0, -Cpractical(n, f, i, m, lambda));
                dvec3 eye_far(0.0, 0.0, -Cpractical(n, f, i + 1.0, m, lambda));

                auto clip_near = projectionMatrix * eye_near;
                auto clip_far = projectionMatrix * eye_far;

                updateCamera(clip_near.z, clip_far.z, clipToWorld);
            }
        }
        else
        {
            dvec3 eye_near(0.0, 0.0, -n);
            dvec3 eye_far(0.0, 0.0, -f);

            auto clip_near = projectionMatrix * eye_near;
            auto clip_far = projectionMatrix * eye_far;

            updateCamera(clip_near.z, clip_far.z, clipToWorld);
        }
    }

    for (auto& [mv, light] : pointLights)
    {
        auto eye_position = mv * light->position;
        (*light_itr++).set(light->color.r, light->color.g, light->color.b, light->intensity);
        (*light_itr++).set(static_cast<float>(eye_position.x), static_cast<float>(eye_position.y), static_cast<float>(eye_position.z), 0.0f);
    }

    for (auto& [mv, light] : spotLights)
    {
        auto eye_position = mv * light->position;
        auto eye_direction = normalize(light->direction * inverse_3x3(mv));
        float cos_innerAngle = static_cast<float>(cos(light->innerAngle));
        float cos_outerAngle = static_cast<float>(cos(light->outerAngle));
        (*light_itr++).set(light->color.r, light->color.g, light->color.b, light->intensity);
        (*light_itr++).set(static_cast<float>(eye_position.x), static_cast<float>(eye_position.y), static_cast<float>(eye_position.z), cos_innerAngle);
        (*light_itr++).set(static_cast<float>(eye_direction.x), static_cast<float>(eye_direction.y), static_cast<float>(eye_direction.z), cos_outerAngle);
    }

    if (requiresPerRenderShadowMaps && preRenderCommandGraph)
    {
        // info("ViewDependentState::traverse(RecordTraversal&) doing pre render command graph. shadowMapIndex = ", shadowMapIndex);
        preRenderCommandGraph->accept(rt);
    }
}