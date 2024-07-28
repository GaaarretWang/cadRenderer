/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/app/View.h>
#include <vsg/commands/BindIndexBuffer.h>
#include <vsg/commands/BindVertexBuffers.h>
#include <vsg/nodes/Bin.h>
#include <vsg/nodes/DepthSorted.h>
#include <vsg/nodes/Geometry.h>
#include <vsg/nodes/Group.h>
#include <vsg/nodes/PagedLOD.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/nodes/VertexDraw.h>
#include <vsg/nodes/VertexIndexDraw.h>
#include <vsg/state/DescriptorImage.h>
#include <vsg/state/MultisampleState.h>
#include <vsg/state/ViewDependentState.h>
#include <vsg/vk/Context.h>
#include <vsg/vk/RenderPass.h>
#include <vsg/vk/ResourceRequirements.h>

using namespace vsg;

/////////////////////////////////////////////////////////////////////
//
// ResourceRequirements
//
ResourceRequirements::ResourceRequirements(ref_ptr<ResourceHints> hints)
{
    viewDetailsStack.push(ResourceRequirements::ViewDetails{});
    if (hints) apply(*hints);
}

uint32_t ResourceRequirements::computeNumDescriptorSets() const
{
    return externalNumDescriptorSets + static_cast<uint32_t>(descriptorSets.size());
}

DescriptorPoolSizes ResourceRequirements::computeDescriptorPoolSizes() const
{
    DescriptorPoolSizes poolSizes;
    for (auto& [type, count] : descriptorTypeMap)
    {
        poolSizes.push_back(VkDescriptorPoolSize{type, count});
    }
    return poolSizes;
}

void ResourceRequirements::apply(const ResourceHints& resourceHints)
{
    if (resourceHints.maxSlot > maxSlot) maxSlot = resourceHints.maxSlot;

    if (!resourceHints.descriptorPoolSizes.empty() || resourceHints.numDescriptorSets > 0)
    {
        externalNumDescriptorSets += resourceHints.numDescriptorSets;

        for (auto& [type, count] : resourceHints.descriptorPoolSizes)
        {
            descriptorTypeMap[type] += count;
        }
    }

    numLightsRange = resourceHints.numLightsRange;
    numShadowMapsRange = resourceHints.numShadowMapsRange;
    shadowMapSize = resourceHints.shadowMapSize;
}

//////////////////////////////////////////////////////////////////////
//
// CollectResourceRequirements
//
ref_ptr<ResourceHints> CollectResourceRequirements::createResourceHints(uint32_t tileMultiplier) const
{
    auto resourceHints = vsg::ResourceHints::create();

    resourceHints->maxSlot = requirements.maxSlot;
    resourceHints->numDescriptorSets = static_cast<uint32_t>(requirements.computeNumDescriptorSets() * tileMultiplier);
    resourceHints->descriptorPoolSizes = requirements.computeDescriptorPoolSizes();

    for (auto& poolSize : resourceHints->descriptorPoolSizes)
    {
        poolSize.descriptorCount = poolSize.descriptorCount * tileMultiplier;
    }

    return resourceHints;
}

void CollectResourceRequirements::apply(const Object& object)
{
    object.traverse(*this);
}

bool CollectResourceRequirements::checkForResourceHints(const Object& object)
{
    auto resourceHints = object.getObject<ResourceHints>("ResourceHints");
    if (resourceHints)
    {
        apply(*resourceHints);
        return true;
    }
    else
    {
        return false;
    }
}

void CollectResourceRequirements::apply(const ResourceHints& resourceHints)
{
    requirements.apply(resourceHints);
}

void CollectResourceRequirements::apply(const Node& node)
{
    bool hasResourceHints = checkForResourceHints(node);
    if (hasResourceHints) ++_numResourceHintsAbove;

    node.traverse(*this);

    if (hasResourceHints) --_numResourceHintsAbove;
}

void CollectResourceRequirements::apply(const StateGroup& stategroup)
{
    bool hasResourceHints = checkForResourceHints(stategroup);
    if (hasResourceHints) ++_numResourceHintsAbove;

    if (_numResourceHintsAbove == 0)
    {
        for (auto& command : stategroup.stateCommands)
        {
            command->accept(*this);
        }
    }

    stategroup.traverse(*this);

    if (hasResourceHints) --_numResourceHintsAbove;
}

void CollectResourceRequirements::apply(const PagedLOD& plod)
{
    bool hasResourceHints = checkForResourceHints(plod);
    if (hasResourceHints) ++_numResourceHintsAbove;

    requirements.containsPagedLOD = true;
    plod.traverse(*this);

    if (hasResourceHints) --_numResourceHintsAbove;
}

void CollectResourceRequirements::apply(const StateCommand& stateCommand)
{
    if (stateCommand.slot > requirements.maxSlot) requirements.maxSlot = stateCommand.slot;

    stateCommand.traverse(*this);
}

