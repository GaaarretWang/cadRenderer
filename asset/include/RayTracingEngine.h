#pragma once

#include <vsg/all.h>

// Ray tracing uniform structure
struct RayTracingUniform
{
    vsg::mat4 viewInverse;
    vsg::mat4 projInverse;
};

class RayTracingEngine : public vsg::Inherit<vsg::Object, RayTracingEngine>
{
public:
    // Initialize ray tracing pipeline and resources
    void initialize(vsg::ref_ptr<vsg::Device> device,
                   vsg::ref_ptr<vsg::Window> window);

    // Build acceleration structures from scene geometry
    void buildAccelerationStructures(vsg::ref_ptr<vsg::Node> scene);

    // Build acceleration structures from vertex/index buffers directly
    void buildAccelerationStructuresFromBuffers(
        const std::vector<vsg::ref_ptr<vsg::Data>>& vertexBuffers,
        const std::vector<vsg::ref_ptr<vsg::Data>>& indexBuffers,
        const std::vector<vsg::mat4>& transforms);

    // Create command graph for ray tracing
    vsg::ref_ptr<vsg::Commands> createCommandGraph();

    // Get ray tracing output image
    vsg::ref_ptr<vsg::Image> getRTImage() const { return rtStorageImage; }

    // Get ray tracing output image view
    vsg::ref_ptr<vsg::ImageView> getRTImageView() const { return rtStorageImageView; }

    // Update camera matrices for ray tracing
    void updateCamera(const vsg::mat4& viewMatrix, const vsg::mat4& projMatrix);

private:
    vsg::ref_ptr<vsg::Device> _device;
    vsg::ref_ptr<vsg::TopLevelAccelerationStructure> _tlas;
    vsg::ref_ptr<vsg::Image> rtStorageImage;
    vsg::ref_ptr<vsg::ImageView> rtStorageImageView;
    vsg::ref_ptr<vsg::RayTracingPipeline> rtPipeline;
    vsg::ref_ptr<vsg::BindRayTracingPipeline> bindRTPipeline;
    vsg::ref_ptr<vsg::DescriptorSet> rtDescriptorSet;
    vsg::ref_ptr<vsg::PipelineLayout> rtPipelineLayout;
    vsg::ref_ptr<vsg::Value<RayTracingUniform>> rtUniform;

    // Shader groups (must be saved to maintain shader binding table info)
    vsg::ref_ptr<vsg::RayTracingShaderGroup> raygenShaderGroup;
    vsg::ref_ptr<vsg::RayTracingShaderGroup> missShaderGroup;
    vsg::ref_ptr<vsg::RayTracingShaderGroup> closestHitShaderGroup;

    uint32_t _width, _height;
};
