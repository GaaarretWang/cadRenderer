#include <vulkan/vulkan.h>

#include <iostream>
#include <vector>
#include <stdexcept>
#include <random>
#include <cmath>
#include <algorithm>


#include "IBL.h"

struct HDRImage {
    uint32_t width;
    uint32_t height;
    std::vector<float> data; // RGBARGBA...格式
};

struct LightInfo
{
    float position[3];
    float color[3];
    float intensity;
};

struct Pixel {
    int x, y;
    float luminance;
};

struct KDNode {
    Pixel pixel;
    KDNode* left;
    KDNode* right;
};

class HDRLightSampler {
public:
    void loadHDRImage(const std::string& filename);
    void computeLuminanceMap();
    void computeCDF();
    std::vector<LightInfo> sampleLights(uint32_t numSamples);
    void buildKDTree(const std::vector<float>& luminanceMap, int width, int height);
    std::vector<LightInfo> sampleLightsFromKDTree(uint32_t numSamples);

private:
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    HDRImage hdrImage;
    std::vector<float> luminanceMap;
    std::vector<float> cdf;

    KDNode* buildKDTreeRecursive(std::vector<Pixel>& pixels, int depth);
    void sampleFromKDTree(KDNode* node, std::vector<LightInfo>& lights, uint32_t& numSamples);

    KDNode* root;
    int imageWidth;
    int imageHeight;

    // 辅助函数
    size_t findBrightestInArea(int areaX, int areaY, int totalAreasX, int totalAreasY);
    void suppressArea(std::vector<float>& probabilities, size_t centerIndex);
    size_t importanceSample(const std::vector<float>& probabilities);
    LightInfo createLightFromIndex(size_t index);
    LightInfo createLightFromXY(int x, int y);
    float calculateVariance(const std::vector<Pixel>& pixels, int axis);
};