#include "Rendering.h"

Rendering::Rendering() = default;

Rendering::~Rendering() = default;

int Rendering::Init(vsg::ref_ptr<vsg::Device> device){
    renderer.setWidthAndHeight(width, height, upsample_scale);
    renderer.initRenderer(device);
    return 0;
}

int Rendering::Update(std::vector<std::vector<uint8_t>> &vPacket){    
    if(!renderer.render(vPacket))
        return -1;

    return 0;
}