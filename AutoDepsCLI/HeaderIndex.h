#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

// Header 属主信息（最小够用）
struct HeaderOwner
{
    std::string ModuleName; // 模块名（如 Core / EnhancedInput / CommonUI）
    bool bIsEngine = false; // 是否来自引擎
};

// Header 索引：Key = 相对 Public 路径（generic_string, 使用 / 分隔）
// 例如： "Misc/Config.h"  -> Core
class HeaderIndex
{
public:
    // 清空全部索引与统计
    void Reset();

    // 构建项目/项目插件索引（只扫每个模块的 ModuleRoot/Public）
    void BuildProject(const std::vector<fs::path>& moduleRoots,
                      const std::vector<std::string>& moduleNames);

    // 构建引擎索引
    // engineRoot 应该是 UE 根目录（包含 Engine 文件夹的那一层）
    // 例如：D:\Program Files\UE_5.6
    void BuildEngine(const fs::path& engineRoot,
                     bool bIncludeDeveloper = false,
                     bool bIncludeEditor = false);

    // 追加构建：给外部直接加一个 Public 目录（用于你后面扩展）
    void AddModulePublicDir(const std::string& moduleName,
                            const fs::path& publicDir,
                            bool bIsEngine);

    // 查询
    const std::unordered_map<std::string, std::vector<HeaderOwner>>& GetIndex() const { return Index; }

    // 统计
    size_t GetProjectHeaderFileCount() const { return ProjectHeaderFiles; }
    size_t GetEngineHeaderFileCount() const { return EngineHeaderFiles; }
    size_t GetHeaderKeyCount() const { return Index.size(); }

    // key 冲突数量：Index[key].size() > 1 的 key 计数
    size_t GetConflictKeyCount() const;

private:
    // 扫一个 PublicDir，key 用 “相对 PublicDir 的路径”
    void ScanPublicDir(const std::string& moduleName,
                       const fs::path& publicDir,
                       bool bIsEngine);

    // Engine 扫描：Engine/Source/(Runtime|Developer|Editor)/<Module>/Public
    void ScanEngineSourceGroup(const fs::path& engineSourceRoot,
                               const std::string& groupName,
                               bool bEnabled);

    // Engine 插件扫描：Engine/Plugins/**/Source/<Module>/Public
    void ScanEnginePlugins(const fs::path& enginePluginsRoot);

    // 基础过滤：跳过噪声目录
    static bool ShouldSkipPath(const fs::path& p);

private:
    std::unordered_map<std::string, std::vector<HeaderOwner>> Index;

    size_t ProjectHeaderFiles = 0; // 扫到的头文件数量（文件数量，不是 key 数）
    size_t EngineHeaderFiles = 0;
};