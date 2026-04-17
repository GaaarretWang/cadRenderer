#include "IBL.h"
#include "Utils.h"
#include <random>

#ifndef VSGSSAOPASS_H
#define VSGSSAOPASS_H
namespace SSAOPass{
    vsg::ref_ptr<vsg::ShaderSet> customSSAOShaderSet(vsg::ref_ptr<const vsg::Options> options);
    void buildSSAOData(vsg::ref_ptr<vsg::Options> options, vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::ImageView> GBufferView0, vsg::ref_ptr<vsg::ImageView> GBufferView1, vsg::ref_ptr<vsg::ImageView> GBufferView2, VkExtent2D extent, vsg::BufferInfoList global_buffer_info_list);

    vsg::ref_ptr<vsg::ShaderSet> customSSAODenoiseShaderSet(vsg::ref_ptr<const vsg::Options> options);
    void buildSSAODenoiseData(vsg::ref_ptr<vsg::Options> options, vsg::ref_ptr<vsg::Group> scene, vsg::ref_ptr<vsg::ImageView> GBufferView0, vsg::ref_ptr<vsg::ImageView> ShadowWriteView, vsg::ref_ptr<vsg::ImageView> SSAOResultImageView, vsg::BufferInfoList global_buffer_info_list);

    vsg::ref_ptr<vsg::ShaderSet> customStandaloneSSAOShaderSet(vsg::ref_ptr<const vsg::Options> options);
    void buildStandaloneSSAOData(vsg::ref_ptr<vsg::Options> options,
                                 vsg::ref_ptr<vsg::Group> scene,
                                 vsg::ref_ptr<vsg::ImageView> normalView,
                                 vsg::ref_ptr<vsg::ImageView> worldPosView,
                                 VkExtent2D outputExtent,
                                 vsg::BufferInfoList global_buffer_info_list);

    vsg::ref_ptr<vsg::ShaderSet> customStandaloneSSAOCompositeShaderSet(vsg::ref_ptr<const vsg::Options> options);
    void buildStandaloneSSAOCompositeData(vsg::ref_ptr<vsg::Options> options,
                                          vsg::ref_ptr<vsg::Group> scene,
                                          vsg::ref_ptr<vsg::ImageView> colorView,
                                          vsg::ref_ptr<vsg::ImageView> shadowView,
                                          vsg::ref_ptr<vsg::ImageView> ssaoView,
                                          vsg::BufferInfoList global_buffer_info_list);
}
#endif
