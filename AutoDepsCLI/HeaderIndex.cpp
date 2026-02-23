#include "HeaderIndex.h"
#include <iostream>

static bool IsHeaderFile(const fs::path& p)
{
    const std::string ext = p.extension().string();
    return ext == ".h" || ext == ".hpp" || ext == ".inl";
}

bool HeaderIndex::ShouldSkipPath(const fs::path& p)
{
    // 中文注释：这里用最常见的噪声目录过滤，避免扫到第三方/中间产物/二进制等
    const std::string s = p.generic_string();

    auto has = [&](const char* token)
    {
        return s.find(token) != std::string::npos;
    };

    // 注意：这里是“目录级”过滤，命中就直接跳过
    if (has("/ThirdParty/") || has("/Intermediate/") || has("/Binaries/") || has("/DerivedDataCache/"))
        return true;

    // Programs 一般不该作为游戏依赖来源（工具链）
    if (has("/Programs/"))
        return true;

    return false;
}

void HeaderIndex::Reset()
{
    Index.clear();
    ProjectHeaderFiles = 0;
    EngineHeaderFiles = 0;
}

size_t HeaderIndex::GetConflictKeyCount() const
{
    size_t c = 0;
    for (const auto& kv : Index)
    {
        if (kv.second.size() > 1)
            ++c;
    }
    return c;
}

void HeaderIndex::AddModulePublicDir(const std::string& moduleName,
                                     const fs::path& publicDir,
                                     bool bIsEngine)
{
    ScanPublicDir(moduleName, publicDir, bIsEngine);
}

void HeaderIndex::ScanPublicDir(const std::string& moduleName,
                                const fs::path& publicDir,
                                bool bIsEngine)
{
    if (!fs::exists(publicDir))
        return;

    std::error_code ec;
    fs::recursive_directory_iterator it(publicDir, fs::directory_options::skip_permission_denied, ec);
    fs::recursive_directory_iterator end;

    for (; it != end; it.increment(ec))
    {
        if (ec) { ec.clear(); continue; }

        const fs::path p = it->path();

        // 目录过滤：命中噪声目录就不深入
        if (it->is_directory(ec))
        {
            if (ShouldSkipPath(p))
                it.disable_recursion_pending();
            continue;
        }

        if (!it->is_regular_file(ec))
            continue;

        if (!IsHeaderFile(p))
            continue;

        // key = 相对 Public 目录的路径
        fs::path rel = fs::relative(p, publicDir, ec);
        if (ec) { ec.clear(); continue; }

        std::string key = rel.generic_string();

        // 跳过 generated
        if (key.find(".generated.") != std::string::npos)
            continue;

        HeaderOwner owner;
        owner.ModuleName = moduleName;
        owner.bIsEngine = bIsEngine;

        Index[key].push_back(owner);

        if (bIsEngine) EngineHeaderFiles++;
        else ProjectHeaderFiles++;
    }
}

void HeaderIndex::BuildProject(const std::vector<fs::path>& moduleRoots,
                               const std::vector<std::string>& moduleNames)
{
    // 中文注释：project 的模块 root 是 <ModuleRoot>（Build.cs 所在目录）
    // Public 位于 <ModuleRoot>/Public
    if (moduleRoots.size() != moduleNames.size())
    {
        std::cout << "[HeaderIndex] BuildProject: invalid input sizes.\n";
        return;
    }

    for (size_t i = 0; i < moduleRoots.size(); ++i)
    {
        const fs::path publicDir = moduleRoots[i] / "Public";
        ScanPublicDir(moduleNames[i], publicDir, false);
    }
}

void HeaderIndex::ScanEngineSourceGroup(const fs::path& engineSourceRoot,
                                        const std::string& groupName,
                                        bool bEnabled)
{
    if (!bEnabled) return;

    // 目标结构：Engine/Source/<groupName>/<ModuleName>/Public/**
    // 例如：Engine/Source/Runtime/Core/Public/**
    const fs::path groupRoot = engineSourceRoot / groupName;
    if (!fs::exists(groupRoot))
        return;

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(groupRoot, ec))
    {
        if (ec) { ec.clear(); break; }
        if (!entry.is_directory(ec)) continue;

        const fs::path moduleRoot = entry.path();            // .../<ModuleName>
        const fs::path publicDir = moduleRoot / "Public";    // .../<ModuleName>/Public
        const std::string moduleName = moduleRoot.filename().string();

        ScanPublicDir(moduleName, publicDir, true);
    }
}

