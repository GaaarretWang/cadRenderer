#include "HDRLightSampler.h"

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

    // 计算区域的起始和结束索引
    size_t startIndex = startY * hdrImage.width + startX;
    size_t endIndex = (endY - 1) * hdrImage.width + (endX - 1);

    // 使用CDF找到区域内最大亮度的索引
    float startCDF = (startIndex > 0) ? cdf[startIndex - 1] : 0.0f;
    float endCDF = cdf[endIndex];

    // 区域内的最大CDF差值对应最亮的点
    float maxCDFDiff = 0.0f;
    size_t brightestIndex = startIndex;

    for (size_t y = startY; y < endY; ++y) {
        for (size_t x = startX; x < endX; ++x) {
            size_t index = y * hdrImage.width + x;
            float cdfDiff = cdf[index] - ((index > 0) ? cdf[index - 1] : 0.0f);
            
            if (cdfDiff > maxCDFDiff) {
                maxCDFDiff = cdfDiff;
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
    /*
    std::cout << "Sampling lights with area division..." << std::endl;

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

    return lights;
    */
    std::cout << "Sampling lights with importance sampling and suppression..." << std::endl;

    std::vector<LightInfo> lights;
    std::vector<float> samplingProbability = luminanceMap;

    for (uint32_t i = 0; i < numSamples; ++i) {
        size_t selectedIndex = importanceSample(samplingProbability);
        
        //lights.push_back(createLightFromIndex(selectedIndex));
        
        // 局部抑制：降低已选中点周围区域的采样概率
        //suppressArea(samplingProbability, selectedIndex);
    }

    //手动选取
    lights.push_back(createLightFromXY(110, 150));
    lights.push_back(createLightFromXY(265, 150));
    lights.push_back(createLightFromXY(600, 145));
    lights.push_back(createLightFromXY(178, 135));

    return lights;
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

size_t HDRLightSampler::importanceSample(const std::vector<float>& probabilities) {
    float totalProbability = std::accumulate(probabilities.begin(), probabilities.end(), 0.0f);
    float r = static_cast<float>(rand()) / RAND_MAX * totalProbability;

    float cumulativeProbability = 0.0f;
    for (size_t i = 0; i < probabilities.size(); ++i) {
        cumulativeProbability += probabilities[i];
        if (r <= cumulativeProbability) {
            return i;
        }
    }

    return probabilities.size() - 1;//返回最后一个
}

void HDRLightSampler::suppressArea(std::vector<float>& probabilities, size_t centerIndex) {
    int centerX = centerIndex % hdrImage.width;
    int centerY = centerIndex / hdrImage.width;
    int suppressRadius = std::min(hdrImage.width, hdrImage.height) / 20;  // 可调整的抑制半径

    for (int y = std::max(0, centerY - suppressRadius); y < std::min(static_cast<int>(hdrImage.height), centerY + suppressRadius); ++y) {
        for (int x = std::max(0, centerX - suppressRadius); x < std::min(static_cast<int>(hdrImage.width), centerX + suppressRadius); ++x) {
            size_t index = y * hdrImage.width + x;
            float distance = std::sqrt(std::pow(x - centerX, 2) + std::pow(y - centerY, 2));
            float suppressionFactor = std::max(0.0f, 1.0f - distance / suppressRadius);
            probabilities[index] *= (1.0f - suppressionFactor);
        }
    }
}
