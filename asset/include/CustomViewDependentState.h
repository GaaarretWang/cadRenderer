#pragma once

#include <vsg/all.h>
#include "MyMask.h"

#ifndef CUSTOMVIEWDEPENDENTSTATE_H
#define CUSTOMVIEWDEPENDENTSTATE_H

class CustomViewDependentState : public vsg::Inherit<vsg::ViewDependentState, CustomViewDependentState>
{
public:
    vsg::ref_ptr<vsg::mat4Array> viewMatrixData;
    vsg::ref_ptr<vsg::BufferInfo> viewMatrixDataBufferInfo;
    vsg::dbox scene_bound_ws_virtual;
    vsg::dbox scene_bound_ws_real;

    CustomViewDependentState(vsg::View* in_view) :
        vsg::Inherit<vsg::ViewDependentState, CustomViewDependentState>(in_view){}

    // to override descriptorset layout
    virtual void init(vsg::ResourceRequirements& requirements) override;

    // to save viewMatrix and inverseViewMatrix
    void traverse(vsg::RecordTraversal& rt) const override;

    // to actually bind the descriptor set
    //virtual void bindDescriptorSets(vsg::CommandBuffer& commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet) override;
};

#endif // CUSTOMVIEWDEPENDENTSTATE_H