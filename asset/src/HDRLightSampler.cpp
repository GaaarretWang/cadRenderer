#include "HDRLightSampler.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void HDRLightSampler::loadHDRImage(const std::string& filename) {
    int width, height, channels;
    float* data = IBL::loadHdrFile(filename, width, height, channels);
    if (!data) {
        throw std::runtime_error("Failed to load HDR image: " + filename);
    }

    hdrImage.width = width;
    hdrImage.height = height;
    hdrImage.data.assign(data, data + width * height * 4);
}

void HDRLightSampler::computeLuminanceMap() {
    std::cout << "Computing luminance map..." << std::endl;
    luminanceMap.resize(hdrImage.width * hdrImage.height);

    for (size_t i = 0; i < hdrImage.data.size(); i += 4) {
        float r = hdrImage.data[i];
        float g = hdrImage.data[i + 1];
        float b = hdrImage.data[i + 2];
        float luminance = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        luminanceMap[i / 4] = luminance;
    }

    // 找到最大亮度值
    float maxLuminance = *std::max_element(luminanceMap.begin(), luminanceMap.end());

    // 归一化亮度值
    if (maxLuminance > 0.0f) {
        for (float& lum : luminanceMap) {
            lum /= maxLuminance;
        }
    }

    // 创建 Vulkan 缓冲区来存储亮度图
    /*
    VkBuffer luminanceBuffer;
    VkDeviceMemory luminanceBufferMemory;
    VkDeviceSize bufferSize = sizeof(float) * luminanceMap.size();
    createBuffer(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, luminanceBuffer, luminanceBufferMemory);

    // 将亮度数据复制到缓冲区
    void* data;
    vkMapMemory(device, luminanceBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, luminanceMap.data(), bufferSize);
    vkUnmapMemory(device, luminanceBufferMemory);
    */
}

size_t HDRLightSampler::findBrightestInArea(int areaX, int areaY, int totalAreasX, int totalAreasY) {
    int startX = areaX * hdrImage.width / totalAreasX;
    int endX = (areaX + 1) * hdrImage.width / totalAreasX;
    int startY = areaY * hdrImage.height / totalAreasY;
    int endY = (areaY + 1) * hdrImage.height / totalAreasY;

    float maxLuminance = 0.0f;
    size_t brightestIndex = startY * hdrImage.width + startX;

    for (int y = startY; y < endY; ++y) {
        for (int x = startX; x < endX; ++x) {
            size_t index = y * hdrImage.width + x;
            if (luminanceMap[index] > maxLuminance) {
                maxLuminance = luminanceMap[index];
                brightestIndex = index;
            }
        }
    }

    return brightestIndex;
}

void HDRLightSampler::computeCDF() {
    std::cout << "Computing CDF..." << std::endl;
    size_t size = hdrImage.width * hdrImage.height;
    cdf.resize(size + 1);
    cdf[0] = 0.0f;

    // 计算总亮度
    float totalLuminance = 0.0f;
    for (size_t i = 0; i < size; ++i) {
        totalLuminance += luminanceMap[i];
    }

    // 构建 CDF
    for (size_t i = 1; i <= size; ++i) {
        cdf[i] = cdf[i-1] + luminanceMap[i-1] / totalLuminance;
    }

    // 确保最后一个值为 1.0
    cdf[size] = 1.0f;

}

std::vector<LightInfo> HDRLightSampler::sampleLights(uint32_t numSamples) {
    std::cout << "Sampling lights with area division..." << std::endl;

    buildKDTree(luminanceMap, hdrImage.width, hdrImage.height);

    std::vector<LightInfo> lights;
    int areasX = std::ceil(std::sqrt(numSamples));
    int areasY = std::ceil(static_cast<float>(numSamples) / areasX);

    for (int y = 0; y < areasY; ++y) {
        for (int x = 0; x < areasX; ++x) {
            size_t brightestIndex = findBrightestInArea(x, y, areasX, areasY);
            lights.push_back(createLightFromIndex(brightestIndex));
            
            if (lights.size() >= numSamples) return lights;
        }
    }

    //return lights;

    return sampleLightsFromKDTree(numSamples);
}

LightInfo HDRLightSampler::createLightFromIndex(size_t index) {

    // 计算 UV 坐标
    float u = (index % hdrImage.width + 0.5f) / hdrImage.width;
    float v = (index / hdrImage.width + 0.5f) / hdrImage.height;

    // 将 UV 坐标转换为球面坐标
    float theta = u * 2.0f * M_PI;
    float phi = v * M_PI;

    // 将球面坐标转换为笛卡尔坐标
    float x = std::sin(phi) * std::cos(theta);
    float y = std::sin(phi) * std::sin(theta);
    float z = std::cos(phi);

    // 获取颜色
    size_t pixelIndex = index * 4;
    float r = hdrImage.data[pixelIndex];
    float g = hdrImage.data[pixelIndex + 1];
    float b = hdrImage.data[pixelIndex + 2];

    // 计算强度
    float intensity = luminanceMap[index];

    LightInfo light;
    light.position[0] = x * 1000.0f; // 假设光源在半径为1000的球面上
    light.position[1] = y * 1000.0f;
    light.position[2] = z * 1000.0f;
    light.color[0] = r;
    light.color[1] = g;
    light.color[2] = b;
    light.intensity = intensity * 1.3;

    return light;

}

