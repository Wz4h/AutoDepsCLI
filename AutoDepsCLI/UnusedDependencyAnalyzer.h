#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "DependencyAnalyzer.h"
#include "BuildCsReader.h"

struct UnusedDependencyIssue
{
    std::string ModuleName;
    std::string TargetModule;
    std::string Visibility; // "Public" / "Private"
};

struct UnusedDependencyReport
{
    std::vector<UnusedDependencyIssue> Issues;
};

class UnusedDependencyAnalyzer
{
public:
    UnusedDependencyReport Analyze(
        const std::vector<DependencyIssue>& rawIssues,
        const std::unordered_map<std::string, ModuleDeps>& declaredDepsMap) const;

private:
    static bool IsProtectedModule(const std::string& moduleName);

    static std::unordered_set<std::string> CollectUsedModules(
        const std::string& moduleName,
        const std::vector<DependencyIssue>& rawIssues);
};
