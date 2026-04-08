#ifndef IMAGE_UTILS_H
#define IMAGE_UTILS_H
#pragma once

#include <vsg/all.h>
#include <string>
#include <vector>

class ImageUtils {
public:
    // Image-comparison result.
    struct CompareResult {
        double diff_ratio = 1.0;     // Ratio of differing pixels.
        int diff_pixels = 0;         // Number of differing pixels.
        int total_pixels = 0;        // Total pixel count.
        bool valid = false;          // Whether the comparison result is valid.
        std::string message;         // Additional detail message.
    };

    // Save vsg::Data as a PNG file.
    static bool savePNG(const std::string& filepath, vsg::ref_ptr<vsg::Data> data);

    // Save raw pixel data as a PNG file for compatibility with existing call sites.
    static bool savePNG(const std::string& filepath,
                        const uint8_t* pixels,
                        int width, int height, int channels = 4);

    // Load a PNG file into vsg::Data.
    static vsg::ref_ptr<vsg::Data> loadPNG(const std::string& filepath,
                                            int desired_channels = 4);

    // Compare two PNG files.
    static CompareResult comparePNG(const std::string& file1,
                                     const std::string& file2,
                                     int tolerance = 2);

    // Compare in-memory data against a reference image.
    static CompareResult compareWithRef(const uint8_t* pixels,
                                         int width, int height, int channels,
                                         const std::string& ref_path,
                                         int tolerance = 2);

    // Decode in-memory PNG data into an RGB image for loadImagePair.
    static unsigned char* convertColor(const std::string& png_data, int& width, int& height);

    // Decode in-memory PNG data into a 16-bit depth image for loadImagePair.
    static unsigned short* convertDepth(const std::string& png_data, int& width, int& height);
};

#endif // IMAGE_UTILS_H