LightInfo HDRLightSampler::createLightFromXY(int _x, int _y) {

    size_t index = _y * hdrImage.width + _x;

    // 计算 UV 坐标
    float u = (index % hdrImage.width + 0.5f) / hdrImage.width;
    float v = (index / hdrImage.width + 0.5f) / hdrImage.height;

    // 将 UV 坐标转换为球面坐标
    float theta = u * 2.0f * M_PI;
    float phi = v * M_PI;

    // 将球面坐标转换为笛卡尔坐标
    float x = std::sin(phi) * std::cos(theta);
    float y = std::sin(phi) * std::sin(theta);
    float z = std::cos(phi);

    // 获取颜色
    size_t pixelIndex = index * 4;
    float r = hdrImage.data[pixelIndex];
    float g = hdrImage.data[pixelIndex + 1];
    float b = hdrImage.data[pixelIndex + 2];

    // 计算强度
    float intensity = luminanceMap[index];

    LightInfo light;
    light.position[0] = x * 1000.0f; // 假设光源在半径为1000的球面上
    light.position[1] = y * 1000.0f;
    light.position[2] = z * 1000.0f;
    light.color[0] = r;
    light.color[1] = g;
    light.color[2] = b;
    light.intensity = intensity * 1.3;

    return light;

}

void HDRLightSampler::buildKDTree(const std::vector<float>& luminanceMap, int width, int height) {
    imageWidth = width;
    imageHeight = height;
    std::vector<Pixel> pixels;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t index = y * width + x;
            pixels.push_back({x, y, luminanceMap[index]});
        }
    }

    root = buildKDTreeRecursive(pixels, 0);
}

KDNode* HDRLightSampler::buildKDTreeRecursive(std::vector<Pixel>& pixels, int depth) {
    if (pixels.empty()) return nullptr;

        // Calculate variance in x and y directions
    float varianceX = calculateVariance(pixels, 0);
    float varianceY = calculateVariance(pixels, 1);

    // Choose the axis with the maximum variance
    int axis = (varianceX > varianceY) ? 0 : 1;
    std::sort(pixels.begin(), pixels.end(), [axis](const Pixel& a, const Pixel& b) {
        return (axis == 0) ? a.x < b.x : a.y < b.y;
    });

    size_t medianIndex = pixels.size() / 2;
    KDNode* node = new KDNode{pixels[medianIndex], nullptr, nullptr};

    std::vector<Pixel> leftPixels(pixels.begin(), pixels.begin() + medianIndex);
    std::vector<Pixel> rightPixels(pixels.begin() + medianIndex + 1, pixels.end());

    node->left = buildKDTreeRecursive(leftPixels, depth + 1);
    node->right = buildKDTreeRecursive(rightPixels, depth + 1);

    return node;
}

std::vector<LightInfo> HDRLightSampler::sampleLightsFromKDTree(uint32_t numSamples) {
    std::vector<LightInfo> lights;
    sampleFromKDTree(root, lights, numSamples);
    return lights;
}

void HDRLightSampler::sampleFromKDTree(KDNode* node, std::vector<LightInfo>& lights, uint32_t& numSamples) {
    if (!node || numSamples == 0) return;

    // Create light from current node
    if(node->pixel.luminance > 0.9f) {
        lights.push_back(createLightFromXY(node->pixel.x, node->pixel.y));
        --numSamples;
    }
    
    // Recursively sample from left and right children
    sampleFromKDTree(node->left, lights, numSamples);
    sampleFromKDTree(node->right, lights, numSamples);
}

float HDRLightSampler::calculateVariance(const std::vector<Pixel>& pixels, int axis) {
    float mean = 0.0f;
    float variance = 0.0f;

    if (axis == 0) {
        for (const Pixel& p : pixels) {
            mean += p.x * p.luminance;
        }
    } else {
        for (const Pixel& p : pixels) {
            mean += p.y * p.luminance;
        }
    }
    mean /= pixels.size();

    if (axis == 0) {
        for (const Pixel& p : pixels) {
            variance += p.luminance * (p.x - mean) * (p.x - mean);
        }
    } else {
        for (const Pixel& p : pixels) {
            variance += p.luminance * (p.y - mean) * (p.y - mean);
        }
    }
    variance /= pixels.size();

    return variance;
}
