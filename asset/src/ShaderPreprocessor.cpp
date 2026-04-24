#include "ShaderPreprocessor.h"

#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

ShaderPreprocessor::ShaderPreprocessor(const std::string& shadersDir)
    : m_shadersDir(shadersDir)
    , m_includesDir(fs::path(shadersDir) / "includes")
    , m_outputDir(fs::path(shadersDir) / "output")
{
}

bool ShaderPreprocessor::isShaderFile(const fs::path& filepath) const
{
    auto ext = filepath.extension().string();
    return ext == ".frag" || ext == ".vert" || ext == ".comp";
}

bool ShaderPreprocessor::processAll()
{
    m_failed = false;

    std::error_code ec;
    if (!fs::exists(m_shadersDir, ec) || !fs::is_directory(m_shadersDir, ec))
    {
        vsg::error("ShaderPreprocessor: shaders directory not found: ", m_shadersDir.string());
        return false;
    }

    if (!fs::exists(m_includesDir, ec) || !fs::is_directory(m_includesDir, ec))
    {
        vsg::error("ShaderPreprocessor: includes directory not found: ", m_includesDir.string());
        return false;
    }

    fs::create_directories(m_outputDir, ec);
    if (ec)
    {
        vsg::error("ShaderPreprocessor: cannot create output directory ", m_outputDir.string(), ": ", ec.message());
        return false;
    }

    std::vector<fs::path> shaderFiles;
    fs::recursive_directory_iterator end;
    for (fs::recursive_directory_iterator it(m_shadersDir, ec); it != end; it.increment(ec))
    {
        if (ec)
        {
            vsg::error("ShaderPreprocessor: cannot iterate shaders directory: ", ec.message());
            return false;
        }

        const auto& path = it->path();
        if (it->is_directory(ec))
        {
            auto name = path.filename().string();
            if (name == "includes" || name == "output")
            {
                it.disable_recursion_pending();
            }
            continue;
        }

        if (it->is_regular_file(ec) && isShaderFile(path))
        {
            shaderFiles.push_back(path);
        }
    }

    std::sort(shaderFiles.begin(), shaderFiles.end());

    int processed = 0;
    for (const auto& shaderFile : shaderFiles)
    {
        auto result = processFile(shaderFile);
        if (m_failed)
        {
            return false;
        }

        auto relativePath = fs::relative(shaderFile, m_shadersDir, ec);
        if (ec)
        {
            vsg::error("ShaderPreprocessor: cannot build relative path for ", shaderFile.string(), ": ", ec.message());
            return false;
        }

        auto outPath = m_outputDir / relativePath;
        fs::create_directories(outPath.parent_path(), ec);
        if (ec)
        {
            vsg::error("ShaderPreprocessor: cannot create output directory ", outPath.parent_path().string(), ": ", ec.message());
            return false;
        }

        if (!writeIfChanged(outPath, result))
        {
            return false;
        }
        ++processed;
    }

    return processed > 0;
}

std::string ShaderPreprocessor::processFile(const fs::path& filepath)
{
    std::ifstream file(filepath, std::ios::binary);
    if (!file)
    {
        vsg::error("ShaderPreprocessor: cannot open ", filepath.string());
        m_failed = true;
        return "";
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return resolveIncludes(ss.str());
}

bool ShaderPreprocessor::writeIfChanged(const fs::path& filepath, const std::string& source)
{
    {
        std::ifstream existing(filepath, std::ios::binary);
        if (existing)
        {
            std::stringstream ss;
            ss << existing.rdbuf();
            if (ss.str() == source)
            {
                return true;
            }
        }
    }

    std::ofstream out(filepath, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        vsg::error("ShaderPreprocessor: cannot write ", filepath.string());
        return false;
    }

    out << source;
    if (!out)
    {
        vsg::error("ShaderPreprocessor: failed while writing ", filepath.string());
        return false;
    }

    return true;
}

std::string ShaderPreprocessor::resolveIncludes(const std::string& source, int depth)
{
    if (depth > MAX_INCLUDE_DEPTH)
    {
        vsg::error("ShaderPreprocessor: max include depth exceeded");
        m_failed = true;
        return source;
    }

    std::string result;
    result.reserve(source.size());
    std::istringstream stream(source);
    std::string line;

    // Match: #pragma include "filename.glsl"
    std::regex includeRegex("^\\s*#pragma\\s+include\\s+\"([^\"]+)\"\\s*$");

    while (std::getline(stream, line))
    {
        std::smatch match;
        if (std::regex_match(line, match, includeRegex))
        {
            std::string includeName = match[1].str();
            std::string includeContent = loadInclude(includeName);
            if (!includeContent.empty())
            {
                // Recursively resolve includes in the included content
                includeContent = resolveIncludes(includeContent, depth + 1);
                result += "// --- BEGIN GLSL block: " + includeName + " ---\n";
                result += includeContent;
                if (!includeContent.empty() && includeContent.back() != '\n')
                    result += "\n";
                result += "// --- END GLSL block: " + includeName + " ---\n";
            }
            else
            {
                vsg::error("ShaderPreprocessor: include not found: ", includeName);
                m_failed = true;
                return result;
            }
        }
        else
        {
            result += line + "\n";
        }
    }
    return result;
}

std::string ShaderPreprocessor::loadInclude(const std::string& includeName)
{
    auto it = m_includeCache.find(includeName);
    if (it != m_includeCache.end())
        return it->second;

    auto includePath = m_includesDir / includeName;
    std::ifstream file(includePath, std::ios::binary);
    if (!file)
    {
        return "";
    }

    std::stringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();
    m_includeCache[includeName] = content;
    return content;
}
