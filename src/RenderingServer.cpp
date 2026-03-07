#include "RenderingServer.h"
#include <json.hpp>
#include <fstream>
#include <algorithm>
#include <cctype>

RenderingServer::RenderingServer() = default;

RenderingServer::~RenderingServer() = default;

bool RenderingServer::loadSceneFromJSON(const std::string& scene_name_or_id) {
    std::string json_path = rendering_dir + "asset/data/Scenes.json";
    std::ifstream json_file(json_path);

    if (!json_file.is_open()) {
        std::cerr << "错误：无法打开场景配置文件 " << json_path << std::endl;
        return false;
    }

    nlohmann::json json_data = nlohmann::json::parse(json_file);
    json_file.close();

    // 清空现有场景数据
    clearSceneData();

    // 查找场景
    nlohmann::json* target_scene = nullptr;

    // 尝试按ID查找
    if (std::all_of(scene_name_or_id.begin(), scene_name_or_id.end(), ::isdigit)) {
        int scene_id = std::stoi(scene_name_or_id);
        for (auto& scene : json_data["scenes"]) {
            if (scene["id"] == scene_id) {
                target_scene = &scene;
                break;
            }
        }
    }

    // 尝试按名称查找
    if (!target_scene) {
        for (auto& scene : json_data["scenes"]) {
            if (scene["name"] == scene_name_or_id) {
                target_scene = &scene;
                break;
            }
        }
    }

    if (!target_scene) {
        std::cerr << "错误：未找到场景 '" << scene_name_or_id << "'" << std::endl;
        return false;
    }

    // 加载场景中的模型
    for (auto& model : (*target_scene)["models"]) {
        std::string model_path = rendering_dir + model["path"].get<std::string>();
        model_paths.push_back(model_path);

        // 计算最终变换矩阵
        vsg::dmat4 final_transform;
        bool first_matrix = true;

        for (auto& matrix_array : model["transform_sequence"]) {
            vsg::dmat4 current_matrix = parseMatrixFromJSON(matrix_array);

            if (first_matrix) {
                final_transform = current_matrix;
                first_matrix = false;
            } else {
                final_transform = final_transform * current_matrix;
            }
        }

        model_transforms.push_back(final_transform);
        instance_names.push_back(model["instance_name"].get<std::string>());
    }

    // 加载shadow_receiver配置
    if (target_scene->contains("shadow_receiver_path") && target_scene->contains("shadow_receiver_transform")) {
        std::string shadow_path = rendering_dir + (*target_scene)["shadow_receiver_path"].get<std::string>();
        renderer.shadow_recevier_path = shadow_path;

        // 计算shadow_receiver变换矩阵
        vsg::dmat4 shadow_final_transform;
        bool first_shadow_matrix = true;

        for (auto& matrix_array : (*target_scene)["shadow_receiver_transform"]) {
            vsg::dmat4 current_matrix = parseMatrixFromJSON(matrix_array);

            if (first_shadow_matrix) {
                shadow_final_transform = current_matrix;
                first_shadow_matrix = false;
            } else {
                shadow_final_transform = shadow_final_transform * current_matrix;
            }
        }

        renderer.shadow_recevier_transform = shadow_final_transform;
        std::cout << "已设置shadow_receiver: " << shadow_path << std::endl;
    } else {
        // 如果没有配置，使用默认值（保持向后兼容）
        renderer.shadow_recevier_path = rendering_dir + "asset/data/obj/shadow_receiver2.obj";
        renderer.shadow_recevier_transform = vsg::dmat4();
        std::cout << "使用默认shadow_receiver配置" << std::endl;
    }

    std::cout << "成功加载场景 '" << scene_name_or_id << "'，包含 " << model_paths.size() << " 个模型" << std::endl;
    return true;
}

