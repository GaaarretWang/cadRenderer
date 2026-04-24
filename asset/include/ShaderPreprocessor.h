#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vsg/io/Logger.h>

class ShaderPreprocessor
{
public:
    ShaderPreprocessor(const std::string& shadersDir);

    bool processAll();

    std::string processFile(const std::filesystem::path& filepath);

private:
    bool isShaderFile(const std::filesystem::path& filepath) const;
    bool writeIfChanged(const std::filesystem::path& filepath, const std::string& source);
    std::string resolveIncludes(const std::string& source, int depth = 0);
    std::string loadInclude(const std::string& includeName);

    std::filesystem::path m_shadersDir;
    std::filesystem::path m_includesDir;
    std::filesystem::path m_outputDir;
    std::unordered_map<std::string, std::string> m_includeCache;
    bool m_failed = false;

    static constexpr int MAX_INCLUDE_DEPTH = 10;
};
