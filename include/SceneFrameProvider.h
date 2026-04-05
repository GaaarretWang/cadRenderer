#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include <vsg/all.h>

#include <JsonConfigManager.h>

struct ImagePair
{
    std::unique_ptr<unsigned char[]> color;
    std::unique_ptr<unsigned short[]> depth;
};

struct FrameData
{
    ImagePair image;
    std::array<double, 9> lookat{};
};

class SceneFrameProvider
{
public:
    enum class PlaybackType
    {
        DynamicVideo,
        StaticImage
    };

    SceneFrameProvider() = default;

    bool initialize(const SceneConfig& scene,
                    const std::string& rendering_dir,
                    int width,
                    int height,
                    std::string* error_message = nullptr);

    bool hasFrame() const;
    size_t frameCount() const;
    bool isStatic() const;
    const FrameData& currentFrame() const;
    void advance();
    void reset();

private:
    SceneConfig scene_config_;
    std::string rendering_dir_;
    int width_ = 0;
    int height_ = 0;
    PlaybackType playback_type_ = PlaybackType::DynamicVideo;
    size_t current_frame_index_ = 0;
    std::vector<FrameData> frames_;

    bool loadPoseFile(const std::string& pose_path,
                      bool first_line_only,
                      std::vector<std::string>& timestamps,
                      std::vector<std::array<double, 9>>& lookat_values,
                      std::string* error_message);
    bool loadImagePair(const std::string& color_path, const std::string& depth_path, ImagePair& image_pair, std::string* error_message);
    static bool parsePoseLineToLookAt(const std::string& line, std::string& timestamp, std::array<double, 9>& lookat_values);
    static bool readBinaryFile(const std::string& path, std::string& out_data, std::string* error_message);
    void clear();
};
