#pragma  once
#include "vsg/all.h"

class ConfigShader{
public:
    vsg::ref_ptr<vsg::ShaderSet> buildShader(std::string vert, std::string frag);
    vsg::ref_ptr<vsg::ShaderSet> buildIntgShader(std::string vert, std::string frag);
};