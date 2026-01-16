#pragma once

#include <vsg/all.h>
#include "MyMask.h"
#include "CADMesh.h"

#ifndef CUSTOMVIEWDEPENDENTSTATE_H
#define CUSTOMVIEWDEPENDENTSTATE_H

class CustomViewDependentState : public vsg::Inherit<vsg::ViewDependentState, CustomViewDependentState>
{
public:
    vsg::dbox scene_bound_ws_virtual;
    vsg::dbox scene_bound_ws_real;
    vsg::ref_ptr<vsg::CommandGraph> computeCommandGraphShadow;
    std::string project_path;
    vsg::ref_ptr<vsg::DescriptorImage> shadowMapSamplerImages;

    CustomViewDependentState(vsg::View* in_view, vsg::ref_ptr<vsg::Device> device, int computeQueueFamily, std::string in_project_path) :
        vsg::Inherit<vsg::ViewDependentState, CustomViewDependentState>(in_view), computeCommandGraphShadow(vsg::CommandGraph::create(device, computeQueueFamily)), project_path(in_project_path){}

    // to override descriptorset layout
    virtual void init(vsg::ResourceRequirements& requirements) override;

    // to save viewMatrix and inverseViewMatrix
    void traverse(vsg::RecordTraversal& rt) const override;
    vsg::ref_ptr<vsg::Image> createCustomShadowImage(uint32_t width, uint32_t height, uint32_t levels, VkFormat format, VkImageUsageFlags usage);

    // to actually bind the descriptor set
    //virtual void bindDescriptorSets(vsg::CommandBuffer& commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet) override;
};

#endif // CUSTOMVIEWDEPENDENTSTATE_H