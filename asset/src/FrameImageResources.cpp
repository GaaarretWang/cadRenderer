#include "FrameImageResources.h"

void FrameImageResources::initialize(int width, int height)
{
    color_image_ = vsg::ubvec3Array2D::create(width, height);
    depth_image_ = vsg::ushortArray2D::create(width, height);

    color_image_->properties.format = VK_FORMAT_R8G8B8_UNORM;
    color_image_->properties.dataVariance = vsg::DYNAMIC_DATA;
    depth_image_->properties.format = VK_FORMAT_R16_UNORM;
    depth_image_->properties.dataVariance = vsg::DYNAMIC_DATA;

    camera_info_ = createImageInfo(color_image_);
    depth_info_ = createImageInfo(depth_image_);
}

vsg::ImageInfoList FrameImageResources::createImageInfo(vsg::ref_ptr<vsg::Data> data)
{
    auto sampler = vsg::Sampler::create();
    sampler->magFilter = VK_FILTER_NEAREST;
    sampler->minFilter = VK_FILTER_NEAREST;

    return {vsg::ImageInfo::create(sampler, data)};
}
