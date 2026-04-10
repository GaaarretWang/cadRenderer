#include "FrameImageResources.h"

void FrameImageResources::initialize(int width, int height)
{
    width_ = static_cast<uint32_t>(width);
    height_ = static_cast<uint32_t>(height);
    color_image_ = vsg::ubvec3Array2D::create(width, height);
    raw_depth_image_ = vsg::ushortArray2D::create(width, height);

    color_image_->properties.format = VK_FORMAT_R8G8B8_UNORM;
    color_image_->properties.dataVariance = vsg::DYNAMIC_DATA;
    raw_depth_image_->properties.format = VK_FORMAT_R16_UNORM;
    raw_depth_image_->properties.dataVariance = vsg::DYNAMIC_DATA;

    camera_info_ = createImageInfo(color_image_);
    raw_depth_input_info_ = createImageInfo(raw_depth_image_).front();

    depth_sampler_ = vsg::Sampler::create();
    depth_sampler_->magFilter = VK_FILTER_NEAREST;
    depth_sampler_->minFilter = VK_FILTER_NEAREST;

    ping_depth_image_ = createStorageImage(width_, height_);
    pong_depth_image_ = createStorageImage(width_, height_);
    ping_depth_input_info_ = createSampledImageInfo(depth_sampler_, ping_depth_image_);
    pong_depth_input_info_ = createSampledImageInfo(depth_sampler_, pong_depth_image_);
    ping_depth_storage_info_ = createStorageImageInfo(depth_sampler_, ping_depth_image_);
    pong_depth_storage_info_ = createStorageImageInfo(depth_sampler_, pong_depth_image_);
    depth_info_ = {ping_depth_input_info_};
}

void FrameImageResources::uploadDepthPixels(const unsigned short* depth_pixels)
{
    if (!depth_pixels || !raw_depth_image_)
    {
        return;
    }

    auto* raw_depth = static_cast<unsigned short*>(raw_depth_image_->dataPointer(0));
    std::copy(depth_pixels, depth_pixels + (width_ * height_), raw_depth);
    raw_depth_image_->dirty();
}

vsg::ImageInfoList FrameImageResources::createImageInfo(vsg::ref_ptr<vsg::Data> data)
{
    auto sampler = vsg::Sampler::create();
    sampler->magFilter = VK_FILTER_NEAREST;
    sampler->minFilter = VK_FILTER_NEAREST;

    return {vsg::ImageInfo::create(sampler, data)};
}

vsg::ref_ptr<vsg::Image> FrameImageResources::createStorageImage(uint32_t width, uint32_t height)
{
    auto image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = VK_FORMAT_R32_SFLOAT;
    image->mipLevels = 1;
    image->arrayLayers = 1;
    image->samples = VK_SAMPLE_COUNT_1_BIT;
    image->tiling = VK_IMAGE_TILING_OPTIMAL;
    image->usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image->initialLayout = VK_IMAGE_LAYOUT_GENERAL;
    image->extent = VkExtent3D{width, height, 1};
    return image;
}

vsg::ref_ptr<vsg::ImageView> FrameImageResources::createImageView(vsg::ref_ptr<vsg::Image> image)
{
    auto image_view = vsg::ImageView::create(image);
    image_view->subresourceRange.baseMipLevel = 0;
    image_view->subresourceRange.levelCount = 1;
    image_view->subresourceRange.baseArrayLayer = 0;
    image_view->subresourceRange.layerCount = 1;
    return image_view;
}

vsg::ref_ptr<vsg::ImageInfo> FrameImageResources::createSampledImageInfo(vsg::ref_ptr<vsg::Sampler> sampler, vsg::ref_ptr<vsg::Image> image)
{
    return vsg::ImageInfo::create(sampler, createImageView(image), VK_IMAGE_LAYOUT_GENERAL);
}

vsg::ref_ptr<vsg::ImageInfo> FrameImageResources::createStorageImageInfo(vsg::ref_ptr<vsg::Sampler> sampler, vsg::ref_ptr<vsg::Image> image)
{
    return vsg::ImageInfo::create(sampler, createImageView(image), VK_IMAGE_LAYOUT_GENERAL);
}
