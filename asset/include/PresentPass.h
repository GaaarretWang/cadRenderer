#ifndef VSG_PRESENT_PASS_H
#define VSG_PRESENT_PASS_H

#include "vsg/all.h"

namespace PresentPass
{
void buildPresentToMsaaData(vsg::ref_ptr<vsg::Options> options,
                            vsg::ref_ptr<vsg::Group> scene,
                            vsg::ref_ptr<vsg::ImageView> sourceColorView,
                            VkSampleCountFlagBits samples);
}

#endif
