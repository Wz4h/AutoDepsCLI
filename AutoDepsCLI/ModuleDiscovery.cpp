#include "ModuleDiscovery.h"
#include <algorithm>

// 判断字符串是否以 suffix 结尾
static bool EndsWith(const std::string& s, const std::string& suffix)
{
    if (s.size() < suffix.size()) return false;
    return std::equal(suffix.rbegin(), suffix.rend(), s.rbegin());
}

// 从 xxx.Build.cs 提取模块名
static std::string GetModuleNameFromBuildCs(const fs::path& buildCsPath)
{
    std::string filename = buildCsPath.filename().string();
    const std::string suffix = ".Build.cs";

    if (EndsWith(filename, suffix))
        filename.resize(filename.size() - suffix.size());

    return filename;
}

// 添加模块信息
static void AddModuleIfValid(std::vector<ModuleInfo>& outModules,
                             const fs::path& buildCsPath,
                             bool bIsPlugin,
                             const std::string& pluginName)
{
    if (!fs::exists(buildCsPath)) return;

    ModuleInfo info;
    info.Name = GetModuleNameFromBuildCs(buildCsPath);
    info.BuildCsPath = fs::absolute(buildCsPath);
    info.ModuleRoot = fs::absolute(buildCsPath.parent_path());
    info.bIsPluginModule = bIsPlugin;
    info.PluginName = pluginName;

    outModules.push_back(std::move(info));
}

// 扫描项目 + 插件模块
std::vector<ModuleInfo> DiscoverProjectModules(const fs::path& projectRoot)
{
    std::vector<ModuleInfo> modules;

    // ===== 1. 扫描项目模块 =====
    fs::path sourceRoot = projectRoot / "Source";

    if (fs::exists(sourceRoot))
    {
        for (const auto& dir : fs::directory_iterator(sourceRoot))
        {
            if (!dir.is_directory()) continue;

            for (const auto& file : fs::directory_iterator(dir.path()))
            {
                if (!file.is_regular_file()) continue;

                if (EndsWith(file.path().filename().string(), ".Build.cs"))
                {
                    AddModuleIfValid(modules, file.path(), false, "");
                }
            }
        }
    }

    // ===== 2. 扫描插件模块 =====
    fs::path pluginsRoot = projectRoot / "Plugins";

    if (fs::exists(pluginsRoot))
    {
        for (const auto& entry : fs::recursive_directory_iterator(pluginsRoot))
        {
            if (!entry.is_regular_file()) continue;

            fs::path filePath = entry.path();
            if (!EndsWith(filePath.filename().string(), ".Build.cs"))
                continue;

            std::string pluginName;

            // 插件名取 Plugins 下第一层目录
            fs::path relative = fs::relative(filePath, pluginsRoot);
            if (!relative.empty())
            {
                auto it = relative.begin();
                if (it != relative.end())
                    pluginName = it->string();
            }

            AddModuleIfValid(modules, filePath, true, pluginName);
        }
    }

    return modules;
}