#include "Rendering.h"
#include "RenderingServer.h"
#include <iostream>
#include <vsg/all.h>
#include <CADMesh.h>
#include "communication/dataInterface.h"
#include "ImageUtils.h"
#include <string>
#include <chrono>
#include <fstream>
#include <vector>
#include <memory>
#include <stdexcept>
// #define RENDER_TEST
// simplelogger::Logger* logger = simplelogger::LoggerFactory::CreateConsoleLogger();

int main(int argc, char** argv){
    try {
        // Parse PNG test arguments manually so other modules can still consume their own flags.
        bool save_ref = false;
        bool compare_ref = false;
        double diff_threshold = 0.01; // Difference-ratio threshold, default 1%.
        std::string scene_id_str = "0";
        for(int i = 1; i < argc; i++){
            std::string arg = argv[i];
            if(arg == "--save-ref") save_ref = true;
            else if(arg == "--compare-ref") compare_ref = true;
            else if(arg == "--diff-threshold" && i + 1 < argc) diff_threshold = std::stod(argv[++i]);
            else if((arg == "--scene" || arg == "-s") && i + 1 < argc) scene_id_str = argv[i + 1]; // Do not consume it here; RenderingServer still needs it.
        }

    // Parse command-line arguments.
    vsg::CommandLine arguments(&argc, argv);
    int max_frames = 0; // Zero means run indefinitely.
    arguments.read("--frames", max_frames);
    arguments.read("-f", max_frames);

    double render_scale = 2;
    double encode_scale = 2;
    RenderingServer rendering_server;
    rendering_server.upsample_scale = render_scale;
    rendering_server.encode_scale = encode_scale;
    rendering_server.Init(argc, argv);
#ifndef RENDER_TEST
    Rendering rendering_client;
    rendering_client.upsample_scale = encode_scale;
    rendering_client.Init(rendering_server.device);
#endif

    auto startTime = std::chrono::high_resolution_clock::now();
    int frameCount = 0;
    int total_frames_rendered = 0;
    while((max_frames <= 0 || total_frames_rendered < max_frames)){
        // Increment the frame counter.
        frameCount++;
        total_frames_rendered++;

        // Compute the elapsed time.
        auto currentTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> elapsedTime = currentTime - startTime;

        // Check whether one second has elapsed.
        if (elapsedTime.count() >= 1.0f)
        {
            // Compute FPS.
            float fps = frameCount / elapsedTime.count();
            gui::global_params->currentFps = fps;

            // Print FPS to the console.
            std::cout << "FPS: " << fps << std::endl;

            // Reset counters for the next second.
            frameCount = 0;
            startTime = currentTime;
        }
        
        if(rendering_server.Update() == -1){
            std::cout << "end" << std::endl;
            return 0;
        }

#ifndef RENDER_TEST
        if(rendering_server.vPacket.size() > 0)
            rendering_client.Update(rendering_server.vPacket);
#endif
    }

    // Save or compare PNG reference images after the loop completes.
    if(save_ref || compare_ref){
        int rw = rendering_server.renderer.render_width;
        int rh = rendering_server.renderer.render_height;
        std::vector<uint8_t> window_image(rw * rh * 4);
        uint8_t* img_ptr = window_image.data();
        rendering_server.renderer.getWindowImage(img_ptr);

        std::string ref_dir = "../test_references";
        std::string ref_path = ref_dir + "/scene_" + scene_id_str + ".png";

        if(save_ref){
            bool ok = ImageUtils::savePNG(ref_path, window_image.data(), rw, rh, 4);
            if(ok){
                std::cout << "Reference saved: " << ref_path << std::endl;
            } else {
                std::cerr << "Failed to save reference: " << ref_path << std::endl;
                return 1;
            }
        }

        if(compare_ref){
            auto cmp = ImageUtils::compareWithRef(window_image.data(), rw, rh, 4, ref_path);
            std::cout << "Compare result: " << cmp.message << std::endl;
            if(!cmp.valid){
                std::cerr << "FAIL: comparison invalid" << std::endl;
                return 1;
            }
            if(cmp.diff_ratio > diff_threshold){
                std::cerr << "FAIL: diff ratio " << (cmp.diff_ratio * 100.0) << "% exceeds threshold "
                          << (diff_threshold * 100.0) << "%" << std::endl;
                return 1;
            } else {
                std::cout << "PASS: diff ratio " << (cmp.diff_ratio * 100.0) << "% within threshold "
                          << (diff_threshold * 100.0) << "%" << std::endl;
            }
        }
    }

    // Stop once the requested frame count is reached, or once a capture-mode run completes.
    std::cout << "Program stopped after " << total_frames_rendered << " frames" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "FATAL: unknown exception" << std::endl;
        return 1;
    }
}
