#ifndef FRAME_IMAGE_RESOURCES_H
#define FRAME_IMAGE_RESOURCES_H
#pragma once

#include <vsg/all.h>

class FrameImageResources
{
public:
    void initialize(int width, int height);
    void uploadDepthPixels(const unsigned short* depth_pixels);

    vsg::ref_ptr<vsg::Data> colorImage() const { return color_image_; }
    vsg::ref_ptr<vsg::Data> rawDepthImage() const { return raw_depth_image_; }
    const vsg::ImageInfoList& cameraInfo() const { return camera_info_; }
    const vsg::ImageInfoList& depthInfo() const { return depth_info_; }
    vsg::ref_ptr<vsg::ImageInfo> rawDepthInputInfo() const { return raw_depth_input_info_; }
    vsg::ref_ptr<vsg::ImageInfo> pingDepthInputInfo() const { return ping_depth_input_info_; }
    vsg::ref_ptr<vsg::ImageInfo> pongDepthInputInfo() const { return pong_depth_input_info_; }
    vsg::ref_ptr<vsg::ImageInfo> pingDepthStorageInfo() const { return ping_depth_storage_info_; }
    vsg::ref_ptr<vsg::ImageInfo> pongDepthStorageInfo() const { return pong_depth_storage_info_; }
    uint32_t width() const { return width_; }
    uint32_t height() const { return height_; }

private:
    static vsg::ImageInfoList createImageInfo(vsg::ref_ptr<vsg::Data> data);
    static vsg::ref_ptr<vsg::Image> createStorageImage(uint32_t width, uint32_t height);
    static vsg::ref_ptr<vsg::ImageView> createImageView(vsg::ref_ptr<vsg::Image> image);
    static vsg::ref_ptr<vsg::ImageInfo> createSampledImageInfo(vsg::ref_ptr<vsg::Sampler> sampler, vsg::ref_ptr<vsg::Image> image);
    static vsg::ref_ptr<vsg::ImageInfo> createStorageImageInfo(vsg::ref_ptr<vsg::Sampler> sampler, vsg::ref_ptr<vsg::Image> image);

    uint32_t width_ = 0;
    uint32_t height_ = 0;
    vsg::ref_ptr<vsg::Data> color_image_;
    vsg::ref_ptr<vsg::Data> raw_depth_image_;
    vsg::ImageInfoList camera_info_;
    vsg::ImageInfoList depth_info_;
    vsg::ref_ptr<vsg::ImageInfo> raw_depth_input_info_;
    vsg::ref_ptr<vsg::Sampler> depth_sampler_;
    vsg::ref_ptr<vsg::Image> ping_depth_image_;
    vsg::ref_ptr<vsg::Image> pong_depth_image_;
    vsg::ref_ptr<vsg::ImageInfo> ping_depth_input_info_;
    vsg::ref_ptr<vsg::ImageInfo> pong_depth_input_info_;
    vsg::ref_ptr<vsg::ImageInfo> ping_depth_storage_info_;
    vsg::ref_ptr<vsg::ImageInfo> pong_depth_storage_info_;
};

#endif
