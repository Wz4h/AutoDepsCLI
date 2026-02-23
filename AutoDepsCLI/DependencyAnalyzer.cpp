#include "DependencyAnalyzer.h"
#include <unordered_set>

std::vector<DependencyIssue> DependencyAnalyzer::Analyze(
    const HeaderIndex& headerIndex,
    const std::unordered_map<std::string, std::vector<IncludeRecord>>& moduleIncludes)
{
    std::vector<DependencyIssue> issues;
    const auto& index = headerIndex.GetIndex();

    // 去重：同一个 Module + SourceFile + HeaderKey 只报一次
    struct KeyHash
    {
        size_t operator()(const std::string& s) const { return std::hash<std::string>{}(s); }
    };
    std::unordered_set<std::string, KeyHash> dedup;

    for (const auto& mkv : moduleIncludes)
    {
        const std::string& moduleName = mkv.first;
        const auto& includes = mkv.second;

        for (const auto& rec : includes)
        {
            // 组合去重 key
            std::string dedupKey = moduleName + "|" + rec.SourceFile + "|" + rec.HeaderKey;
            if (!dedup.insert(dedupKey).second)
                continue;

            DependencyIssue issue;
            issue.ModuleName = moduleName;
            issue.SourceFile = rec.SourceFile;
            issue.HeaderKey  = rec.HeaderKey;

            auto it = index.find(rec.HeaderKey);
            if (it == index.end())
            {
                issue.Kind = "Unresolved";
                issues.push_back(std::move(issue));
                continue;
            }

            const auto& owners = it->second;

            // 多 owner：歧义
            if (owners.size() > 1)
            {
                issue.Kind = "Ambiguous";
                issue.Candidates.reserve(owners.size());
                for (const auto& o : owners)
                    issue.Candidates.push_back(o.ModuleName);

                issues.push_back(std::move(issue));
                continue;
            }

            // 唯一 owner：先标记为 Missing（下一阶段再结合 Build.cs 做“是否真的缺失”）
            issue.Kind = "Missing";
            issue.TargetModule = owners[0].ModuleName;
            issues.push_back(std::move(issue));
        }
    }

    return issues;
}