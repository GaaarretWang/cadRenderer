#pragma once
#include <vsg/core/Inherit.h>
#include <fstream>
#include <mutex>
#include <cstdint>
#include <string>
#include <sstream>

namespace vsg
{
    /// 通用 Vulkan 对象地址映射文件日志器（单例模式）
    /// 支持记录 VkImage, VkBuffer, VkCommandBuffer 等所有 Vulkan 对象
    class VSG_DECLSPEC VkFileLogger : public Inherit<Object, VkFileLogger>
    {
    public:
        static ref_ptr<VkFileLogger>& instance();

        VkFileLogger();
        ~VkFileLogger();

        /// 记录 VkImage 创建（从 Data 构造后编译）
        void logImageCreated(const std::string& name, uint64_t vkImageAddress,
                            uint32_t format, uint32_t width, uint32_t height, uint32_t depth,
                            uint32_t mipLevels, uint32_t arrayLayers);

        /// 记录 VkImage 包装（从已有 VkImage 构造）
        void logImageWrapped(const std::string& name, uint64_t vkImageAddress);

        /// 记录 VkBuffer 创建
        void logBufferCreated(const std::string& name, uint64_t vkBufferAddress,
                             uint64_t size, uint32_t usage);

        /// 记录 VkBuffer 包装
        void logBufferWrapped(const std::string& name, uint64_t vkBufferAddress);

        /// 通用日志方法：记录任意 Vulkan 对象
        /// objectType: "VkImage", "VkBuffer", "VkCommandBuffer" 等
        /// action: "Created", "Wrapped", "Destroyed" 等
        /// properties: 自定义属性字符串，如 "format=76 extent=1920x1080"
        void log(const std::string& objectType, const std::string& action,
                const std::string& name, uint64_t address,
                const std::string& properties = "");

        /// 强制刷新到磁盘
        void flush();

    protected:
        std::ofstream _file;
        std::mutex _mutex;

        /// 格式化时间戳
        std::string getTimestamp();
    };
    VSG_type_name(vsg::VkFileLogger);

} // namespace vsg
