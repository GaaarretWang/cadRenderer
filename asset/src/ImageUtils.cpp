#include "ImageUtils.h"
#include <vsgXchange/images.h>
#include <stb_image.h>
#include <cstring>
#include <filesystem>
#include <cmath>
#include <iostream>

namespace {

ImageUtils::CompareResult comparePixelBuffers(const uint8_t* pixels1,
                                             const uint8_t* pixels2,
                                             int width,
                                             int height,
                                             int channels,
                                             int tolerance) {
    ImageUtils::CompareResult result;

    if (!pixels1 || !pixels2) {
        result.message = "Null pixel buffer";
        return result;
    }

    if (width <= 0 || height <= 0 || channels <= 0) {
        result.message = "Invalid image dimensions";
        return result;
    }

    result.total_pixels = width * height;
    result.diff_pixels = 0;

    for (int i = 0; i < result.total_pixels; ++i) {
        const int offset = i * channels;
        bool pixel_diff = false;
        for (int c = 0; c < channels; ++c) {
            const int diff = std::abs(static_cast<int>(pixels1[offset + c]) - static_cast<int>(pixels2[offset + c]));
            if (diff > tolerance) {
                pixel_diff = true;
                break;
            }
        }
        if (pixel_diff) {
            ++result.diff_pixels;
        }
    }

    result.diff_ratio = result.total_pixels > 0 ?
        static_cast<double>(result.diff_pixels) / static_cast<double>(result.total_pixels) : 0.0;
    result.valid = true;
    result.message = "Diff: " + std::to_string(result.diff_pixels) + "/" +
        std::to_string(result.total_pixels) + " pixels (" +
        std::to_string(result.diff_ratio * 100.0) + "%)";
    return result;
}

} // namespace

// Save vsg::Data as a PNG file.
bool ImageUtils::savePNG(const std::string& filepath, vsg::ref_ptr<vsg::Data> data) {
    if (!data) {
        vsg::error("savePNG: null data pointer");
        return false;
    }

    // Ensure the output directory exists.
    std::filesystem::path path(filepath);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    // Create the vsgXchange options object.
    auto options = vsg::Options::create(vsgXchange::images::create());

    // Write the file.
    if (!vsg::write(data, filepath, options)) {
        vsg::error("Failed to save PNG: ", filepath);
        return false;
    }

    std::cout << "PNG saved: " << filepath << std::endl;
    return true;
}

// Save raw pixel data as a PNG file for compatibility with existing call sites.
bool ImageUtils::savePNG(const std::string& filepath,
                          const uint8_t* pixels,
                          int width, int height, int channels) {
    if (!pixels) {
        vsg::error("savePNG: null pixels pointer");
        return false;
    }

    // Create the vsg::Data wrapper.
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

// Load a PNG file into vsg::Data.
vsg::ref_ptr<vsg::Data> ImageUtils::loadPNG(const std::string& filepath, int desired_channels) {
    (void)desired_channels;
    auto options = vsg::Options::create(vsgXchange::images::create());
    auto data = vsg::read_cast<vsg::Data>(filepath, options);

    if (!data) {
        vsg::error("Failed to load PNG: ", filepath);
        return {};
    }

    return data;
}

// Compare two PNG files.
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

    // Read the image dimensions.
    int w1 = data1->width(), h1 = data1->height();
    int w2 = data2->width(), h2 = data2->height();

    if (w1 != w2 || h1 != h2) {
        result.message = "Image size mismatch: " +
            std::to_string(w1) + "x" + std::to_string(h1) + " vs " +
            std::to_string(w2) + "x" + std::to_string(h2);
        return result;
    }

    const int channels1 = static_cast<int>(data1->valueSize());
    const int channels2 = static_cast<int>(data2->valueSize());
    if (channels1 != channels2) {
        result.message = "Image channel mismatch: " +
            std::to_string(channels1) + " vs " + std::to_string(channels2);
        return result;
    }

    return comparePixelBuffers(
        static_cast<const uint8_t*>(data1->dataPointer()),
        static_cast<const uint8_t*>(data2->dataPointer()),
        w1,
        h1,
        channels1,
        tolerance);
}

// Compare in-memory data against a reference image.
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

    const int ref_channels = static_cast<int>(ref_data->valueSize());
    if (ref_channels != channels) {
        result.message = "Channel mismatch: rendered " +
            std::to_string(channels) + " vs ref " + std::to_string(ref_channels);
        return result;
    }

    return comparePixelBuffers(
        pixels,
        static_cast<const uint8_t*>(ref_data->dataPointer()),
        width,
        height,
        channels,
        tolerance);
}

// Decode in-memory PNG data into an RGB image for loadImagePair.
unsigned char* ImageUtils::convertColor(const std::string& png_data, int& width, int& height) {
    const stbi_uc* data_ptr = reinterpret_cast<const stbi_uc*>(png_data.data());
    int channels = 3;
    unsigned char* pixels = stbi_load_from_memory(data_ptr, png_data.size(), &width, &height, &channels, 3);
    return pixels;
}

// Decode in-memory PNG data into a 16-bit depth image for loadImagePair.
unsigned short* ImageUtils::convertDepth(const std::string& png_data, int& width, int& height) {
    const stbi_uc* data_ptr = reinterpret_cast<const stbi_uc*>(png_data.data());
    int channels = 1;
    unsigned short* pixels = stbi_load_16_from_memory(data_ptr, png_data.size(), &width, &height, &channels, 1);
    return pixels;
}
