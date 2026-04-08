#ifndef FRAME_IMAGE_RESOURCES_H
#define FRAME_IMAGE_RESOURCES_H
#pragma once

#include <vsg/all.h>

class FrameImageResources
{
public:
    void initialize(int width, int height);

    vsg::ref_ptr<vsg::Data> colorImage() const { return color_image_; }
    vsg::ref_ptr<vsg::Data> depthImage() const { return depth_image_; }
    const vsg::ImageInfoList& cameraInfo() const { return camera_info_; }
    const vsg::ImageInfoList& depthInfo() const { return depth_info_; }

private:
    static vsg::ImageInfoList createImageInfo(vsg::ref_ptr<vsg::Data> data);

    vsg::ref_ptr<vsg::Data> color_image_;
    vsg::ref_ptr<vsg::Data> depth_image_;
    vsg::ImageInfoList camera_info_;
    vsg::ImageInfoList depth_info_;
};

#endif
