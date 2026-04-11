#include "RayTracingEngine.h"
#include <iostream>

using RayTracingUniformValue = vsg::Value<RayTracingUniform>;

void RayTracingEngine::initialize(vsg::ref_ptr<vsg::Device> device,
                                   vsg::ref_ptr<vsg::Window> window)
{
    _device = device;
    _width = window->extent2D().width;
    _height = window->extent2D().height;

    vsg::info("RayTracingEngine: Initializing with resolution ", _width, "x", _height);

    // Load shaders
    auto options = vsg::Options::create();
    options->paths.push_back("asset/data/shaders/");
    options->paths.push_back("../asset/data/shaders/");
    options->paths.push_back("data/shaders/");

    vsg::info("RayTracingEngine: Loading shaders...");
    // Load compiled SPIR-V shaders
    auto raygenShader = vsg::ShaderStage::read(VK_SHADER_STAGE_RAYGEN_BIT_KHR,
        "main", "rt_simple.rgen.spv", options);
    auto missShader = vsg::ShaderStage::read(VK_SHADER_STAGE_MISS_BIT_KHR,
        "main", "rt_simple.rmiss.spv", options);
    auto closesthitShader = vsg::ShaderStage::read(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        "main", "rt_simple.rchit.spv", options);

    if (!raygenShader || !missShader || !closesthitShader)
    {
        vsg::warn("RayTracingEngine: Could not load shaders, ray tracing disabled");
        vsg::warn("  raygenShader: ", (raygenShader ? "OK" : "FAILED"));
        vsg::warn("  missShader: ", (missShader ? "OK" : "FAILED"));
        vsg::warn("  closesthitShader: ", (closesthitShader ? "OK" : "FAILED"));
        return;
    }

    vsg::info("RayTracingEngine: Shaders loaded successfully");

    auto shaderStages = vsg::ShaderStages{raygenShader, missShader, closesthitShader};

    // Set up shader groups (save as member variables)
    raygenShaderGroup = vsg::RayTracingShaderGroup::create();
    raygenShaderGroup->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    raygenShaderGroup->generalShader = 0;

    missShaderGroup = vsg::RayTracingShaderGroup::create();
    missShaderGroup->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    missShaderGroup->generalShader = 1;

    closestHitShaderGroup = vsg::RayTracingShaderGroup::create();
    closestHitShaderGroup->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    closestHitShaderGroup->closestHitShader = 2;

    auto shaderGroups = vsg::RayTracingShaderGroups{raygenShaderGroup, missShaderGroup, closestHitShaderGroup};

    // Create storage image for ray tracing output
    auto context = vsg::Context::create(_device);

    rtStorageImage = vsg::Image::create();
    rtStorageImage->imageType = VK_IMAGE_TYPE_2D;
    rtStorageImage->format = VK_FORMAT_B8G8R8A8_UNORM;  // Critical: B8G8R8A8, not R8G8B8A8
    rtStorageImage->extent.width = _width;
    rtStorageImage->extent.height = _height;
    rtStorageImage->extent.depth = 1;
    rtStorageImage->mipLevels = 1;
    rtStorageImage->arrayLayers = 1;
    rtStorageImage->samples = VK_SAMPLE_COUNT_1_BIT;
    rtStorageImage->tiling = VK_IMAGE_TILING_OPTIMAL;
    rtStorageImage->usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    rtStorageImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    rtStorageImage->flags = 0;
    rtStorageImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    rtStorageImageView = createImageView(*context, rtStorageImage, VK_IMAGE_ASPECT_COLOR_BIT);
    auto storageImageInfo = vsg::ImageInfo::create(vsg::ref_ptr<vsg::Sampler>{}, rtStorageImageView, VK_IMAGE_LAYOUT_GENERAL);

    // Create descriptor set layout
    vsg::DescriptorSetLayoutBindings descriptorBindings{
        {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},
        {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr}};

    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

    // Create pipeline layout and ray tracing pipeline
    rtPipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, vsg::PushConstantRanges{});
    rtPipeline = vsg::RayTracingPipeline::create(rtPipelineLayout, shaderStages, shaderGroups);
    bindRTPipeline = vsg::BindRayTracingPipeline::create(rtPipeline);

    vsg::info("RayTracingEngine: Initialization complete");
}

void RayTracingEngine::buildAccelerationStructures(vsg::ref_ptr<vsg::Node> scene)
{
    if (!scene || !_device)
    {
        vsg::warn("RayTracingEngine: Cannot build acceleration structures - invalid scene or device");
        return;
    }

    vsg::info("RayTracingEngine: Building acceleration structures from scene");

    // Use VSG's built-in traversal to build acceleration structures
    vsg::BuildAccelerationStructureTraversal buildAccelStruct(_device);
    scene->accept(buildAccelStruct);
    _tlas = buildAccelStruct.tlas;

    if (!_tlas)
    {
        vsg::warn("RayTracingEngine: Failed to build TLAS - no geometry found in scene");
        return;
    }

    // Log TLAS info
    vsg::info("RayTracingEngine: TLAS created with ", _tlas->geometryInstances.size(), " geometry instances");

    vsg::info("RayTracingEngine: Acceleration structures built successfully");
}