void HeaderIndex::ScanEnginePlugins(const fs::path& enginePluginsRoot)
{
    // 目标结构：Engine/Plugins/**/Source/<ModuleName>/Public/**
    // 例如：Engine/Plugins/EnhancedInput/Source/EnhancedInput/Public/**
    if (!fs::exists(enginePluginsRoot))
        return;

    std::error_code ec;
    fs::recursive_directory_iterator it(enginePluginsRoot, fs::directory_options::skip_permission_denied, ec);
    fs::recursive_directory_iterator end;

    for (; it != end; it.increment(ec))
    {
        if (ec) { ec.clear(); continue; }

        const fs::path p = it->path();

        // 目录过滤：命中噪声目录就不深入
        if (it->is_directory(ec))
        {
            if (ShouldSkipPath(p))
                it.disable_recursion_pending();
            continue;
        }

        if (!it->is_regular_file(ec))
            continue;

        // 我们不在这里扫文件，而是找 Public 目录再去 ScanPublicDir（避免重复）
        // 所以这里用目录判断：如果当前路径是 .../Public 并且其父是模块目录（.../<ModuleName>/Public）
        // 但 recursive_directory_iterator 走的是文件/目录混合，所以我们改成：遇到目录时处理。
    }

    // 上面那种“遍历文件”不好做 Public 目录捕获，我们换一种更稳的：递归遍历目录，发现 Public 目录就处理并停止深入。
    ec.clear();
    fs::recursive_directory_iterator dit(enginePluginsRoot, fs::directory_options::skip_permission_denied, ec);
    for (; dit != end; dit.increment(ec))
    {
        if (ec) { ec.clear(); continue; }
        if (!dit->is_directory(ec)) continue;

        const fs::path dirPath = dit->path();

        if (ShouldSkipPath(dirPath))
        {
            dit.disable_recursion_pending();
            continue;
        }

        // 识别：.../Source/<ModuleName>/Public
        if (dirPath.filename() == "Public")
        {
            const fs::path moduleRoot = dirPath.parent_path();          // .../Source/<ModuleName>
            const fs::path sourceRoot = moduleRoot.parent_path();       // .../Source
            if (sourceRoot.filename() == "Source")
            {
                const std::string moduleName = moduleRoot.filename().string();
                ScanPublicDir(moduleName, dirPath, true);

                // Public 已经扫过了，不必继续深入 Public 子目录（ScanPublicDir 自己会递归）
                dit.disable_recursion_pending();
            }
        }
    }
}

void HeaderIndex::BuildEngine(const fs::path& engineRoot,
                              bool bIncludeDeveloper,
                              bool bIncludeEditor)
{
    // 中文注释：
    // 这里 engineRoot 必须是 “包含 Engine 文件夹的那一层”
    // 我们要找的是：<engineRoot>/Engine/Source 和 <engineRoot>/Engine/Plugins

    const fs::path engineDir = engineRoot / "Engine";
    const fs::path sourceRoot = engineDir / "Source";
    const fs::path pluginsRoot = engineDir / "Plugins";

    if (!fs::exists(engineDir))
    {
        std::cout << "[HeaderIndex] Engine folder not found: " << engineDir.string() << "\n";
        return;
    }
    if (!fs::exists(sourceRoot))
    {
        std::cout << "[HeaderIndex] Engine Source not found: " << sourceRoot.string() << "\n";
        return;
    }

    // 1) Engine/Source 主要模块来源
    // 默认只扫 Runtime（可卖工具默认不建议把 Editor/Developer 加到游戏依赖里）
    ScanEngineSourceGroup(sourceRoot, "Runtime", true);
    ScanEngineSourceGroup(sourceRoot, "Developer", bIncludeDeveloper);
    ScanEngineSourceGroup(sourceRoot, "Editor", bIncludeEditor);

    // 2) Engine/Plugins 很关键：EnhancedInput/CommonUI 等都在这里
    if (fs::exists(pluginsRoot))
    {
        ScanEnginePlugins(pluginsRoot);
    }
    else
    {
        std::cout << "[HeaderIndex] Engine Plugins not found: " << pluginsRoot.string() << "\n";
    }
}