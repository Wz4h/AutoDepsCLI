#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include <filesystem>
#include "ModuleDiscovery.h"

namespace fs = std::filesystem;

// 一条 include 记录
struct IncludeRecord
{
    // 与 HeaderIndex 的 key 完全一致：
    // 例如：Misc/Config.h
    std::string HeaderKey;

    // 可选：来源文件（用于未来生成报告）
    std::string SourceFile;
};

class IncludeScanner
{
public:
    // 只扫描项目模块（Public + Private）
    void Scan(const std::vector<ModuleInfo>& modules);

    const std::unordered_map<std::string, std::vector<IncludeRecord>>& GetResults() const
    {
        return ModuleIncludes;
    }

    size_t GetTotalIncludeCount() const
    {
        return TotalIncludes;
    }

private:
    static bool IsSourceFile(const fs::path& p);
    static std::string NormalizeIncludeKey(const std::string& raw);

private:
    std::unordered_map<std::string, std::vector<IncludeRecord>> ModuleIncludes;
    size_t TotalIncludes = 0;
};