vsg::dmat4 RenderingServer::parseMatrixFromJSON(const nlohmann::json& matrix_array) {
    if (matrix_array.size() != 16) {
        throw std::runtime_error("矩阵数组必须包含16个元素");
    }

    return vsg::dmat4(
        matrix_array[0].get<double>(), matrix_array[1].get<double>(), matrix_array[2].get<double>(), matrix_array[3].get<double>(),
        matrix_array[4].get<double>(), matrix_array[5].get<double>(), matrix_array[6].get<double>(), matrix_array[7].get<double>(),
        matrix_array[8].get<double>(), matrix_array[9].get<double>(), matrix_array[10].get<double>(), matrix_array[11].get<double>(),
        matrix_array[12].get<double>(), matrix_array[13].get<double>(), matrix_array[14].get<double>(), matrix_array[15].get<double>()
    );
}

int RenderingServer::Init(int argc, char** argv){
    renderer.setWidthAndHeight(width, height, upsample_scale, encode_scale);
    renderer.setKParameters(fx, fy, cx, cy);

    // 场景数据已迁移到JSON文件，通过命令行参数加载

    vsg::CommandLine arguments(&argc, argv);

    // 添加命令行参数处理
    std::string scene_to_load = "0"; // 默认加载第一个场景
    arguments.read("--scene", scene_to_load);
    arguments.read("-s", scene_to_load);

    // 加载场景
    if (!loadSceneFromJSON(scene_to_load)) {
        std::cerr << "错误：无法加载场景 '" << scene_to_load << "'，程序将退出" << std::endl;
        return -1;
    }

    renderer.setUpShader(rendering_dir);
    renderer.shader_type = shader_type;
    renderer.cull_mode_none_model_paths.insert(rendering_dir + "asset/data/geos/1105/twoAirplaneBody.fb");
    renderer.cull_mode_none_model_paths.insert(rendering_dir + "asset/data/geos/1105/twoAirplaneBody_white.fb");
    renderer.cull_mode_none_model_paths.insert(rendering_dir + "asset/data/geos/1105/window.fb");

    // renderer.cull_mode_none_model_paths.insert(rendering_dir + "asset/data/geos/1105/window.fb");
    renderer.initRenderer(rendering_dir, model_transforms, model_paths, instance_names, vsg::dmat4());
    
    device = renderer.device;
    return 0;
}

