#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2022 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <cstdint>
#include <vsg/core/Mask.h>

//constexpr vsg::Mask MASK_SHADOW_CASTER = 1ul;
//constexpr vsg::Mask MASK_SHADOW_RECEIVER = 2ul;
//constexpr vsg::Mask MASK_BACKGROUND = 4ul;
//constexpr vsg::Mask MASK_MODEL = MASK_SHADOW_CASTER | MASK_SHADOW_RECEIVER;
//constexpr vsg::Mask MASK_GEOMETRY = MASK_SHADOW_RECEIVER | MASK_BACKGROUND;

#ifndef MYMASK_H
#define MYMASK_H
constexpr vsg::Mask MASK_PBR_FULL = 1ul;
constexpr vsg::Mask MASK_SHADOW_CASTER = 2ul;
constexpr vsg::Mask MASK_SHADOW_RECEIVER = 4ul;
constexpr vsg::Mask MASK_DRAW_SHADOW = 4ul;
constexpr vsg::Mask MASK_FAKE_BACKGROUND = 8ul;
constexpr vsg::Mask MASK_WIREFRAME = 16ul;
constexpr vsg::Mask MASK_CAMERA_IMAGE = 32ul;

// constexpr vsg::Mask MASK_MODEL = MASK_PBR_FULL | MASK_SHADOW_CASTER;
constexpr vsg::Mask MASK_MODEL = MASK_PBR_FULL | MASK_SHADOW_CASTER;
constexpr vsg::Mask MASK_GEOMETRY = MASK_DRAW_SHADOW | MASK_FAKE_BACKGROUND | MASK_SHADOW_CASTER;
constexpr vsg::Mask MASK_SKYBOX = MASK_FAKE_BACKGROUND;

enum mergeShaderType{
    FULL_MODEL, 
    CAMERA_DEPTH,
    CAD_DAPTH
};
#endif // MYMASK_H
