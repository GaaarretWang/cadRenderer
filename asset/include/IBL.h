#ifndef ZSZ_IBL
#define ZSZ_IBL

#include <vsg/all.h>
#include "CustomViewDependentState.h"

namespace IBL
{
    template<typename T>
    using ptr = vsg::ref_ptr<T>;//简化智能指针书写

    namespace Constants
    {
        namespace EnvmapCube
        {
            constexpr VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
            constexpr int32_t dim = 512;
            // no auto generated mips
            constexpr uint32_t numMips = 10;  // log2(dim)=9
            //constexpr uint32_t numMips = 1;
            constexpr VkExtent2D extent = {dim, dim};
        } // namespace EnvmapCube
        namespace BrdfLUT
        {
            constexpr VkFormat format = VK_FORMAT_R16G16_SFLOAT;
            constexpr int32_t dim = 512;
            constexpr VkExtent2D extent = {dim, dim};
        }
        namespace IrradianceCube
        {
            constexpr VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
            constexpr int32_t dim = 64;
            constexpr uint32_t numMips = 7; // floor(log2(dim))) + 1
            constexpr VkExtent2D extent = {dim, dim};
        } // namespace Irradiance
        namespace PrefilteredEnvmapCube
        {
            constexpr VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
            constexpr int32_t dim = 512;
            constexpr uint32_t numMips = 10; // log2(dim)=9
            //constexpr int32_t dim = 64;
            //constexpr uint32_t numMips = 7; // floor(log2(dim))) + 1
            constexpr VkExtent2D extent = {dim, dim};
        } // namespace PrefiltedEnvmapCube
    }     // namespace Constants

    typedef struct _Textures{
        vsg::ref_ptr<vsg::Image> envmapCube;
        vsg::ref_ptr<vsg::ImageView> envmapCubeView;
        vsg::ref_ptr<vsg::Sampler> envmapCubeSmapler;
        vsg::ref_ptr<vsg::ImageInfo> envmapCubeInfo;
        // Generated at runtime
        vsg::ref_ptr<vsg::Image> brdfLut, irradianceCube, prefilterCube;
        vsg::ref_ptr<vsg::ImageView> brdfLutView, irradianceCubeView, prefilterCubeView;
        vsg::ref_ptr<vsg::Sampler> brdfLutSampler, irradianceCubeSampler, prefilterCubeSampler;
        vsg::ref_ptr<vsg::ImageInfo> brdfLutInfo, irradianceCubeInfo, prefilterCubeInfo;

        vsg::ref_ptr<vsg::vec4Array> params;
        vsg::ref_ptr<vsg::BufferInfo> paramsInfo;
    } Textures;

    typedef struct _VsgContext
    {
        ptr<vsg::Viewer> viewer;
        //ptr<vsg::Window> window;
        ptr<vsg::Context> context;
        ptr<vsg::Device> device;
        int queueFamily;
    } VsgContext;

    typedef struct _AppData {
        ptr<vsg::Options> options;
        vsg::Path debugOutputPath;
    } AppData;

    extern Textures textures;
    //extern VsgContext vsgContext;
    extern AppData appData;

    void createResources(VsgContext &vsgContext);

    void loadEnvmapRect(VsgContext &vsgContext);

    void clearResources();

    void generateBRDFLUT(VsgContext &vsgContext);

    void generateEnvmap(VsgContext& vsgContext);

    void generateIrradianceCube(VsgContext& vsgContext);

    void generatePrefilteredEnvmapCube(VsgContext& vsgContext);

    ptr<vsg::StateGroup> drawSkyboxVSGNode(VsgContext& context);

    vsg::ref_ptr<vsg::ShaderSet> customPbrShaderSet(vsg::ref_ptr<const vsg::Options> options);

    ptr<vsg::Node> iblDemoSceneGraph(VsgContext& context);
}
#endif // !ZSZ_IBL