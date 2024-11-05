#pragma once
#include "stb_image.h"

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
};
