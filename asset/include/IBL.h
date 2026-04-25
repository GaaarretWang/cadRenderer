#ifndef ZSZ_IBL
#define ZSZ_IBL

#include <vsg/all.h>
#include <vsg/utils/Builder.h>
#include <vsg/utils/ShaderSet.h>
#include "CustomViewDependentState.h"
#include "CustomViewDependentState1.h"

namespace IBL
{
    template<typename T>
    using ptr = vsg::ref_ptr<T>; // Short alias for the smart-pointer type.

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

    struct _ImageLine{
        vsg::ref_ptr<vsg::Image> cube;
        vsg::ref_ptr<vsg::ImageView> cubeView;
        vsg::ref_ptr<vsg::Sampler> cubeSmapler;
        vsg::ref_ptr<vsg::ImageInfo> cubeInfo;
    };

    typedef struct _Textures{
        vsg::ref_ptr<vsg::Image> envmapCube;
        vsg::ref_ptr<vsg::ImageView> envmapCubeView;
        vsg::ref_ptr<vsg::Sampler> envmapCubeSmapler;
        vsg::ref_ptr<vsg::ImageInfo> envmapCubeInfo;
        //for test
        std::unordered_map<int, _ImageLine> testMap;
        std::unordered_map<int, _ImageLine> irraMap;
        std::unordered_map<int, _ImageLine> prefMap;
        // Generated at runtime
        std::unordered_map<int, vsg::ref_ptr<vsg::Image>> envMap, irradianceMap, prefilteredMap;
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

    struct DynamicSkyboxParams
    {
        float exposure = 3.0f;
        float gamma = 2.2f;
        float width = 0.0f;
        float height = 0.0f;
        float enableRealDepthOcclusion = 0.0f;
        float shadowMode = 0.0f;
    };

    void createResources(VsgContext &vsgContext, int hdr_image_max_num);

    void loadEnvmapRect(VsgContext &vsgContext, const std::string& filePath);

    float *loadHdrFile(const std::string &filepath, int &width, int &height, int &channels);

    void clearResources();

    void generateBRDFLUT(VsgContext &vsgContext);

    void generateEnvmap(VsgContext& vsgContext, std::string& envmapFilepath, int hdr);

    void generateIrradianceCube(VsgContext& vsgContext, int hdr);

    void generatePrefilteredEnvmapCube(VsgContext& vsgContext, int hdr);

    ptr<vsg::StateGroup> drawSkyboxVSGNode(vsg::ref_ptr<vsg::StateGroup> root,
                                           int width,
                                           int height,
                                           vsg::ref_ptr<vsg::Data> tonemap_params_override = {});

    ptr<vsg::StateGroup> drawCameraBaseVSGNode(vsg::ref_ptr<vsg::StateGroup> root,
                                               int width,
                                               int height,
                                               vsg::ImageInfoList camera_image_data,
                                               vsg::ImageInfoList depth_data,
                                               vsg::ref_ptr<vsg::Data> tonemap_params_override = {},
                                               bool write_color = false);

    vsg::ref_ptr<vsg::ShaderSet> customPbrShaderSet(vsg::ref_ptr<const vsg::Options> options);

    ptr<vsg::Node> iblDemoSceneGraph(VsgContext& context);
    
    void updateHDRTextures(vsg::ref_ptr<vsg::Commands>& vsgContext, int hdr);
}
#endif // ZSZ_IBL
