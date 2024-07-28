#pragma once 

#include <vsg/all.h>

namespace vsgExt
{
    using namespace vsg;
    /// ArrayConfigurator utility provides a means of setting up arrays using ShaderSet as a guide for required bindings.
    class CustomArrayConfigurator : public vsg::Inherit<vsg::ArrayConfigurator, CustomArrayConfigurator>
    {
    public:
        CustomArrayConfigurator(vsg::ref_ptr<ShaderSet> in_shaderSet = {});
        bool assignArray(const std::string& name, VkVertexInputRate vertexInputRate, vsg::ref_ptr<Data> array, uint32_t offset);
    };

    class CustomGraphicsPipelineConfigurator : public vsg::Inherit<vsg::GraphicsPipelineConfigurator, CustomGraphicsPipelineConfigurator>
    {
    public:
        CustomGraphicsPipelineConfigurator(vsg::ref_ptr<ShaderSet> in_shaderSet = {});
        bool assignArray(DataList& arrays, const std::string& name, VkVertexInputRate vertexInputRate, vsg::ref_ptr<Data> array, uint32_t offset);
    };
} // namespace vsg

EVSG_type_name(vsgExt::CustomArrayConfigurator);
EVSG_type_name(vsgExt::CustomGraphicsPipelineConfigurator);