int RenderingServer::Update(){
    // if(frame_count == 1000){
    //     renderer.hdr_image_num = 0;
    //     renderer.updateEnvLighting();
    // }

    // model_transforms[0][3][2] += 0.005;
    // if(model_transforms[0][3][2] > -0.45)
    //     model_transforms[0][3][2] = -0.679909;
    // renderer.updateObjectPose("YIBIAOPAN1", model_transforms[0]);
    // renderer.repaint("YIBIAOPAN1", 1);

    // static PlaneData planeData = createTestPlanes();
    // static float subdivisions = 0.1;
    // static PlaneData subdividedPlaneData = subdividePlanes(planeData, subdivisions);
    // static MeshData mesh = convertPlaneDataToMesh(subdividedPlaneData);
    // float* vertices_pointer = static_cast<float*>(mesh.vertices->dataPointer(0));
    // size_t vertices_size = mesh.vertices->size() * 3;
    // uint32_t* indices_pointer = static_cast<uint32_t*>(mesh.indices->dataPointer(0));
    // size_t indices_size = mesh.indices->size();
    // renderer.addLineData(vertices_pointer, vertices_size, indices_pointer, indices_size);
    // renderer.addPointData(vertices_pointer, vertices_size, indices_pointer, indices_size);

    // std::vector<std::string> texts{"aaaaaaa"};
    // std::vector<vsg::ref_ptr<vsg::StandardLayout>> dynamic_text_layouts(1, vsg::StandardLayout::create());
    // dynamic_text_layouts[0]->billboard = true;
    // dynamic_text_layouts[0]->position = vsg::vec3(0.0, 0.0, 0.0);
    // dynamic_text_layouts[0]->horizontal = vsg::vec3(1.0, 0.0, 0.0);
    // dynamic_text_layouts[0]->vertical = vsg::vec3(0.0, 1.0, 0.0);
    // dynamic_text_layouts[0]->color = vsg::vec4(1.0, 0.9, 1.0, 1.0);
    // dynamic_text_layouts[0]->outlineWidth = 0.1;
    // renderer.addTextData(texts, dynamic_text_layouts);

    // a proto instance movement
    // vsg::dmat4 matrix = vsg::dmat4(0.173648, 0, 0, 0,
    //                                0.984808, 0, -0.173648, 0,
    //                                0, 1, 0, 0,
    //                                -238.327, -215.6, 22.5233, 1);
    // static double z_offset = 0.0;
    // z_offset += 0.5;
    // if(z_offset > 20)
    //     z_offset = 0;
    // matrix[3][2] += z_offset;
    // renderer.updateObjectPose("YIBIAOPAN128A9D3E8-D181-40BA-A99F-DDDF0B3F3384352", matrix);
    // renderer.repaint("YIBIAOPAN128A9D3E8-D181-40BA-A99F-DDDF0B3F3384352", 1);
    auto cameralookat = renderer.camera->viewMatrix.cast<vsg::LookAt>();
    if(cameara_pos_bool){//停止位姿变化
        vsg::dvec3 centre = {lookat_vector[0], lookat_vector[1], lookat_vector[2]};                    // 固定观察点
        vsg::dvec3 eye = {lookat_vector[3], lookat_vector[4], lookat_vector[5]};// 固定相机位置
        vsg::dvec3 up = {lookat_vector[6], lookat_vector[7], lookat_vector[8]};                       // 固定观察方向
        renderer.updateCamera(centre, eye, up);
    renderer.updateCamera(vsg::dvec3(-0.326470, 0.600668, -0.649556), 
                        vsg::dvec3(1.197728, -0.131292, -0.120896), 
                        vsg::dvec3(-0.246774, 0.174593, 0.953216));

        auto color_pixels = all_images[frame_count % all_images.size()].color.get();
        auto depth_pixels = all_images[frame_count % all_images.size()].depth.get();
        renderer.setRealColorAndImage(color_pixels, depth_pixels);

        static auto last_frame_time = std::chrono::steady_clock::now();
        auto current_time = std::chrono::steady_clock::now();
        auto frame_time = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_frame_time).count();
        if (frame_time < 33) {
            std::this_thread::sleep_for(std::chrono::milliseconds(33 - frame_time));
        }
        current_time = std::chrono::steady_clock::now();
        last_frame_time = current_time;

        if(stop_cameara_pos)
            cameara_pos_bool = false;
    }


    auto startRender = std::chrono::high_resolution_clock::now();
    if(!renderer.render())
        return -1;
    auto endRender = std::chrono::high_resolution_clock::now();
    gui::global_params->render_server_times[0] = std::chrono::duration<double, std::milli>(endRender - startRender).count();

    auto startEncode = std::chrono::high_resolution_clock::now();
    renderer.getEncodeImage(vPacket);
    auto endEncode = std::chrono::high_resolution_clock::now();
    gui::global_params->render_server_times[1] = std::chrono::duration<double, std::milli>(endEncode - startEncode).count();

    num++;
    // if(num % 400 == 0)
    // {
    //     renderer.hdr_image_num = 4;
    //     renderer.updateEnvLighting();
    //     std::cout << "update env lighting, hdr_image_num: " << renderer.hdr_image_num << std::endl;
    // }
    // else if(num % 200 == 0)
    // {
    //     renderer.hdr_image_num = 5;
    //     renderer.updateEnvLighting();
    //     std::cout << "update env lighting, hdr_image_num: " << renderer.hdr_image_num << std::endl;
    // }
    // else if(num == 6000)
    // {
    //     num = 0;
    //     renderer.hdr_image_num = 2;
    //     renderer.updateEnvLighting();
    //     std::cout << "update env lighting, hdr_image_num: " << renderer.hdr_image_num << std::endl;
    // }

    frame_count ++;
    // std::cout << "frame_count " << frame_count << std::endl;
    return 0;
}

void RenderingServer::clearSceneData() {
    model_paths.clear();
    model_transforms.clear();
    instance_names.clear();
    // 注意：不清除 shadow_receiver_path 和 shadow_receiver_transform，因为它们会在 loadSceneFromJSON 中被覆盖
}