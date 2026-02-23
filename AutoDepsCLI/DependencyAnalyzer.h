#pragma once
#include <string>
#include <vector>
#include <unordered_map>

#include "HeaderIndex.h"
#include "IncludeScanner.h"

// 一条问题记录（文件级）
struct DependencyIssue
{
    std::string ModuleName;     // 当前扫描模块（谁 include 的）
    std::string SourceFile;     // 哪个源文件里 include 的
    std::string HeaderKey;      // include 的 key（例如 GameFramework/Actor.h）
    std::string Kind;           // "Missing" / "Ambiguous" / "Unresolved"
    std::string TargetModule;   // Missing 时：建议依赖的模块名（唯一命中）
    std::vector<std::string> Candidates; // Ambiguous 时：候选模块列表
};

class DependencyAnalyzer
{
public:
    std::vector<DependencyIssue> Analyze(
        const HeaderIndex& headerIndex,
        const std::unordered_map<std::string, std::vector<IncludeRecord>>& moduleIncludes
    );
};

