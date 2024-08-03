#pragma once

#include <vsg/all.h>

template<typename T>
using ptr = vsg::ref_ptr<T>;

void createImage2D(VkFormat format, VkImageUsageFlags usage, VkExtent2D extent, ptr<vsg::Image>& image);

void createImageCube(VkFormat format, VkImageUsageFlags usage, VkExtent2D extent, uint32_t numMips, ptr<vsg::Image>& image);

void createImageViewCube(ptr<vsg::Context> context, const ptr<vsg::Image> image, VkImageAspectFlags aspectFlags, ptr<vsg::ImageView>& imageView);

void createSampler(uint32_t numMips, ptr<vsg::Sampler>& sampler);

void createImageInfo(const ptr<vsg::ImageView> imageView, VkImageLayout layout, const ptr<vsg::Sampler> sampler, ptr<vsg::ImageInfo>& imageInfo);


ptr<vsg::ImageMemoryBarrier> createImageMemoryBarrier(
    ptr<vsg::Image> image,
    VkImageSubresourceRange subresourceRange,
    VkImageLayout oldImageLayout,
    VkImageLayout newImageLayout);

ptr<vsg::PipelineBarrier> createImageLayoutPipelineBarrier(
    ptr<vsg::Image> image,
    VkImageLayout oldImageLayout,
    VkImageLayout newImageLayout,
    VkImageSubresourceRange subresourceRange,
    VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

ptr<vsg::PipelineBarrier> createImageLayoutPipelineBarrier(
    ptr<vsg::Image> image,
    VkImageAspectFlags aspectMask,
    VkImageLayout oldImageLayout,
    VkImageLayout newImageLayout,
    VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);