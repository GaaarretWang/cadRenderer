#pragma once

#include <vsg/all.h>
#include <vsg/state/ViewDependentState.h>
#include "MyMask.h"

#ifndef CUSTOMVIEWDEPENDENTSTATE_H
#define CUSTOMVIEWDEPENDENTSTATE_H

class CustomViewDependentState : public vsg::Inherit<vsg::ViewDependentState, CustomViewDependentState>
{
public:
    static const uint32_t MAX_MATRIX_COUNT = 4;
    vsg::ref_ptr<vsg::mat4Array> viewMatrixData;
    vsg::ref_ptr<vsg::BufferInfo> viewMatrixDataBufferInfo;
    bool disableShadowMap;
    vsg::dbox scene_bound_ws;

    CustomViewDependentState(vsg::View* in_view, bool in_disableShadowMap = false) :
        vsg::Inherit<vsg::ViewDependentState, CustomViewDependentState>(in_view), disableShadowMap(in_disableShadowMap) {}

    // to override descriptorset layout
    virtual void init(vsg::ResourceRequirements& requirements) override;

    // to save viewMatrix and inverseViewMatrix
    void traverse(vsg::RecordTraversal& rt) const override;

    // to actually bind the descriptor set
    //virtual void bindDescriptorSets(vsg::CommandBuffer& commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet) override;
};

#endif // CUSTOMVIEWDEPENDENTSTATE_H