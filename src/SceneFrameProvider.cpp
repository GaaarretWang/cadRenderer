#include "SceneFrameProvider.h"

#include "ImageUtils.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>

namespace
{
bool isStaticScene(const SceneConfig& scene)
{
    return scene.media.type == SceneMediaConfig::Type::StaticImage;
}
}

bool SceneFrameProvider::initialize(const SceneConfig& scene,
                                    const std::string& rendering_dir,
                                    int width,
                                    int height,
                                    std::string* error_message)
{
    clear();

    scene_config_ = scene;
    rendering_dir_ = rendering_dir;
    width_ = width;
    height_ = height;
    playback_type_ = isStaticScene(scene) ? PlaybackType::StaticImage : PlaybackType::DynamicVideo;

    const bool static_mode = playback_type_ == PlaybackType::StaticImage;
    std::vector<std::string> timestamps;
    std::vector<std::array<double, 9>> lookat_values;
    const std::string pose_path = rendering_dir_ + scene_config_.media.pose_path;
    if (!loadPoseFile(pose_path, static_mode, timestamps, lookat_values, error_message))
    {
        clear();
        return false;
    }

    if (static_mode)
    {
        FrameData frame;
        if (!loadImagePair(rendering_dir_ + scene_config_.media.color_path,
                           rendering_dir_ + scene_config_.media.depth_path,
                           frame.image,
                           error_message))
        {
            clear();
            return false;
        }
        frame.lookat = lookat_values.front();
        frames_.push_back(std::move(frame));
    }
    else
    {
        const std::string color_dir = rendering_dir_ + scene_config_.media.color_dir;
        const std::string depth_dir = rendering_dir_ + scene_config_.media.depth_dir;
        frames_.reserve(timestamps.size());
        for (size_t i = 0; i < timestamps.size(); ++i)
        {
            FrameData frame;
            frame.lookat = lookat_values[i];
            if (!loadImagePair(color_dir + "/" + timestamps[i] + ".png",
                               depth_dir + "/" + timestamps[i] + ".png",
                               frame.image,
                               error_message))
            {
                clear();
                return false;
            }
            frames_.push_back(std::move(frame));
        }
    }

    current_frame_index_ = 0;
    if (frames_.empty())
    {
        if (error_message) *error_message = "No frame data loaded from scene media";
        clear();
        return false;
    }

    return true;
}

bool SceneFrameProvider::hasFrame() const
{
    return !frames_.empty();
}

size_t SceneFrameProvider::frameCount() const
{
    return frames_.size();
}

bool SceneFrameProvider::isStatic() const
{
    return playback_type_ == PlaybackType::StaticImage;
}

const FrameData& SceneFrameProvider::currentFrame() const
{
    return frames_.at(current_frame_index_);
}

void SceneFrameProvider::advance()
{
    if (frames_.empty() || isStatic())
    {
        return;
    }
    current_frame_index_ = (current_frame_index_ + 1) % frames_.size();
}

void SceneFrameProvider::reset()
{
    current_frame_index_ = 0;
}

bool SceneFrameProvider::loadPoseFile(const std::string& pose_path,
                                      bool first_line_only,
                                      std::vector<std::string>& timestamps,
                                      std::vector<std::array<double, 9>>& out_lookat_values,
                                      std::string* error_message)
{
    std::ifstream pose_stream(pose_path);
    if (!pose_stream.is_open())
    {
        if (error_message) *error_message = "Failed to open pose file: " + pose_path;
        return false;
    }

    std::string line;
    while (std::getline(pose_stream, line))
    {
        std::string ts;
        std::array<double, 9> parsed_lookat{};
        if (!parsePoseLineToLookAt(line, ts, parsed_lookat))
        {
            continue;
        }
        timestamps.push_back(ts);
        out_lookat_values.push_back(parsed_lookat);

        if (first_line_only)
        {
            break;
        }
    }

    return !timestamps.empty();
}

bool SceneFrameProvider::loadImagePair(const std::string& color_path,
                                       const std::string& depth_path,
                                       ImagePair& image_pair,
                                       std::string* error_message)
{
    try
    {
        std::string color_data;
        if (!readBinaryFile(color_path, color_data, error_message))
        {
            return false;
        }

        std::string depth_data;
        if (!readBinaryFile(depth_path, depth_data, error_message))
        {
            return false;
        }

        int w = width_;
        int h = height_;
        image_pair.color.reset(ImageUtils::convertColor(color_data, w, h));
        image_pair.depth.reset(ImageUtils::convertDepth(depth_data, w, h));

        if (!image_pair.color || !image_pair.depth)
        {
            if (error_message) *error_message = "Failed to decode image pair: " + color_path + " / " + depth_path;
            return false;
        }

        return true;
    }
    catch (const std::exception& e)
    {
        if (error_message) *error_message = std::string("Failed to load image pair: ") + e.what();
        return false;
    }
}

bool SceneFrameProvider::parsePoseLineToLookAt(const std::string& line,
                                               std::string& timestamp,
                                               std::array<double, 9>& lookat_values)
{
    std::istringstream line_stream(line);
    std::vector<double> values;
    std::string token;
    int count = 0;

    while (line_stream >> token)
    {
        if (count == 0)
        {
            timestamp = token;
        }
        else
        {
            try
            {
                values.push_back(std::stod(token));
            }
            catch (...)
            {
                return false;
            }
        }
        ++count;
    }

    if (values.size() < 9)
    {
        return false;
    }

    lookat_values[0] = values[0] + values[6];
    lookat_values[1] = values[1] + values[7];
    lookat_values[2] = values[2] + values[8];
    lookat_values[3] = values[0];
    lookat_values[4] = values[1];
    lookat_values[5] = values[2];
    lookat_values[6] = values[3];
    lookat_values[7] = values[4];
    lookat_values[8] = values[5];
    return true;
}

bool SceneFrameProvider::readBinaryFile(const std::string& path, std::string& out_data, std::string* error_message)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        if (error_message) *error_message = "Failed to open file: " + path;
        return false;
    }

    out_data.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}

void SceneFrameProvider::clear()
{
    frames_.clear();
    current_frame_index_ = 0;
    playback_type_ = PlaybackType::DynamicVideo;
}
