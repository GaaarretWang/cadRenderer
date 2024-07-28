#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2019 Thomas Hogarth

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/Command.h>
#include <vsg/state/Image.h>

namespace vsg
{

    /// ClearColorImage command encapsulates vkCmdClearColorImage functionality and associated settings.
    class VSG_DECLSPEC ClearColorImage : public Inherit<Command, ClearColorImage>
    {
    public:
        using Ranges = std::vector<VkImageSubresourceRange>;

        vsg::ref_ptr<vsg::Image> image;
        VkImageLayout imageLayout; // imageLayout must be VK_IMAGE_LAYOUT_GENERAL or VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        VkClearColorValue color;
        Ranges ranges;

        void record(CommandBuffer& commandBuffer) const override;
    };
    VSG_type_name(vsg::ClearColorImage);

    /// ClearDepthStencilImage command encapsulates vkCmdClearDepthStencilImage functionality and associated settings.
    class VSG_DECLSPEC ClearDepthStencilImage : public Inherit<Command, ClearDepthStencilImage>
    {
    public:
        using Ranges = std::vector<VkImageSubresourceRange>;

        vsg::ref_ptr<vsg::Image> image;
        VkImageLayout imageLayout; // imageLayout must be VK_IMAGE_LAYOUT_GENERAL or VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        VkClearDepthStencilValue depthStencil;
        Ranges ranges;

        void record(CommandBuffer& commandBuffer) const override;
    };
    VSG_type_name(vsg::ClearDepthStencilImage);

} // namespace vsg
