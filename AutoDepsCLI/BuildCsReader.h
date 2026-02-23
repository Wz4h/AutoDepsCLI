

#pragma once
#include <string>

#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>

namespace fs = std::filesystem;

struct ModuleDeps
{
    std::unordered_set<std::string> PublicDeps;
    std::unordered_set<std::string> PrivateDeps;

    bool Has(const std::string& module) const
    {
        return PublicDeps.count(module) || PrivateDeps.count(module);
    }
};

class BuildCsReader
{
public:
    // 输入：Build.cs 文件路径
    // 输出：解析到的 Public/Private 依赖集合（只支持标准写法/字符串字面量）
    ModuleDeps ReadModuleBuildCs(const fs::path& buildCsPath);

    // 批量读取：key = ModuleName
    std::unordered_map<std::string, ModuleDeps> ReadAll(
        const std::unordered_map<std::string, fs::path>& moduleBuildCsMap);

private:
    static std::string ReadTextFile(const fs::path& p);
    static void ParseStringLiteralsFromRangeAdd(
        const std::string& text,
        const std::string& listName,
        std::unordered_set<std::string>& out);

    static void ParseStringLiteralsFromAdd(
        const std::string& text,
        const std::string& listName,
        std::unordered_set<std::string>& out);
};