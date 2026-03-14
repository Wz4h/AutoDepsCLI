#include "HeaderIndex.h"
#include <iostream>
#include <vector>

static bool IsHeaderFile(const fs::path& p)
{
    const std::string ext = p.extension().string();
    return ext == ".h" || ext == ".hpp" || ext == ".inl";
}

// 中文注释：统一头文件 key，避免路径分隔符和 "./" 导致匹配不上
static std::string NormalizeHeaderKey(std::string s)
{
    for (char& c : s)
    {
        if (c == '\\')
        {
            c = '/';
        }
    }

    if (s.rfind("./", 0) == 0)
    {
        s = s.substr(2);
    }

    while (!s.empty() &&
           (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n'))
    {
        s.pop_back();
    }

    return s;
}

// 中文注释：
// 一个真实头文件生成多个别名 key，提高 include 命中率。
// 例如：Public/GameFramework/Character.h
// 会生成：
// 1. GameFramework/Character.h
// 2. Character.h
static std::vector<std::string> BuildHeaderAliases(const fs::path& relPath)
{
    std::vector<std::string> keys;
    keys.reserve(2);

    const std::string fullKey = NormalizeHeaderKey(relPath.generic_string());
    if (!fullKey.empty())
    {
        keys.push_back(fullKey);
    }

    const std::string shortKey = NormalizeHeaderKey(relPath.filename().generic_string());
    if (!shortKey.empty() && shortKey != fullKey)
    {
        keys.push_back(shortKey);
    }

    return keys;
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

    // 中文注释：
    // 向索引中注册 owner。
    // 同一个 key 允许多个模块 owner 共存，这样后续 DependencyAnalyzer 才能正确报 Ambiguous。
    // 但同一个模块不能重复插入。
    auto AddOwnerToIndex = [this](const std::string& key, const HeaderOwner& owner)
    {
        auto& owners = Index[key];

        for (const auto& existing : owners)
        {
            if (existing.ModuleName == owner.ModuleName &&
                existing.bIsEngine == owner.bIsEngine)
            {
                return;
            }
        }

        owners.push_back(owner);
    };

    std::error_code ec;
    fs::recursive_directory_iterator it(publicDir, fs::directory_options::skip_permission_denied, ec);
    fs::recursive_directory_iterator end;

    for (; it != end; it.increment(ec))
    {
        if (ec)
        {
            ec.clear();
            continue;
        }

        const fs::path p = it->path();

        // 目录过滤：命中噪声目录就不深入
        if (it->is_directory(ec))
        {
            if (ShouldSkipPath(p))
            {
                it.disable_recursion_pending();
            }
            continue;
        }

        if (!it->is_regular_file(ec))
            continue;

        if (!IsHeaderFile(p))
            continue;

        // key 基于相对目录路径生成
        fs::path rel = fs::relative(p, publicDir, ec);
        if (ec)
        {
            ec.clear();
            continue;
        }

        const auto keys = BuildHeaderAliases(rel);

        HeaderOwner owner;
        owner.ModuleName = moduleName;
        owner.bIsEngine = bIsEngine;

        bool bCounted = false;

        for (const auto& key : keys)
        {
            if (key.empty())
                continue;

            // 跳过 generated
            if (key.find(".generated.") != std::string::npos)
                continue;

            AddOwnerToIndex(key, owner);
            bCounted = true;
        }

        if (bCounted)
        {
            if (bIsEngine) EngineHeaderFiles++;
            else ProjectHeaderFiles++;
        }
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

    // 中文注释：
    // 目标结构：
    // Engine/Source/<groupName>/<ModuleName>/Public/**
    // Engine/Source/<groupName>/<ModuleName>/Classes/**
    const fs::path groupRoot = engineSourceRoot / groupName;
    if (!fs::exists(groupRoot))
        return;

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(groupRoot, ec))
    {
        if (ec)
        {
            ec.clear();
            break;
        }

        if (!entry.is_directory(ec))
            continue;

        const fs::path moduleRoot = entry.path();
        const std::string moduleName = moduleRoot.filename().string();

        ScanPublicDir(moduleName, moduleRoot / "Public", true);
        ScanPublicDir(moduleName, moduleRoot / "Classes", true);
    }
}

void HeaderIndex::ScanEnginePlugins(const fs::path& enginePluginsRoot)
{
    // 中文注释：
    // 目标结构：
    // Engine/Plugins/**/Source/<ModuleName>/Public/**
    // Engine/Plugins/**/Source/<ModuleName>/Classes/**
    if (!fs::exists(enginePluginsRoot))
        return;

    std::error_code ec;
    fs::recursive_directory_iterator dit(enginePluginsRoot, fs::directory_options::skip_permission_denied, ec);
    fs::recursive_directory_iterator end;

    for (; dit != end; dit.increment(ec))
    {
        if (ec)
        {
            ec.clear();
            continue;
        }

        if (!dit->is_directory(ec))
            continue;

        const fs::path dirPath = dit->path();

        if (ShouldSkipPath(dirPath))
        {
            dit.disable_recursion_pending();
            continue;
        }

        const std::string dirName = dirPath.filename().string();

        // 识别：
        // .../Source/<ModuleName>/Public
        // .../Source/<ModuleName>/Classes
        if (dirName == "Public" || dirName == "Classes")
        {
            const fs::path moduleRoot = dirPath.parent_path();    // .../Source/<ModuleName>
            const fs::path sourceRoot = moduleRoot.parent_path(); // .../Source

            if (sourceRoot.filename() == "Source")
            {
                const std::string moduleName = moduleRoot.filename().string();
                ScanPublicDir(moduleName, dirPath, true);

                // 当前目录已经交给 ScanPublicDir 递归处理，这里不必再继续深入
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
