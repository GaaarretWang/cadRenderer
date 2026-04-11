#include <vsg/io/VkFileLogger.h>
#include <iomanip>
#include <chrono>
#include <ctime>

using namespace vsg;

ref_ptr<VkFileLogger>& VkFileLogger::instance()
{
    static ref_ptr<VkFileLogger> s_instance = VkFileLogger::create();
    return s_instance;
}

VkFileLogger::VkFileLogger()
{
    // 使用 trunc 模式清空之前的日志
    _file.open("vulkan_objects.log", std::ios::out | std::ios::trunc);
    if (_file.is_open())
    {
        _file << "=== Vulkan Object Logging Session Started at "
              << getTimestamp() << " ===\n" << std::flush;
    }
}

VkFileLogger::~VkFileLogger()
{
    if (_file.is_open())
    {
        _file << "=== Session Ended at " << getTimestamp() << " ===\n" << std::flush;
        _file.close();
    }
}

std::string VkFileLogger::getTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::string ts = std::ctime(&time_t);
    if (!ts.empty() && ts.back() == '\n') ts.pop_back(); // 移除换行符
    return ts;
}

void VkFileLogger::logImageCreated(const std::string& name, uint64_t vkImageAddress,
                                   uint32_t format, uint32_t width, uint32_t height, uint32_t depth,
                                   uint32_t mipLevels, uint32_t arrayLayers)
{
    if (!_file.is_open() || name.empty()) return;

    std::ostringstream props;
    props << "format=" << format
          << " extent=" << width << "x" << height << "x" << depth
          << " mipLevels=" << mipLevels
          << " arrayLayers=" << arrayLayers;

    log("VkImage", "Created", name, vkImageAddress, props.str());
}

void VkFileLogger::logImageWrapped(const std::string& name, uint64_t vkImageAddress)
{
    log("VkImage", "Wrapped", name, vkImageAddress);
}

void VkFileLogger::logBufferCreated(const std::string& name, uint64_t vkBufferAddress,
                                   uint64_t size, uint32_t usage)
{
    if (!_file.is_open() || name.empty()) return;

    std::ostringstream props;
    props << "size=" << size << " usage=0x" << std::hex << usage << std::dec;

    log("VkBuffer", "Created", name, vkBufferAddress, props.str());
}

void VkFileLogger::logBufferWrapped(const std::string& name, uint64_t vkBufferAddress)
{
    log("VkBuffer", "Wrapped", name, vkBufferAddress);
}

void VkFileLogger::log(const std::string& objectType, const std::string& action,
                      const std::string& name, uint64_t address,
                      const std::string& properties)
{
    if (!_file.is_open() || name.empty()) return;

    std::scoped_lock<std::mutex> lock(_mutex);
    _file << "[" << objectType << " " << action << "] name='" << name
          << "' address=0x" << std::hex << address << std::dec;

    if (!properties.empty())
    {
        _file << " " << properties;
    }

    _file << "\n" << std::flush;
}

void VkFileLogger::flush()
{
    if (_file.is_open())
    {
        std::scoped_lock<std::mutex> lock(_mutex);
        _file.flush();
    }
}
