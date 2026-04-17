#include "IBL.h"
#include "Utils.h"
#include <random>

#ifndef VSGSSAOPASS_H
#define VSGSSAOPASS_H
namespace SSAOPass{
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
                                          vsg::ref_ptr<vsg::ImageView> ssaoView,
                                          vsg::ref_ptr<vsg::ImageView> realSceneView,
                                          vsg::BufferInfoList global_buffer_info_list);

    vsg::ref_ptr<vsg::ShaderSet> customStandaloneRealSceneShaderSet(vsg::ref_ptr<const vsg::Options> options);
    void buildStandaloneRealSceneData(vsg::ref_ptr<vsg::Options> options,
                                      vsg::ref_ptr<vsg::Group> scene,
                                      const vsg::ImageInfoList& cameraImageInfoList,
                                      const vsg::ImageInfoList& realDepthInfoList,
                                      vsg::ref_ptr<vsg::ImageView> sceneDepthView,
                                      vsg::BufferInfoList global_buffer_info_list,
                                      vsg::ref_ptr<vsg::Data> shadow_pc_data);
}
#endif
