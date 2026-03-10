#ifndef IMAGE_UTILS_H
#define IMAGE_UTILS_H
#pragma once

#include <vsg/all.h>
#include <string>
#include <vector>

class ImageUtils {
public:
    // 对比结果结构体
    struct CompareResult {
        double diff_ratio = 1.0;     // 差异像素率
        int diff_pixels = 0;         // 差异像素数
        int total_pixels = 0;        // 总像素数
        bool valid = false;          // 对比是否有效
        std::string message;         // 说明信息
    };

    // 保存 vsg::Data 为 PNG 文件
    static bool savePNG(const std::string& filepath, vsg::ref_ptr<vsg::Data> data);

    // 保存原始像素数据为 PNG 文件（兼容现有调用）
    static bool savePNG(const std::string& filepath,
                        const uint8_t* pixels,
                        int width, int height, int channels = 4);

    // 加载 PNG 文件为 vsg::Data
    static vsg::ref_ptr<vsg::Data> loadPNG(const std::string& filepath,
                                            int desired_channels = 4);

    // 对比两个 PNG 文件
    static CompareResult comparePNG(const std::string& file1,
                                     const std::string& file2,
                                     int tolerance = 2);

    // 对比内存数据与参考图像
    static CompareResult compareWithRef(const uint8_t* pixels,
                                         int width, int height, int channels,
                                         const std::string& ref_path,
                                         int tolerance = 2);

    // 从内存中的 PNG 数据解码为 RGB 图像（用于 loadImagePair）
    static unsigned char* convertColor(const std::string& png_data, int& width, int& height);

    // 从内存中的 PNG 数据解码为 16-bit 深度图像（用于 loadImagePair）
    static unsigned short* convertDepth(const std::string& png_data, int& width, int& height);
};

#endif // IMAGE_UTILS_H
