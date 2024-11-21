#include <vsg/all.h>

class ImageUtils
{
public:
    vsg::ref_ptr<vsg::Context> context;
    ImageUtils(vsg::ref_ptr<vsg::Window> window)
    {
        context = vsg::Context::create(window->getOrCreateDevice());
    }

    VkCommandBuffer beginSingleTimeCommands()
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = mCommandPool->getCommandPool();
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(mDevice->getDevice(), &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    void endSingleTimeCommands(VkCommandBuffer commandBuffer, uint32_t waitSemaphoreCount = 0, VkSemaphore* pWaitSemaphores = nullptr, VkPipelineStageFlags* pWaitDstStageMask = nullptr, uint32_t signalSemaphoreCount = 0, VkSemaphore* pSignalSemaphores = nullptr)
    {
       vkEndCommandBuffer(commandBuffer);

       VkSubmitInfo submitInfo{};
       submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
       submitInfo.waitSemaphoreCount = waitSemaphoreCount;
       submitInfo.pWaitSemaphores = pWaitSemaphores;
       submitInfo.pWaitDstStageMask = pWaitDstStageMask;
       submitInfo.signalSemaphoreCount = signalSemaphoreCount;
       submitInfo.pSignalSemaphores = pSignalSemaphores;
       submitInfo.commandBufferCount = 1;
       submitInfo.pCommandBuffers = &commandBuffer;

       vkQueueSubmit(mDevice->getGraphicQueue(), 1, &submitInfo, VK_NULL_HANDLE);
       vkQueueWaitIdle(mDevice->getGraphicQueue());

       vkFreeCommandBuffers(mDevice->getDevice(), mCommandPool->getCommandPool(), 1, &commandBuffer);
    }
};
