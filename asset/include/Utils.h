#ifndef UTILS_H
#define UTILS_H

#include "vsg/all.h"

namespace Utils{
    inline vsg::ref_ptr<vsg::Sampler> createLinearSampler()
    {
        static vsg::ref_ptr<vsg::Sampler> sampler = []() {
            auto s = vsg::Sampler::create();
            
            s->magFilter = VK_FILTER_LINEAR;
            s->minFilter = VK_FILTER_LINEAR;
            s->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            
            s->addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            s->addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            s->addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            
            return s;
        }();

        return sampler;
    }

    inline vsg::ref_ptr<vsg::Sampler> createNearestSampler()
    {
        static vsg::ref_ptr<vsg::Sampler> sampler = []() {
            auto s = vsg::Sampler::create();
            
            s->magFilter = VK_FILTER_NEAREST;
            s->minFilter = VK_FILTER_NEAREST;
            s->mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            
            s->addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            s->addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            s->addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            
            return s;
        }();

        return sampler;
    }
}

#endif // UTILS_H