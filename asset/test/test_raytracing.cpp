// Minimal Ray Tracing Test Program
// Based on vsgExamples/vsgraytracing.cpp
// Purpose: Verify RT configuration with a single triangle

#include <vsg/all.h>
#include <iostream>

struct RayTracingUniform
{
    vsg::mat4 viewInverse;
    vsg::mat4 projInverse;
};

using RayTracingUniformValue = vsg::Value<RayTracingUniform>;

int main(int argc, char** argv)
{
    std::cout << "\n=== Ray Tracing Minimal Test ===" << std::endl;

    // Command line options
    vsg::CommandLine arguments(&argc, argv);
    auto windowTraits = vsg::WindowTraits::create();
    windowTraits->windowTitle = "RT Test - Single Triangle";
    windowTraits->width = 800;
    windowTraits->height = 600;
    windowTraits->debugLayer = arguments.read({"--debug", "-d"});
    windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});

    // Critical RT configuration from vsgExamples
    windowTraits->queueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
    windowTraits->imageAvailableSemaphoreWaitFlag = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    windowTraits->swapchainPreferences.imageUsage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    windowTraits->vulkanVersion = VK_API_VERSION_1_1;

    // Device extensions (8 extensions - complete list from vsgExamples)
    windowTraits->deviceExtensionNames = {
        VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_KHR_SPIRV_1_4_EXTENSION_NAME,
        VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME
    };

    // Device features (3 feature structures)
    auto features = windowTraits->deviceFeatures = vsg::DeviceFeatures::create();
    features->get();

    auto& deviceAddressFeatures = features->get<VkPhysicalDeviceBufferDeviceAddressFeatures,
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES>();
    deviceAddressFeatures.bufferDeviceAddress = VK_TRUE;

    auto& rayTracingFeatures = features->get<VkPhysicalDeviceRayTracingPipelineFeaturesKHR,
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR>();
    rayTracingFeatures.rayTracingPipeline = VK_TRUE;

    auto& accelerationFeatures = features->get<VkPhysicalDeviceAccelerationStructureFeaturesKHR,
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR>();
    accelerationFeatures.accelerationStructure = VK_TRUE;

    std::cout << "✓ Window traits configured" << std::endl;

    // Create viewer and window
    auto viewer = vsg::Viewer::create();
    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cerr << "✗ Failed to create window" << std::endl;
        return 1;
    }
    viewer->addWindow(window);
    std::cout << "✓ Window created" << std::endl;

    // Get device
    vsg::ref_ptr<vsg::Device> device;
    try
    {
        device = window->getOrCreateDevice();
    }
    catch (const vsg::Exception& exception)
    {
        std::cerr << "✗ Device creation failed: " << exception.message << std::endl;
        return 1;
    }
    std::cout << "✓ Device obtained" << std::endl;

    // Fixed triangle geometry
    auto vertices = vsg::vec3Array::create({
        {-0.5f, -0.5f, 0.0f},
        { 0.5f, -0.5f, 0.0f},
        { 0.0f,  0.5f, 0.0f}
    });

    auto indices = vsg::uintArray::create({0, 1, 2});

    // Create acceleration geometry
    auto accelGeometry = vsg::AccelerationGeometry::create();
    accelGeometry->assignVertices(vertices);
    accelGeometry->assignIndices(indices);

    // Create BLAS (Bottom Level Acceleration Structure)
    auto blas = vsg::BottomLevelAccelerationStructure::create(device);
    blas->geometries.push_back(accelGeometry);

    // Create TLAS (Top Level Acceleration Structure)
    auto tlas = vsg::TopLevelAccelerationStructure::create(device);

    // Add geometry instance to TLAS
    auto geomInstance = vsg::GeometryInstance::create();
    geomInstance->accelerationStructure = blas;
    geomInstance->transform = vsg::mat4();  // Identity matrix

    tlas->geometryInstances.push_back(geomInstance);

    std::cout << "✓ Acceleration structures created" << std::endl;

    // Create compile context for storage image
    auto context = vsg::Context::create(device);

    // Create storage image (B8G8R8A8_UNORM)
    auto storageImage = vsg::Image::create();
    storageImage->imageType = VK_IMAGE_TYPE_2D;
    storageImage->format = VK_FORMAT_B8G8R8A8_UNORM;
    storageImage->extent.width = 800;
    storageImage->extent.height = 600;
    storageImage->extent.depth = 1;
    storageImage->mipLevels = 1;
    storageImage->arrayLayers = 1;
    storageImage->samples = VK_SAMPLE_COUNT_1_BIT;
    storageImage->tiling = VK_IMAGE_TILING_OPTIMAL;
    storageImage->usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    storageImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    storageImage->flags = 0;
    storageImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    auto storageImageInfo = vsg::ImageInfo::create(
        vsg::ref_ptr<vsg::Sampler>{},
        createImageView(*context, storageImage, VK_IMAGE_ASPECT_COLOR_BIT),
        VK_IMAGE_LAYOUT_GENERAL
    );

    std::cout << "✓ Storage image created" << std::endl;

    // Load shaders
    auto raygenShader = vsg::ShaderStage::read(VK_SHADER_STAGE_RAYGEN_BIT_KHR,
        "main", "/home/wanggaoyuan/cadRenderer/cadRenderer/asset/data/shaders/rt_simple.rgen.spv");
    auto missShader = vsg::ShaderStage::read(VK_SHADER_STAGE_MISS_BIT_KHR,
        "main", "/home/wanggaoyuan/cadRenderer/cadRenderer/asset/data/shaders/rt_simple.rmiss.spv");
    auto closesthitShader = vsg::ShaderStage::read(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        "main", "/home/wanggaoyuan/cadRenderer/cadRenderer/asset/data/shaders/rt_simple.rchit.spv");

    if (!raygenShader || !missShader || !closesthitShader)
    {
        std::cerr << "✗ Failed to load shaders" << std::endl;
        return 1;
    }
    std::cout << "✓ Shaders loaded" << std::endl;

    auto shaderStages = vsg::ShaderStages{raygenShader, missShader, closesthitShader};

    // Set up shader groups
    auto raygenShaderGroup = vsg::RayTracingShaderGroup::create();
    raygenShaderGroup->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    raygenShaderGroup->generalShader = 0;

    auto missShaderGroup = vsg::RayTracingShaderGroup::create();
    missShaderGroup->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    missShaderGroup->generalShader = 1;

    auto closestHitShaderGroup = vsg::RayTracingShaderGroup::create();
    closestHitShaderGroup->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    closestHitShaderGroup->closestHitShader = 2;

    auto shaderGroups = vsg::RayTracingShaderGroups{raygenShaderGroup, missShaderGroup, closestHitShaderGroup};

    std::cout << "✓ Shader groups created" << std::endl;

    // Camera matrices and uniform
    auto perspective = vsg::Perspective::create(60.0, 800.0 / 600.0, 0.1, 10.0);
    auto lookAt = vsg::LookAt::create(
        vsg::dvec3(0.0, 0.0, -2.5),
        vsg::dvec3(0.0, 0.0, 0.0),
        vsg::dvec3(0.0, 1.0, 0.0)
    );

    vsg::ref_ptr<RayTracingUniformValue> raytracingUniform = RayTracingUniformValue::create();
    raytracingUniform->properties.dataVariance = vsg::DataVariance::DYNAMIC_DATA;
    raytracingUniform->value().projInverse = perspective->inverse();
    raytracingUniform->value().viewInverse = lookAt->inverse();

    std::cout << "✓ Camera uniform initialized" << std::endl;

    // Set up descriptor bindings
    vsg::DescriptorSetLayoutBindings descriptorBindings{
        {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},
        {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr}
    };

    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

    // Create descriptors
    auto accelDescriptor = vsg::DescriptorAccelerationStructure::create(
        vsg::AccelerationStructures{tlas}, 0, 0
    );

    auto storageImageDescriptor = vsg::DescriptorImage::create(
        storageImageInfo, 1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
    );

    auto raytracingUniformDescriptor = vsg::DescriptorBuffer::create(
        raytracingUniform, 2, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
    );

    auto pipelineLayout = vsg::PipelineLayout::create(
        vsg::DescriptorSetLayouts{descriptorSetLayout},
        vsg::PushConstantRanges{}
    );

    auto raytracingPipeline = vsg::RayTracingPipeline::create(pipelineLayout, shaderStages, shaderGroups);
    auto bindRayTracingPipeline = vsg::BindRayTracingPipeline::create(raytracingPipeline);

    auto descriptorSet = vsg::DescriptorSet::create(
        descriptorSetLayout,
        vsg::Descriptors{accelDescriptor, storageImageDescriptor, raytracingUniformDescriptor}
    );

    auto bindDescriptorSets = vsg::BindDescriptorSets::create(
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        raytracingPipeline->getPipelineLayout(),
        0,
        vsg::DescriptorSets{descriptorSet}
    );

    std::cout << "✓ RT pipeline and descriptors created" << std::endl;

    // Create scene graph
    auto scenegraph = vsg::Commands::create();
    scenegraph->addChild(bindRayTracingPipeline);
    scenegraph->addChild(bindDescriptorSets);

    // Setup tracing of rays
    auto traceRays = vsg::TraceRays::create();
    traceRays->raygen = raygenShaderGroup;
    traceRays->missShader = missShaderGroup;
    traceRays->hitShader = closestHitShaderGroup;
    traceRays->width = 800;
    traceRays->height = 600;
    traceRays->depth = 1;

    scenegraph->addChild(traceRays);

    std::cout << "✓ TraceRays command created" << std::endl;

    // Create command graph
    auto commandGraph = vsg::CommandGraph::create(window);
    auto copyImageViewToWindow = vsg::CopyImageViewToWindow::create(storageImageInfo->imageView, window);

    commandGraph->addChild(scenegraph);
    commandGraph->addChild(copyImageViewToWindow);

    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    std::cout << "✓ Command graph created" << std::endl;

    // Compile all resources
    viewer->compile();
    std::cout << "✓ Viewer compiled" << std::endl;

    // Render loop
    std::cout << "\n=== Starting render loop ===" << std::endl;
    int frameCount = 0;
    bool firstFrameSuccess = false;

    while (viewer->advanceToNextFrame() && frameCount < 100)
    {
        viewer->handleEvents();

        // Update camera matrix
        raytracingUniform->value().viewInverse = lookAt->inverse();
        raytracingUniform->dirty();

        viewer->update();
        viewer->recordAndSubmit();
        viewer->present();

        if (frameCount == 0)
        {
            std::cout << "✓ First frame rendered successfully!" << std::endl;
            firstFrameSuccess = true;
        }

        frameCount++;
    }

    std::cout << "\n=== Test completed successfully! ===" << std::endl;
    std::cout << "Total frames: " << frameCount << std::endl;

    return firstFrameSuccess ? 0 : 1;
}