void RayTracingEngine::buildAccelerationStructuresFromBuffers(
    const std::vector<vsg::ref_ptr<vsg::Data>>& vertexBuffers,
    const std::vector<vsg::ref_ptr<vsg::Data>>& indexBuffers,
    const std::vector<vsg::mat4>& transforms)
{
    if (!_device)
    {
        vsg::warn("RayTracingEngine: Cannot build acceleration structures - invalid device");
        return;
    }

    if (vertexBuffers.empty() || indexBuffers.empty())
    {
        vsg::warn("RayTracingEngine: No geometry data provided");
        return;
    }

    vsg::info("RayTracingEngine: Building acceleration structures from ", vertexBuffers.size(), " meshes");

    // Create TLAS
    _tlas = vsg::TopLevelAccelerationStructure::create(_device);

    // Build BLAS for each mesh
    for (size_t i = 0; i < vertexBuffers.size() && i < indexBuffers.size(); ++i)
    {
        auto vertices = vertexBuffers[i];
        auto indices = indexBuffers[i];

        if (!vertices || !indices)
        {
            vsg::warn("RayTracingEngine: Skipping invalid mesh ", i);
            continue;
        }

        // Create acceleration geometry
        auto accelGeometry = vsg::AccelerationGeometry::create();
        accelGeometry->assignVertices(vertices);
        accelGeometry->assignIndices(indices);

        // Create BLAS
        auto blas = vsg::BottomLevelAccelerationStructure::create(_device);
        blas->geometries.push_back(accelGeometry);

        // Create geometry instance
        auto geomInstance = vsg::GeometryInstance::create();
        geomInstance->accelerationStructure = blas;

        // Set transform
        if (i < transforms.size())
        {
            geomInstance->transform = transforms[i];
        }
        else
        {
            geomInstance->transform = vsg::mat4(); // Identity
        }

        _tlas->geometryInstances.push_back(geomInstance);
    }

    vsg::info("RayTracingEngine: TLAS created with ", _tlas->geometryInstances.size(), " geometry instances");
    vsg::info("RayTracingEngine: Acceleration structures built successfully");
}

vsg::ref_ptr<vsg::Commands> RayTracingEngine::createCommandGraph()
{
    if (!_device || !_tlas || !rtPipeline)
    {
        vsg::warn("RayTracingEngine: Cannot create command graph - not initialized");
        return {};
    }

    // Create camera uniform (will be updated per frame)
    rtUniform = RayTracingUniformValue::create();
    rtUniform->properties.dataVariance = vsg::DataVariance::DYNAMIC_DATA;
    rtUniform->value().projInverse = vsg::mat4(); // Will be set by caller
    rtUniform->value().viewInverse = vsg::mat4();

    // Create storage image info
    auto storageImageInfo = vsg::ImageInfo::create(vsg::ref_ptr<vsg::Sampler>{}, rtStorageImageView, VK_IMAGE_LAYOUT_GENERAL);

    // Create descriptors
    auto accelDescriptor = vsg::DescriptorAccelerationStructure::create(vsg::AccelerationStructures{_tlas}, 0, 0);
    auto storageImageDescriptor = vsg::DescriptorImage::create(storageImageInfo, 1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    auto raytracingUniformDescriptor = vsg::DescriptorBuffer::create(rtUniform, 2, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    // Create descriptor set
    auto descriptorSetLayout = rtPipelineLayout->setLayouts.front();
    rtDescriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{accelDescriptor, storageImageDescriptor, raytracingUniformDescriptor});
    auto bindDescriptorSets = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipelineLayout, 0, vsg::DescriptorSets{rtDescriptorSet});

    // Create trace rays command (use saved shader groups from initialize())
    auto traceRays = vsg::TraceRays::create();
    traceRays->raygen = raygenShaderGroup;
    traceRays->missShader = missShaderGroup;
    traceRays->hitShader = closestHitShaderGroup;
    traceRays->width = _width;
    traceRays->height = _height;
    traceRays->depth = 1;

    // Build command graph
    auto scenegraph = vsg::Commands::create();
    scenegraph->addChild(bindRTPipeline);
    scenegraph->addChild(bindDescriptorSets);
    scenegraph->addChild(traceRays);

    vsg::info("RayTracingEngine: Command graph created");

    return scenegraph;
}

void RayTracingEngine::updateCamera(const vsg::mat4& viewMatrix, const vsg::mat4& projMatrix)
{
    if (!rtUniform)
    {
        return;
    }

    // Compute inverse matrices for ray generation
    rtUniform->value().viewInverse = vsg::inverse(viewMatrix);
    rtUniform->value().projInverse = vsg::inverse(projMatrix);
    rtUniform->dirty();
}
