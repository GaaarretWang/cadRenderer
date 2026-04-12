#pragma once

#include <vsg/all.h>
#include <vector>
#include "MyMask.h"
#include "CADMesh.h"
#include "OcclusionCullingPasses.h"

#ifndef CUSTOMVIEWDEPENDENTSTATE_H
#define CUSTOMVIEWDEPENDENTSTATE_H

class CustomViewDependentState : public vsg::Inherit<vsg::ViewDependentState, CustomViewDependentState>
{
public:
    struct ShadowOcclusionResources
    {
        vsg::ref_ptr<vsg::Array<OcclusionCullingPasses::CameraPlaneInfo>> camera_plane_info;
        vsg::ref_ptr<vsg::BufferInfo> camera_plane_info_buffer_info;
        vsg::ref_ptr<vsg::mat4Array> camera_matrix;
        vsg::ref_ptr<vsg::BufferInfo> camera_matrix_buffer_info;
        vsg::ref_ptr<vsg::Image> depth_pyramid_image;
        vsg::ref_ptr<vsg::Sampler> depth_pyramid_sampler;
        vsg::ref_ptr<vsg::ImageView> depth_pyramid_image_view;
        vsg::ref_ptr<vsg::ImageInfo> depth_pyramid_image_info;
        vsg::ref_ptr<vsg::ImageInfo> shadow_depth_layer_image_info;
        vsg::ref_ptr<vsg::RenderGraph> second_pass_render_graph;
    };

    mutable vsg::dbox scene_bound_ws_virtual;
    vsg::dbox scene_bound_ws_real;
    mutable bool draw_shadow_light = true;  // Set when lighting changes.
    mutable bool draw_shadow_pose = true;   // Set when the model pose changes.
    vsg::ref_ptr<vsg::Options> options;
    vsg::ref_ptr<vsg::DescriptorImage> shadowMapSamplerImages;
    vsg::ref_ptr<vsg::Switch> shadow_pass_sequence_switch;
    std::vector<ShadowOcclusionResources> shadow_occlusion_resources;

    CustomViewDependentState(vsg::View* in_view, vsg::ref_ptr<vsg::Device> device, int computeQueueFamily, vsg::ref_ptr<vsg::Options> in_options) :
        vsg::Inherit<vsg::ViewDependentState, CustomViewDependentState>(in_view),
        options(in_options)
    {
        (void)device;
        (void)computeQueueFamily;
    }

    // to override descriptorset layout
    virtual void init(vsg::ResourceRequirements& requirements) override;
    virtual void compile(vsg::Context& context) override;

    // to save viewMatrix and inverseViewMatrix
    void traverse(vsg::RecordTraversal& rt) const override;
    vsg::ref_ptr<vsg::Image> createCustomShadowImage(uint32_t width, uint32_t height, uint32_t levels, VkFormat format, VkImageUsageFlags usage);

    // to actually bind the descriptor set
    //virtual void bindDescriptorSets(vsg::CommandBuffer& commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet) override;
};

#endif // CUSTOMVIEWDEPENDENTSTATE_H
