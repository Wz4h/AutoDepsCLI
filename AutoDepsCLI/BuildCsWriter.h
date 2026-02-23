#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>

namespace fs = std::filesystem;

// 对单个模块 Build.cs 的变更（按 Public / Private 分开）
struct BuildCsEdits
{
    std::unordered_set<std::string> AddPublic;
    std::unordered_set<std::string> AddPrivate;
};

// Build.cs 写回器（v1：够用版本）
// - 自动备份 .bak
// - 尝试 AddRange 追加；找不到则插入一段 AddRange
class BuildCsWriter
{
public:
    // 返回 true 表示文件发生了修改
    static bool ApplyEdits(const fs::path& buildCsPath, const BuildCsEdits& edits);
    bool MergeAddRange(std::string& fileText, const std::string& listName,
                          const std::unordered_set<std::string>& newModules);
private:
   
    static std::string ReadAllText(const fs::path& p);
    static bool WriteAllText(const fs::path& p, const std::string& text);
    static bool BackupFile(const fs::path& p);

    // 在 Public/Private DependencyModuleNames 中追加模块名（去重）
    // listName = "PublicDependencyModuleNames" or "PrivateDependencyModuleNames"
    static bool TryAppendToAddRange(std::string& text, const std::string& listName,
                                   const std::vector<std::string>& toAdd);

    // 如果没有 AddRange 语句，则插入一段 AddRange（构造函数开头）
    static bool InsertAddRangeBlock(std::string& text, const std::string& listName,
                                   const std::vector<std::string>& toAdd);

    // 小工具
    static std::vector<std::string> ToSortedVector(const std::unordered_set<std::string>& s);
    static std::string JoinQuoted(const std::vector<std::string>& items);
};