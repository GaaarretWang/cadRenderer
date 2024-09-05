#include <vsgRenderer.h>

class Rendering {
public:
    vsgRenderer renderer;
    int width = 640;
    int height = 480;
    bool use_png = true;
    bool trackingViewMatrix = false;
    bool trackingShader = true;
    std::string* real_color1;
    std::string* real_depth1;
    std::vector<double> lookAtVector = {0.000588, 0.739846, -0.903124, 0.087284, 1.51642, -0.279094, 0.163545, -0.628984, 0.760021};
    float fx = 386.52199190267083;//焦距(x轴上)
    float fy = 387.32300428823663;//焦距(y轴上)
    float cx = 326.5103569741365;//图像中心点(x轴)
    float cy = 237.40293732598795;//图像中心点(y轴)
    std::vector<vsg::dmat4> model_transforms;
    std::vector<vsg::dmat4> init_model_transforms;
    std::vector<std::string> model_paths;
    std::vector<std::string> instance_names;
    std::string renderingDir = "/home/lab/workspace/wgy/cadvsg_intg/";
    std::string colorPath = "/home/lab/workspace/wgy/cadvsg_intg/asset/data/slamData/color/1711699313.948925.png";
    std::string depthPath = "/home/lab/workspace/wgy/cadvsg_intg/asset/data/slamData/depth/1711699313.948925.png";
    double upsample_scale = 2;

public:

    Rendering();

    ~Rendering();

    int Init();

    int Update();
};
