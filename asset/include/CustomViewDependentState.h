#pragma once

#include <vsg/all.h>
#include "MyMask.h"
#include "CADMesh.h"

#ifndef CUSTOMVIEWDEPENDENTSTATE_H
#define CUSTOMVIEWDEPENDENTSTATE_H

class CustomViewDependentState : public vsg::Inherit<vsg::ViewDependentState, CustomViewDependentState>
{
public:
    mutable vsg::dbox scene_bound_ws_virtual;
    vsg::dbox scene_bound_ws_real;
    mutable bool draw_shadow_light = true;  // Set when lighting changes.
    mutable bool draw_shadow_pose = true;   // Set when the model pose changes.
    vsg::ref_ptr<vsg::CommandGraph> computeCommandGraphShadow;
    vsg::ref_ptr<vsg::Options> options;
    vsg::ref_ptr<vsg::DescriptorImage> shadowMapSamplerImages;

    CustomViewDependentState(vsg::View* in_view, vsg::ref_ptr<vsg::Device> device, int computeQueueFamily, vsg::ref_ptr<vsg::Options> in_options) :
        vsg::Inherit<vsg::ViewDependentState, CustomViewDependentState>(in_view), computeCommandGraphShadow(vsg::CommandGraph::create(device, computeQueueFamily)), options(in_options){}

    // to override descriptorset layout
    virtual void init(vsg::ResourceRequirements& requirements) override;

    // to save viewMatrix and inverseViewMatrix
    void traverse(vsg::RecordTraversal& rt) const override;
    vsg::ref_ptr<vsg::Image> createCustomShadowImage(uint32_t width, uint32_t height, uint32_t levels, VkFormat format, VkImageUsageFlags usage);

    // to actually bind the descriptor set
    //virtual void bindDescriptorSets(vsg::CommandBuffer& commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet) override;
};

#endif // CUSTOMVIEWDEPENDENTSTATE_H
