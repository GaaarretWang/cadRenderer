#include "IBL.h"
#include "Utils.h"
#include <random>

#ifndef VSGSSAOPASS_H
#define VSGSSAOPASS_H
namespace SSAOPass{
    void buildStandaloneSSAOData(vsg::ref_ptr<vsg::Options> options,
                                 vsg::ref_ptr<vsg::Group> scene,
                                 vsg::ref_ptr<vsg::ImageView> normalView,
                                 vsg::ref_ptr<vsg::ImageView> worldPosView,
                                 VkExtent2D outputExtent,
                                 vsg::BufferInfoList global_buffer_info_list,
                                 bool useResolvedInputs = false);

    void buildStandaloneSSAOCompositeData(vsg::ref_ptr<vsg::Options> options,
                                          vsg::ref_ptr<vsg::Group> scene,
                                          vsg::ref_ptr<vsg::ImageView> colorView,
                                          vsg::ref_ptr<vsg::ImageView> ssaoView,
                                          vsg::ref_ptr<vsg::ImageView> realSceneView,
                                          vsg::ref_ptr<vsg::ImageView> maskView,
                                          vsg::BufferInfoList global_buffer_info_list,
                                          bool useMsaaColorInput,
                                          VkSampleCountFlagBits outputSamples);

    void buildStandaloneRealSceneData(vsg::ref_ptr<vsg::Options> options,
                                      vsg::ref_ptr<vsg::Group> scene,
                                      const vsg::ImageInfoList& cameraImageInfoList,
                                      const vsg::ImageInfoList& realDepthInfoList,
                                      vsg::ref_ptr<vsg::ImageView> sceneDepthView,
                                      VkImageLayout sceneDepthLayout,
                                      vsg::BufferInfoList global_buffer_info_list,
                                      vsg::ref_ptr<vsg::Data> shadow_pc_data);

    void buildStandaloneResolvedDepthData(vsg::ref_ptr<vsg::Options> options,
                                          vsg::ref_ptr<vsg::Group> scene,
                                          vsg::ref_ptr<vsg::ImageView> depthView);

    void buildStandaloneResolvedNormalData(vsg::ref_ptr<vsg::Options> options,
                                           vsg::ref_ptr<vsg::Group> scene,
                                           vsg::ref_ptr<vsg::ImageView> depthView,
                                           vsg::ref_ptr<vsg::ImageView> normalView);

    void buildStandaloneResolvedWorldPosData(vsg::ref_ptr<vsg::Options> options,
                                             vsg::ref_ptr<vsg::Group> scene,
                                             vsg::ref_ptr<vsg::ImageView> depthView,
                                             vsg::ref_ptr<vsg::ImageView> worldPosView);

    void buildStandaloneResolvedMaterialData(vsg::ref_ptr<vsg::Options> options,
                                             vsg::ref_ptr<vsg::Group> scene,
                                             vsg::ref_ptr<vsg::ImageView> depthView,
                                             vsg::ref_ptr<vsg::ImageView> materialView);

    void buildStandaloneResolvedMaskData(vsg::ref_ptr<vsg::Options> options,
                                         vsg::ref_ptr<vsg::Group> scene,
                                         vsg::ref_ptr<vsg::ImageView> depthView,
                                         vsg::ref_ptr<vsg::ImageView> maskView);

    void buildStandaloneDeferredOpaqueData(vsg::ref_ptr<vsg::Options> options,
                                           vsg::ref_ptr<vsg::Group> scene,
                                           vsg::ref_ptr<vsg::ImageView> colorView,
                                           vsg::ref_ptr<vsg::ImageView> normalView,
                                           vsg::ref_ptr<vsg::ImageView> worldPosView,
                                           vsg::ref_ptr<vsg::ImageView> materialView,
                                           vsg::ref_ptr<vsg::ImageView> ssaoView,
                                           vsg::BufferInfoList global_buffer_info_list,
                                           bool useMsaaInputs,
                                           VkSampleCountFlagBits outputSamples);

    void buildStandaloneDeferredCompositeData(vsg::ref_ptr<vsg::Options> options,
                                              vsg::ref_ptr<vsg::Group> scene,
                                              vsg::ref_ptr<vsg::ImageView> deferredView,
                                              VkImageLayout deferredLayout,
                                              vsg::ref_ptr<vsg::ImageView> fallbackView,
                                              VkImageLayout fallbackLayout);
}
#endif
