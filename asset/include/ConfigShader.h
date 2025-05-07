#pragma  once

#include "vsg/all.h"

class ConfigShader{
public:
    vsg::ref_ptr<vsg::ShaderSet> buildShader(std::string vert, std::string frag);
    vsg::ref_ptr<vsg::ShaderSet> buildIntgShader(std::string vert, std::string frag);
    vsg::ref_ptr<vsg::ShaderSet> buildShadowShader(std::string vert, std::string frag);
    vsg::ref_ptr<vsg::ShaderSet> buildModelShader(std::string vert, std::string frag);
    vsg::ref_ptr<vsg::ShaderSet> buildLineShader(std::string vert, std::string frag);
    void frustumCullingComp(std::string comp);
};