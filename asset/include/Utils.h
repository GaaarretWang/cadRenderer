#ifndef UTILS_H
#define UTILS_H

#include "vsg/all.h"

namespace Utils{
    inline vsg::ref_ptr<vsg::Sampler> createLinearSampler()
    {
        static vsg::ref_ptr<vsg::Sampler> sampler = []() {
            auto s = vsg::Sampler::create();
            
            s->magFilter = VK_FILTER_LINEAR;
            s->minFilter = VK_FILTER_LINEAR;
            s->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            
            s->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            s->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            s->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            
            return s;
        }();

        return sampler;
    }

    inline vsg::ref_ptr<vsg::Sampler> createNearestSampler()
    {
        static vsg::ref_ptr<vsg::Sampler> sampler = []() {
            auto s = vsg::Sampler::create();
            
            s->magFilter = VK_FILTER_NEAREST;
            s->minFilter = VK_FILTER_NEAREST;
            s->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            
            s->addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            s->addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            s->addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            
            return s;
        }();

        return sampler;
    }

    inline void BuildClearCommandGraph(vsg::ref_ptr<vsg::CommandGraph> clear_image_commandgraph, VkExtent2D extent, vsg::ref_ptr<vsg::Window> window, VkSampleCountFlagBits msaaSamples){
        vsg::ref_ptr<vsg::ClearDepthStencilImage> clearDepth = vsg::ClearDepthStencilImage::create();
        vsg::ref_ptr<vsg::ClearDepthStencilImage> clearDepth1 = vsg::ClearDepthStencilImage::create();
        
        clearDepth->imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; // 必须为 TRANSFER_DST_OPTIMAL 或 GENERAL
        clearDepth->depthStencil = {0.0f, 0};
        VkImageSubresourceRange range{};
        range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = 0;
        range.layerCount = 1;
        clearDepth->ranges = {range};

        clearDepth1->imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; // 必须为 TRANSFER_DST_OPTIMAL 或 GENERAL
        clearDepth1->depthStencil = {0.0f, 0};
        clearDepth1->ranges = {range};
        if(msaaSamples != VK_SAMPLE_COUNT_1_BIT)
            clearDepth->image = window->_multisampleDepthImage;
        clearDepth1->image = window->_depthImage;

        if(msaaSamples != VK_SAMPLE_COUNT_1_BIT)
            clear_image_commandgraph->addChild(clearDepth);
        clear_image_commandgraph->addChild(clearDepth1);


        VkImageSubresourceRange range0{};
        range0.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // 颜色附件
        range0.baseMipLevel = 0;
        range0.levelCount = 1;
        range0.baseArrayLayer = 0;
        range0.layerCount = 1;

        auto clearColor0 = vsg::ClearColorImage::create();
        clearColor0->image = window->_GBufferImage0;
        clearColor0->imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; // 符合要求的布局
        clearColor0->color = {0.0f, 0.0f, 0.0f, 1.0f}; // 清除颜色：黑色（RGBA）
        clearColor0->ranges = {range0};

        auto clearColor1 = vsg::ClearColorImage::create();
        clearColor1->image = window->_GBufferImage1;
        clearColor1->imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; // 符合要求的布局
        clearColor1->color = {0.0f, 0.0f, 0.0f, 1.0f}; // 清除颜色：默认法线（0,0,1）映射后的值
        clearColor1->ranges = {range0};

        auto clearColor2 = vsg::ClearColorImage::create();
        clearColor2->image = window->_GBufferImage2;
        clearColor2->imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; // 符合要求的布局
        clearColor2->color = {0.0f, 0.0f, 0.0f, 1.0f}; // 清除颜色：默认法线（0,0,1）映射后的值
        clearColor2->ranges = {range0};

        clear_image_commandgraph->addChild(clearColor0);
        clear_image_commandgraph->addChild(clearColor1);
        clear_image_commandgraph->addChild(clearColor2);

        // auto clearCmd = vsg::ClearAttachments::create();
        // VkClearRect clearRect{};
        // clearRect.rect = {
        //     0,          // 左上角x
        //     0,          // 左上角y
        //     extent.width,      // 宽度
        //     extent.height      // 高度
        // };
        // clearRect.baseArrayLayer = 0;    // 起始图层
        // clearRect.layerCount = 1;       // 图层数量

        // for (uint32_t i = 1; i < 4; ++i) // 限制最多3张
        // {
        //     VkClearAttachment attachment{};
        //     attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // 颜色附件
        //     attachment.colorAttachment = i;                    // 附件索引（0/1/2）
        //     attachment.clearValue.color = {0, 0, 0, 0};      // 该附件的清除颜色
        //     clearCmd->attachments.push_back(attachment);
        //     clearCmd->rects.push_back(clearRect);
        // }

        // clear_image_commandgraph->addChild(clearCmd);
        // 3. 添加到命令图（和原有深度清除逻辑一致）

    }
}

#endif // UTILS_H