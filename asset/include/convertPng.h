#ifndef CONVERT_PNG_H
#define CONVERT_PNG_H
#pragma once
#include "stb_image.h"
#include "stb_image_write.h"
#include <string>
#include <vector>
#include <cmath>
#include <cstring>
#include <iostream>
#include <sys/stat.h>

class ConvertImage{
public:
    int width;
    int height;

    ConvertImage(int width, int height){
        this->width = width;
        this->height = height;
    }
    unsigned char * convertColor(const std::string& color){
        const stbi_uc* color_char = reinterpret_cast<const stbi_uc *>(color.data());
        int channels = 3;
        unsigned char *data = stbi_load_from_memory(color_char, color.size(), &width, &height, &channels, 3);
        return data;
    }

    unsigned short * convertDepth(const std::string& depth){
        const stbi_uc* depth_char = reinterpret_cast<const stbi_uc *>(depth.data());
        int channels = 1;
        unsigned short *data = stbi_load_16_from_memory(depth_char, depth.size(), &width, &height, &channels, 1);
        return data;
    }

    // 保存RGBA数据为PNG文件
    static bool savePNG(const std::string& path, const uint8_t* data, int w, int h, int channels = 4){
        if(!data){
            std::cerr << "savePNG: null data pointer" << std::endl;
            return false;
        }
        // 确保目录存在
        size_t lastSep = path.find_last_of("/\\");
        if(lastSep != std::string::npos){
            std::string dir = path.substr(0, lastSep);
            // 简单的递归创建目录
            std::string cmd = "mkdir -p " + dir;
            system(cmd.c_str());
        }
        int stride = w * channels;
        int result = stbi_write_png(path.c_str(), w, h, channels, data, stride);
        if(result){
            std::cout << "PNG saved: " << path << " (" << w << "x" << h << "x" << channels << ")" << std::endl;
        } else {
            std::cerr << "Failed to save PNG: " << path << std::endl;
        }
        return result != 0;
    }

    // 加载PNG文件为RGBA数据
    struct PNGData {
        std::vector<uint8_t> pixels;
        int width = 0;
        int height = 0;
        int channels = 0;
    };

    static PNGData loadPNG(const std::string& path, int desired_channels = 4){
        PNGData result;
        int w, h, c;
        unsigned char* data = stbi_load(path.c_str(), &w, &h, &c, desired_channels);
        if(!data){
            std::cerr << "Failed to load PNG: " << path << std::endl;
            return result;
        }
        result.width = w;
        result.height = h;
        result.channels = desired_channels;
        size_t size = (size_t)w * h * desired_channels;
        result.pixels.assign(data, data + size);
        stbi_image_free(data);
        return result;
    }

    // 对比两张图片，返回差异像素比例
    // pixel_tolerance: 单通道差异容差(默认2，防浮点精度误报)
    // 返回值: 差异像素率 (0.0 ~ 1.0)
    struct CompareResult {
        double diff_ratio = 1.0;     // 差异像素率
        int diff_pixels = 0;         // 差异像素数
        int total_pixels = 0;        // 总像素数
        bool valid = false;          // 对比是否有效
        std::string message;         // 说明信息
    };

    static CompareResult comparePNG(const std::string& path1, const std::string& path2,
                                     int pixel_tolerance = 2){
        CompareResult result;

        PNGData img1 = loadPNG(path1, 4);
        PNGData img2 = loadPNG(path2, 4);

        if(img1.pixels.empty()){
            result.message = "Failed to load image: " + path1;
            return result;
        }
        if(img2.pixels.empty()){
            result.message = "Failed to load image: " + path2;
            return result;
        }
        if(img1.width != img2.width || img1.height != img2.height){
            result.message = "Image size mismatch: " +
                std::to_string(img1.width) + "x" + std::to_string(img1.height) + " vs " +
                std::to_string(img2.width) + "x" + std::to_string(img2.height);
            return result;
        }

        result.total_pixels = img1.width * img1.height;
        result.diff_pixels = 0;

        for(int i = 0; i < result.total_pixels; i++){
            int offset = i * 4;
            bool pixel_diff = false;
            for(int c = 0; c < 4; c++){
                int diff = std::abs((int)img1.pixels[offset + c] - (int)img2.pixels[offset + c]);
                if(diff > pixel_tolerance){
                    pixel_diff = true;
                    break;
                }
            }
            if(pixel_diff) result.diff_pixels++;
        }

        result.diff_ratio = (double)result.diff_pixels / result.total_pixels;
        result.valid = true;
        result.message = "Diff: " + std::to_string(result.diff_pixels) + "/" +
            std::to_string(result.total_pixels) + " pixels (" +
            std::to_string(result.diff_ratio * 100.0) + "%)";
        return result;
    }

    // 对比内存数据和参考图
    static CompareResult compareWithRef(const uint8_t* data, int w, int h, int channels,
                                         const std::string& ref_path, int pixel_tolerance = 2){
        CompareResult result;

        PNGData ref = loadPNG(ref_path, channels);
        if(ref.pixels.empty()){
            result.message = "Failed to load reference: " + ref_path;
            return result;
        }
        if(ref.width != w || ref.height != h){
            result.message = "Size mismatch: rendered " +
                std::to_string(w) + "x" + std::to_string(h) + " vs ref " +
                std::to_string(ref.width) + "x" + std::to_string(ref.height);
            return result;
        }

        result.total_pixels = w * h;
        result.diff_pixels = 0;

        for(int i = 0; i < result.total_pixels; i++){
            int offset = i * channels;
            bool pixel_diff = false;
            for(int c = 0; c < channels; c++){
                int diff = std::abs((int)data[offset + c] - (int)ref.pixels[offset + c]);
                if(diff > pixel_tolerance){
                    pixel_diff = true;
                    break;
                }
            }
            if(pixel_diff) result.diff_pixels++;
        }

        result.diff_ratio = (double)result.diff_pixels / result.total_pixels;
        result.valid = true;
        result.message = "Diff: " + std::to_string(result.diff_pixels) + "/" +
            std::to_string(result.total_pixels) + " pixels (" +
            std::to_string(result.diff_ratio * 100.0) + "%)";
        return result;
    }
};
#endif //CONVERT_PNG_H
