#include "UnusedDependencyAnalyzer.h"

bool UnusedDependencyAnalyzer::IsProtectedModule(const std::string& moduleName)
{
    static const std::unordered_set<std::string> Protected =
    {
        "Core",
        "CoreUObject",
        "Engine"
    };

    return Protected.find(moduleName) != Protected.end();
}

std::unordered_set<std::string> UnusedDependencyAnalyzer::CollectUsedModules(
    const std::string& moduleName,
    const std::vector<DependencyIssue>& rawIssues)
{
    std::unordered_set<std::string> used;

    for (const auto& issue : rawIssues)
    {
        if (issue.ModuleName != moduleName)
            continue;

        // 中文注释：
        // 当前 DependencyAnalyzer 的语义是：
        // 只要 include 能唯一解析到 owner，就先标成 Missing。
        // 后续 main.cpp 才会结合 Build.cs 过滤掉“已经声明过”的 Missing。
        // 所以这里要用 rawIssues，并把 Missing 视为“有直接使用证据”。
        if (issue.Kind != "Missing")
            continue;

        if (issue.TargetModule.empty())
            continue;

        if (issue.TargetModule == moduleName)
            continue;

        used.insert(issue.TargetModule);
    }

    return used;
}

UnusedDependencyReport UnusedDependencyAnalyzer::Analyze(
    const std::vector<DependencyIssue>& rawIssues,
    const std::unordered_map<std::string, ModuleDeps>& declaredDepsMap) const
{
    UnusedDependencyReport report;

    for (const auto& kv : declaredDepsMap)
    {
        const std::string& moduleName = kv.first;
        const ModuleDeps& deps = kv.second;

        const std::unordered_set<std::string> usedModules =
            CollectUsedModules(moduleName, rawIssues);

        for (const auto& dep : deps.PublicDeps)
        {
            if (dep == moduleName)
                continue;

            if (IsProtectedModule(dep))
                continue;

            if (usedModules.find(dep) == usedModules.end())
            {
                UnusedDependencyIssue issue;
                issue.ModuleName = moduleName;
                issue.TargetModule = dep;
                issue.Visibility = "Public";
                report.Issues.push_back(std::move(issue));
            }
        }

        for (const auto& dep : deps.PrivateDeps)
        {
            if (dep == moduleName)
                continue;

            if (IsProtectedModule(dep))
                continue;

            if (usedModules.find(dep) == usedModules.end())
            {
                UnusedDependencyIssue issue;
                issue.ModuleName = moduleName;
                issue.TargetModule = dep;
                issue.Visibility = "Private";
                report.Issues.push_back(std::move(issue));
            }
        }
    }

    return report;
}
