#include <vsgRendererClient.h>

class Rendering {
public:
    vsgRendererClient renderer;
    int width = 640;
    int height = 480;
    double upsample_scale = 2;

public:

    Rendering();

    ~Rendering();

    int Init(vsg::ref_ptr<vsg::Device> device);

    int Update(std::vector<std::vector<uint8_t>> &vPacket);
};
