#include "Rendering.h"
#include "RenderingServer.h"
#include <iostream>
#include <vsg/all.h>
#include <CADMesh.h>
#include "communication/dataInterface.h"
#include "ImageUtils.h"
#include <cstdlib>
#include <chrono>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// #define RENDER_TEST
// simplelogger::Logger* logger = simplelogger::LoggerFactory::CreateConsoleLogger();

namespace {

enum class CaptureMode {
    None,
    SaveRef,
    CompareRef
};

struct ProgramOptions {
    std::string scene_id = "0";
    int max_frames = 0;
    CaptureMode capture_mode = CaptureMode::None;
    double diff_threshold = 0.01;
};

struct RenderLoopResult {
    int exit_code = 0;
    int total_frames_rendered = 0;
};

bool parseInteger(const std::string& text, int& value)
{
    try
    {
        size_t parsed_length = 0;
        value = std::stoi(text, &parsed_length);
        return parsed_length == text.size();
    }
    catch (...)
    {
        return false;
    }
}

bool parseDouble(const std::string& text, double& value)
{
    try
    {
        size_t parsed_length = 0;
        value = std::stod(text, &parsed_length);
        return parsed_length == text.size();
    }
    catch (...)
    {
        return false;
    }
}

bool parseProgramOptions(int argc, char** argv, ProgramOptions& options, std::string& error_message)
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--scene" || arg == "-s")
        {
            if (i + 1 >= argc)
            {
                error_message = "Missing value for " + arg;
                return false;
            }
            options.scene_id = argv[++i];
        }
        else if (arg == "--frames" || arg == "-f")
        {
            if (i + 1 >= argc)
            {
                error_message = "Missing value for " + arg;
                return false;
            }
            if (!parseInteger(argv[++i], options.max_frames))
            {
                error_message = "Invalid integer value for " + arg + ": " + argv[i];
                return false;
            }
        }
        else if (arg == "--save-ref")
        {
            if (options.capture_mode == CaptureMode::CompareRef)
            {
                error_message = "--save-ref and --compare-ref cannot be used together";
                return false;
            }
            options.capture_mode = CaptureMode::SaveRef;
        }
        else if (arg == "--compare-ref")
        {
            if (options.capture_mode == CaptureMode::SaveRef)
            {
                error_message = "--save-ref and --compare-ref cannot be used together";
                return false;
            }
            options.capture_mode = CaptureMode::CompareRef;
        }
        else if (arg == "--diff-threshold")
        {
            if (i + 1 >= argc)
            {
                error_message = "Missing value for " + arg;
                return false;
            }
            if (!parseDouble(argv[++i], options.diff_threshold))
            {
                error_message = "Invalid floating-point value for " + arg + ": " + argv[i];
                return false;
            }
            if (options.diff_threshold < 0.0)
            {
                error_message = "--diff-threshold must be non-negative";
                return false;
            }
        }
    }

    if (options.capture_mode != CaptureMode::None && options.max_frames <= 0)
    {
        error_message = "--frames must be greater than 0 when using --save-ref or --compare-ref";
        return false;
    }

    return true;
}

RenderLoopResult runRenderLoop(
    RenderingServer& rendering_server,
#ifndef RENDER_TEST
    Rendering* rendering_client,
#endif
    const ProgramOptions& options)
{
    RenderLoopResult result;
    auto start_time = std::chrono::high_resolution_clock::now();
    int frame_count = 0;

    while (options.max_frames <= 0 || result.total_frames_rendered < options.max_frames)
    {
        if (rendering_server.Update() == -1)
        {
            std::cerr << "ERROR: rendering server update failed" << std::endl;
            result.exit_code = 1;
            return result;
        }

#ifndef RENDER_TEST
        if (!rendering_server.vPacket.empty() && rendering_client->Update(rendering_server.vPacket) == -1)
        {
            std::cerr << "ERROR: rendering client update failed" << std::endl;
            result.exit_code = 1;
            return result;
        }
#endif

        ++frame_count;
        ++result.total_frames_rendered;

        const auto current_time = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<float> elapsed_time = current_time - start_time;
        if (elapsed_time.count() >= 1.0f)
        {
            const float fps = frame_count / elapsed_time.count();
            gui::global_params->currentFps = fps;
            std::cout << "FPS: " << fps << std::endl;

            frame_count = 0;
            start_time = current_time;
        }
    }

    return result;
}