void CollectResourceRequirements::apply(const DescriptorSet& descriptorSet)
{
    if (requirements.descriptorSets.count(&descriptorSet) == 0)
    {
        requirements.descriptorSets.insert(&descriptorSet);

        descriptorSet.traverse(*this);
    }
}

bool CollectResourceRequirements::registerDescriptor(const Descriptor& descriptor)
{
    requirements.descriptorTypeMap[descriptor.descriptorType] += descriptor.getNumDescriptors();

    if (requirements.descriptors.count(&descriptor) == 0)
    {
        requirements.descriptors.insert(&descriptor);
        return true;
    }
    else
    {
        return false;
    }
}

void CollectResourceRequirements::apply(const Descriptor& descriptor)
{
    registerDescriptor(descriptor);
}

void CollectResourceRequirements::apply(const DescriptorBuffer& descriptorBuffer)
{
    if (registerDescriptor(descriptorBuffer))
    {
        //info("CollectResourceRequirements::apply(const DescriptorBuffer& descriptorBuffer) ", &descriptorBuffer);
        for (auto& bufferInfo : descriptorBuffer.bufferInfoList) apply(bufferInfo);
    }
}

void CollectResourceRequirements::apply(const DescriptorImage& descriptorImage)
{
    if (registerDescriptor(descriptorImage))
    {
        //info("CollectResourceRequirements::apply(const DescriptorImage& descriptorImage) ", &descriptorImage);
        for (auto& imageInfo : descriptorImage.imageInfoList) apply(imageInfo);
    }
}

void CollectResourceRequirements::apply(const Light& light)
{
    requirements.viewDetailsStack.top().lights.insert(&light);
}

void CollectResourceRequirements::apply(const View& view)
{

    if (auto itr = requirements.views.find(&view); itr != requirements.views.end())
    {
        requirements.viewDetailsStack.push(itr->second);
    }
    else
    {
        requirements.viewDetailsStack.push(ResourceRequirements::ViewDetails{});
    }

    view.traverse(*this);

    auto& viewDetails = requirements.viewDetailsStack.top();

    for (auto& bin : view.bins)
    {
        viewDetails.bins.insert(bin);
    }

    requirements.views[&view] = viewDetails;

    if (view.viewDependentState)
    {
        if (requirements.maxSlot < 2) requirements.maxSlot = 2;

        view.viewDependentState->init(requirements);

        view.viewDependentState->accept(*this);
    }

    requirements.viewDetailsStack.pop();
}

void CollectResourceRequirements::apply(const DepthSorted& depthSorted)
{
    requirements.viewDetailsStack.top().indices.insert(depthSorted.binNumber);

    depthSorted.traverse(*this);
}

void CollectResourceRequirements::apply(const Bin& bin)
{
    requirements.viewDetailsStack.top().bins.insert(&bin);
}

void CollectResourceRequirements::apply(const Geometry& geometry)
{
    for (auto& bufferInfo : geometry.arrays) apply(bufferInfo);
    apply(geometry.indices);
}

void CollectResourceRequirements::apply(const VertexDraw& vd)
{
    for (auto& bufferInfo : vd.arrays) apply(bufferInfo);
}

void CollectResourceRequirements::apply(const VertexIndexDraw& vid)
{
    for (auto& bufferInfo : vid.arrays) apply(bufferInfo);
    apply(vid.indices);
}

void CollectResourceRequirements::apply(const BindVertexBuffers& bvb)
{
    for (auto& bufferInfo : bvb.arrays) apply(bufferInfo);
}

void CollectResourceRequirements::apply(const BindIndexBuffer& bib)
{
    apply(bib.indices);
}

void CollectResourceRequirements::apply(ref_ptr<BufferInfo> bufferInfo)
{
    if (bufferInfo && bufferInfo->data && bufferInfo->data->dynamic())
    {
        if (bufferInfo->data->properties.dataVariance == DYNAMIC_DATA)
        {
            requirements.earlyDynamicData.bufferInfos.push_back(bufferInfo);
        }
        else // DYNAMIC_DATA_TRANSFER_AFTER_RECORD)
        {
            requirements.lateDynamicData.bufferInfos.push_back(bufferInfo);
        }
    }
}

void CollectResourceRequirements::apply(ref_ptr<ImageInfo> imageInfo)
{
    if (imageInfo && imageInfo->imageView && imageInfo->imageView->image)
    {
        // check for dynamic data
        auto& data = imageInfo->imageView->image->data;
        if (data && data->dynamic())
        {
            if (data->properties.dataVariance == DYNAMIC_DATA)
            {
                requirements.earlyDynamicData.imageInfos.push_back(imageInfo);
            }
            else // DYNAMIC_DATA_TRANSFER_AFTER_RECORD)
            {
                requirements.lateDynamicData.imageInfos.push_back(imageInfo);
            }
        }
    }
}
