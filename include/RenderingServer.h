#include <vsgRendererServer.h>
#include <json.hpp>
#include <chrono>
#include <thread>  // 用于线程睡眠

struct ImagePair {
    std::unique_ptr<unsigned char[]> color;
    std::unique_ptr<unsigned short[]> depth;
};

class RenderingServer {
public:
    vsgRendererServer renderer;
    int width = 640;
    int height = 480;
    bool cameara_pos_bool = true;
    bool stop_cameara_pos = true;
    bool use_png = true;
    mergeShaderType shader_type = CAMERA_DEPTH;

    std::vector<ImagePair> all_images;
    std::string* real_color1;
    std::string* real_depth1;
    std::vector<double> lookat_vector = {0.000588, 0.739846, -0.903124, 0.087284, 1.51642, -0.279094, 0.163545, -0.628984, 0.760021};
    float fx = 386.52199190267083;//焦距(x轴上)
    float fy = 387.32300428823663;//焦距(y轴上)
    float cx = 326.5103569741365;//图像中心点(x轴)
    float cy = 237.40293732598795;//图像中心点(y轴)
    std::vector<vsg::dmat4> model_transforms;
    std::vector<vsg::dmat4> init_model_transforms; // 已弃用：场景数据已迁移到JSON文件
    std::vector<std::string> model_paths;
    std::vector<std::string> instance_names;
    std::string rendering_dir = "../";
    double upsample_scale = 2;
    double encode_scale = 2;
    std::string color;
    std::string depth;

    int frame_count = 0;
    int num = 0;
    vsg::ref_ptr<vsg::Device> device;
    std::vector<std::vector<uint8_t>> vPacket;

private:
    // 场景加载相关方法
    bool loadSceneFromJSON(const std::string& scene_name_or_id);
    vsg::dmat4 parseMatrixFromJSON(const nlohmann::json& matrix_array);
    void clearSceneData();
    int loaded_scene_id = -1;

public:

    RenderingServer();

    ~RenderingServer();

    int Init(int argc, char** argv);

    int Update();
};