int executeCaptureMode(const ProgramOptions& options, RenderingServer& rendering_server)
{
    if (options.capture_mode == CaptureMode::None)
    {
        return 0;
    }

    const int render_width = rendering_server.renderer.render_width;
    const int render_height = rendering_server.renderer.render_height;
    if (render_width <= 0 || render_height <= 0)
    {
        std::cerr << "FAIL: invalid render target size" << std::endl;
        return 1;
    }

    std::vector<uint8_t> window_image(static_cast<size_t>(render_width) * render_height * 4);
    rendering_server.renderer.getWindowImage(window_image.data());

    const std::string ref_dir = "../test_references";
    const std::string ref_path = ref_dir + "/scene_" + options.scene_id + ".png";

    if (options.capture_mode == CaptureMode::SaveRef)
    {
        if (!ImageUtils::savePNG(ref_path, window_image.data(), render_width, render_height, 4))
        {
            std::cerr << "Failed to save reference: " << ref_path << std::endl;
            return 1;
        }

        std::cout << "Reference saved: " << ref_path << std::endl;
        return 0;
    }

    const auto compare_result = ImageUtils::compareWithRef(
        window_image.data(),
        render_width,
        render_height,
        4,
        ref_path);

    if (!compare_result.valid)
    {
        std::cerr << "FAIL: " << compare_result.message << std::endl;
        return 1;
    }

    std::ostringstream detail_stream;
    detail_stream << compare_result.message << ", threshold "
                  << (options.diff_threshold * 100.0) << "%";

    if (compare_result.diff_ratio > options.diff_threshold)
    {
        std::cerr << "FAIL: " << detail_stream.str() << std::endl;
        return 1;
    }

    std::cout << "PASS: " << detail_stream.str() << std::endl;
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        ProgramOptions options;
        std::string parse_error;
        if (!parseProgramOptions(argc, argv, options, parse_error))
        {
            std::cerr << "ERROR: " << parse_error << std::endl;
            return 1;
        }

        const auto finishAndExit = [&options](int exit_code) -> int {
            if (options.max_frames > 0)
            {
                std::cout.flush();
                std::cerr.flush();
                std::quick_exit(exit_code);
            }
            return exit_code;
        };

        const double render_scale = 2;
        const double encode_scale = 2;

        RenderingServer rendering_server;
        rendering_server.upsample_scale = render_scale;
        rendering_server.encode_scale = encode_scale;
        if (rendering_server.Init(options.scene_id) != 0)
        {
            std::cerr << "ERROR: failed to initialize rendering server for scene " << options.scene_id << std::endl;
            return finishAndExit(1);
        }

#ifndef RENDER_TEST
        Rendering rendering_client;
        rendering_client.upsample_scale = encode_scale;
        if (rendering_client.Init(rendering_server.device) != 0)
        {
            std::cerr << "ERROR: failed to initialize rendering client" << std::endl;
            return finishAndExit(1);
        }
#endif

        const auto render_result = runRenderLoop(
            rendering_server,
#ifndef RENDER_TEST
            &rendering_client,
#endif
            options);
        if (render_result.exit_code != 0)
        {
            return finishAndExit(render_result.exit_code);
        }

        std::cout << "Program stopped after " << render_result.total_frames_rendered << " frames" << std::endl;

        return finishAndExit(executeCaptureMode(options, rendering_server));
    }
    catch (const std::exception& e)
    {
        std::cerr << "FATAL: " << e.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "FATAL: unknown exception" << std::endl;
        return 1;
    }
}
