#include "vsg/all.h"
#include "CADMesh.h"

#ifndef VSGOCCLUSIONCULLINGPASSES_H
#define VSGOCCLUSIONCULLINGPASSES_H
namespace OcclusionCullingPasses{
    struct CameraPlaneInfo{
        vsg::vec4 n[6];
    };

    struct ComputePushConstants {
        uint32_t width;
        uint32_t height;
        char padding[8];
    };

    extern int mip_level_count;
    extern vsg::ref_ptr<vsg::Image> depthPyramidImage;
    extern vsg::ref_ptr<vsg::Sampler> depth_pyramid_sampler;
    extern vsg::ref_ptr<vsg::ImageView> depthPyramidImageView;
    extern vsg::ref_ptr<vsg::ImageInfo> depthPyramidImageInfo;
    extern vsg::ref_ptr<vsg::ImageInfo> framebuffer_depthImageInfo;

    void initOcclusionCullingPassesImageInfo(VkExtent2D extent, vsg::ref_ptr<vsg::Window> window);

    extern CameraPlaneInfo camera_plane_info;
    extern vsg::ref_ptr<vsg::Array<CameraPlaneInfo>> camera_plane_info_buffer;
    extern vsg::ref_ptr<vsg::BufferInfo> camera_plane_info_buffer_info;
    extern vsg::ref_ptr<vsg::mat4Array> camera_matrix;

    void generateCameraData(double fx, double fy, double cx, double cy, double w, double h, double near, double far, vsg::ref_ptr<vsg::Camera> camera);

    void buildFirstComputePass(vsg::ref_ptr<vsg::CommandGraph> depth_cull_command_graph1, std::string project_path);
    void buildDepthPyramid(vsg::ref_ptr<vsg::CommandGraph> depth_pyramid_CommandGraph, std::string project_path, vsg::ref_ptr<vsg::Window> window, VkExtent2D extent);
    void buildSecondComputePass(vsg::ref_ptr<vsg::CommandGraph> depth_pyramid_CommandGraph, std::string project_path, VkExtent2D extent);

}
#endif