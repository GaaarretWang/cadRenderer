#include "ImageUtils.h"
#include <vsgXchange/images.h>
#include <stb_image.h>
#include <filesystem>
#include <cmath>
#include <iostream>

// 保存 vsg::Data 为 PNG 文件
bool ImageUtils::savePNG(const std::string& filepath, vsg::ref_ptr<vsg::Data> data) {
    if (!data) {
        vsg::error("savePNG: null data pointer");
        return false;
    }

    // 确保目录存在
    std::filesystem::path path(filepath);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    // 创建 vsgXchange 选项
    auto options = vsg::Options::create(vsgXchange::images::create());

    // 写入文件
    if (!vsg::write(data, filepath, options)) {
        vsg::error("Failed to save PNG: ", filepath);
        return false;
    }

    std::cout << "PNG saved: " << filepath << std::endl;
    return true;
}

// 保存原始像素数据为 PNG 文件（兼容现有调用）
bool ImageUtils::savePNG(const std::string& filepath,
                          const uint8_t* pixels,
                          int width, int height, int channels) {
    if (!pixels) {
        vsg::error("savePNG: null pixels pointer");
        return false;
    }

    // 创建 vsg::Data 对象
    vsg::ref_ptr<vsg::Data> data;

    if (channels == 4) {
        auto imageData = vsg::ubvec4Array2D::create(width, height);
        imageData->properties.format = VK_FORMAT_R8G8B8A8_UNORM;
        std::memcpy(imageData->dataPointer(), pixels, width * height * 4);
        data = imageData;
    } else if (channels == 3) {
        auto imageData = vsg::ubvec3Array2D::create(width, height);
        imageData->properties.format = VK_FORMAT_R8G8B8_UNORM;
        std::memcpy(imageData->dataPointer(), pixels, width * height * 3);
        data = imageData;
    } else if (channels == 1) {
        auto imageData = vsg::ubyteArray2D::create(width, height);
        imageData->properties.format = VK_FORMAT_R8_UNORM;
        std::memcpy(imageData->dataPointer(), pixels, width * height);
        data = imageData;
    } else {
        vsg::error("savePNG: unsupported channel count: ", channels);
        return false;
    }

    return savePNG(filepath, data);
}

// 加载 PNG 文件为 vsg::Data
vsg::ref_ptr<vsg::Data> ImageUtils::loadPNG(const std::string& filepath, int desired_channels) {
    auto options = vsg::Options::create(vsgXchange::images::create());
    auto data = vsg::read_cast<vsg::Data>(filepath, options);

    if (!data) {
        vsg::error("Failed to load PNG: ", filepath);
        return {};
    }

    return data;
}

// 对比两个 PNG 文件
ImageUtils::CompareResult ImageUtils::comparePNG(const std::string& file1,
                                                   const std::string& file2,
                                                   int tolerance) {
    CompareResult result;

    auto data1 = loadPNG(file1, 4);
    auto data2 = loadPNG(file2, 4);

    if (!data1) {
        result.message = "Failed to load image: " + file1;
        return result;
    }
    if (!data2) {
        result.message = "Failed to load image: " + file2;
        return result;
    }

    // 获取图像尺寸
    int w1 = data1->width(), h1 = data1->height();
    int w2 = data2->width(), h2 = data2->height();

    if (w1 != w2 || h1 != h2) {
        result.message = "Image size mismatch: " +
            std::to_string(w1) + "x" + std::to_string(h1) + " vs " +
            std::to_string(w2) + "x" + std::to_string(h2);
        return result;
    }

    result.total_pixels = w1 * h1;
    result.diff_pixels = 0;

    // 获取像素数据指针
    const uint8_t* pixels1 = static_cast<const uint8_t*>(data1->dataPointer());
    const uint8_t* pixels2 = static_cast<const uint8_t*>(data2->dataPointer());

    // 逐像素对比
    for (int i = 0; i < result.total_pixels; i++) {
        int offset = i * 4;
        bool pixel_diff = false;
        for (int c = 0; c < 4; c++) {
            int diff = std::abs((int)pixels1[offset + c] - (int)pixels2[offset + c]);
            if (diff > tolerance) {
                pixel_diff = true;
                break;
            }
        }
        if (pixel_diff) result.diff_pixels++;
    }

    result.diff_ratio = (double)result.diff_pixels / result.total_pixels;
    result.valid = true;
    result.message = "Diff: " + std::to_string(result.diff_pixels) + "/" +
        std::to_string(result.total_pixels) + " pixels (" +
        std::to_string(result.diff_ratio * 100.0) + "%)";
    return result;
}

// 对比内存数据与参考图像
ImageUtils::CompareResult ImageUtils::compareWithRef(const uint8_t* pixels,
                                                       int width, int height, int channels,
                                                       const std::string& ref_path,
                                                       int tolerance) {
    CompareResult result;

    auto ref_data = loadPNG(ref_path, channels);
    if (!ref_data) {
        result.message = "Failed to load reference: " + ref_path;
        return result;
    }

    int ref_w = ref_data->width();
    int ref_h = ref_data->height();

    if (ref_w != width || ref_h != height) {
        result.message = "Size mismatch: rendered " +
            std::to_string(width) + "x" + std::to_string(height) + " vs ref " +
            std::to_string(ref_w) + "x" + std::to_string(ref_h);
        return result;
    }

    result.total_pixels = width * height;
    result.diff_pixels = 0;

    result.diff_ratio = (double)result.diff_pixels / result.total_pixels;
    result.valid = true;
    result.message = "Diff: " + std::to_string(result.diff_pixels) + "/" +
        std::to_string(result.total_pixels) + " pixels (" +
        std::to_string(result.diff_ratio * 100.0) + "%)";
    return result;
}

// 从内存中的 PNG 数据解码为 RGB 图像（用于 loadImagePair）
unsigned char* ImageUtils::convertColor(const std::string& png_data, int& width, int& height) {
    const stbi_uc* data_ptr = reinterpret_cast<const stbi_uc*>(png_data.data());
    int channels = 3;
    unsigned char* pixels = stbi_load_from_memory(data_ptr, png_data.size(), &width, &height, &channels, 3);
    return pixels;
}

// 从内存中的 PNG 数据解码为 16-bit 深度图像（用于 loadImagePair）
unsigned short* ImageUtils::convertDepth(const std::string& png_data, int& width, int& height) {
    const stbi_uc* data_ptr = reinterpret_cast<const stbi_uc*>(png_data.data());
    int channels = 1;
    unsigned short* pixels = stbi_load_16_from_memory(data_ptr, png_data.size(), &width, &height, &channels, 1);
    return pixels;
}
