// Î´Ê¹ÓÃ

//#include <vsg/all.h>
//#include "CustomGraphicsPipelineConfigurator.h"
//
//using namespace vsg;
//namespace vsgExt
//{
//    CustomArrayConfigurator::CustomArrayConfigurator(ref_ptr<ShaderSet> in_shaderSet) :
//        Inherit(in_shaderSet) {}
//
//    bool CustomArrayConfigurator::assignArray(const std::string& name, VkVertexInputRate vertexInputRate, ref_ptr<Data> array, uint32_t offset)
//    {
//        auto& attributeBinding = shaderSet->getAttributeBinding(name);
//        if (attributeBinding)
//        {
//            assigned.insert(name);
//
//            VkFormat format = array ? array->properties.format : VK_FORMAT_UNDEFINED;
//
//            // set up bindings
//            uint32_t bindingIndex = baseAttributeBinding + static_cast<uint32_t>(arrays.size());
//            if (!attributeBinding.define.empty()) defines.insert(attributeBinding.define);
//
//            vertexAttributeDescriptions.push_back(VkVertexInputAttributeDescription{attributeBinding.location, bindingIndex, (format != VK_FORMAT_UNDEFINED) ? format : attributeBinding.format, offset});
//            vertexBindingDescriptions.push_back(VkVertexInputBindingDescription{bindingIndex, array->properties.stride, vertexInputRate});
//
//            arrays.push_back(array);
//            return true;
//        }
//        return false;
//    }
//
//    struct CustomSetPipelineStates : public Visitor
//    {
//        uint32_t base = 0;
//        const AttributeBinding& binding;
//        VkVertexInputRate vir;
//        uint32_t stride;
//        VkFormat format;
//        uint32_t offset;
//
//        CustomSetPipelineStates(uint32_t in_base, const AttributeBinding& in_binding, VkVertexInputRate in_vir, uint32_t in_stride, VkFormat in_format, uint32_t in_offset) :
//            base(in_base),
//            binding(in_binding),
//            vir(in_vir),
//            stride(in_stride),
//            format(in_format),
//            offset(in_offset) {}
//
//        void apply(Object& object) override { object.traverse(*this); }
//        void apply(VertexInputState& vis) override
//        {
//            uint32_t bindingIndex = base + static_cast<uint32_t>(vis.vertexAttributeDescriptions.size());
//            vis.vertexAttributeDescriptions.push_back(VkVertexInputAttributeDescription{binding.location, bindingIndex, (format != VK_FORMAT_UNDEFINED) ? format : binding.format, offset});
//            vis.vertexBindingDescriptions.push_back(VkVertexInputBindingDescription{bindingIndex, stride, vir});
//        }
//    };
//
//    CustomGraphicsPipelineConfigurator::CustomGraphicsPipelineConfigurator(ref_ptr<ShaderSet> in_shaderSet) :
//        Inherit(in_shaderSet)
//    {
//    }
//
//    bool CustomGraphicsPipelineConfigurator::assignArray(DataList& arrays, const std::string& name, VkVertexInputRate vertexInputRate, ref_ptr<Data> array, uint32_t offset)
//    {
//        const auto& attributeBinding = shaderSet->getAttributeBinding(name);
//        if (attributeBinding)
//        {
//            VkFormat format = array ? array->properties.format : VK_FORMAT_UNDEFINED;
//
//            CustomSetPipelineStates setVertexAttributeState(baseAttributeBinding, attributeBinding, vertexInputRate, array->properties.stride, format, offset);
//            accept(setVertexAttributeState);
//
//            arrays.push_back(array);
//            return true;
//        }
//        return false;
//    }
//} // namespace vsgExt