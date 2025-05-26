#pragma once

#include <vsg/all.h>
#include "MyMask.h"
#include "CustomViewDependentState.h"

#ifndef CUSTOMVIEWDEPENDENTSTATE1_H
#define CUSTOMVIEWDEPENDENTSTATE1_H

class CustomViewDependentState1 : public vsg::Inherit<vsg::ViewDependentState, CustomViewDependentState1>
{
public:
    vsg::ref_ptr<vsg::mat4Array> viewMatrixData;
    vsg::ref_ptr<vsg::BufferInfo> viewMatrixDataBufferInfo;
    vsg::dbox scene_bound_ws_virtual;
    vsg::dbox scene_bound_ws_real;

    CustomViewDependentState1(vsg::View* in_view) :
        vsg::Inherit<vsg::ViewDependentState, CustomViewDependentState1>(in_view){}

    // to override descriptorset layout
    virtual void init(vsg::ResourceRequirements& requirements) override;

    // to save viewMatrix and inverseViewMatrix
    void traverse(vsg::RecordTraversal& rt) const override;

    // virtual void compile(vsg::Context& context) override;
    vsg::ref_ptr<vsg::Image> createCustomShadowImage(uint32_t width, uint32_t height, uint32_t levels, VkFormat format, VkImageUsageFlags usage);
    // to actually bind the descriptor set
    //virtual void bindDescriptorSets(vsg::CommandBuffer& commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet) override;
};

#endif // CUSTOMVIEWDEPENDENTSTATE1_H