#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <cstdlib>
#include <unordered_map>
#include "BuildCsReader.h"
#include "BuildCsWriter.h"
#include "DependencyAnalyzer.h"
#include "DependencyRules.h"
#include "ModuleDiscovery.h"
#include "HeaderIndex.h"
#include "IncludeScanner.h"
#include "UnusedDependencyAnalyzer.h"

namespace fs = std::filesystem;

// 判断是不是 flag（以 - 开头）
static bool IsFlag(const std::string& s)
{
    return !s.empty() && s[0] == '-';
}

int main(int argc, char** argv)
{
    std::string command = "list";
    fs::path engineRoot;

    // =============================
    // 解析命令：第一个非 flag 参数
    // flags: --engine <path>
    // =============================
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (!IsFlag(arg) && (arg == "list" || arg == "index" || arg == "scan" || arg == "report" || arg == "fix"))
        {
            command = arg;
        }
        else if (arg == "--engine" && i + 1 < argc)
        {
            engineRoot = argv[++i];
        }
    }
    // ---------------------------------
    // engineRoot: 优先命令行，其次环境变量 UE_ENGINE_ROOT
    // ---------------------------------
    if (engineRoot.empty())
    {
        char* buffer = nullptr;
        size_t len = 0;

        if (_dupenv_s(&buffer, &len, "UE_ENGINE_ROOT") == 0 && buffer != nullptr)
        {
            engineRoot = fs::path(buffer);
            free(buffer); // 必须释放
        }
    }
    // =============================
    // 查找 .uproject（当前目录）
    // =============================
    fs::path cwd = fs::current_path();
    fs::path uproject;

    for (const auto& entry : fs::directory_iterator(cwd))
    {
        if (entry.path().extension() == ".uproject")
        {
            uproject = entry.path();
            break;
        }
    }

    if (uproject.empty())
    {
        std::cout << "No .uproject found in current directory.\n";
        std::cout << "Run this tool inside your UE project root (where .uproject is).\n";
        return 1;
    }

    fs::path projectRoot = uproject.parent_path();
    auto modules = DiscoverProjectModules(projectRoot);

    // =============================
    // list
    // =============================
    if (command == "list")
    {
        std::cout << "Total Modules: " << modules.size() << "\n\n";
        for (const auto& m : modules)
        {
            std::cout << "- " << m.Name;
            if (m.bIsPluginModule)
                std::cout << " (Plugin: " << m.PluginName << ")";
            std::cout << "\n";
        }
        return 0;
    }

    // =============================
    // index
    // =============================
    if (command == "index")
    {
        std::cout << "Building header index...\n\n";

        HeaderIndex index;
        index.Reset();

        // 项目索引
        std::vector<fs::path> moduleRoots;
        std::vector<std::string> moduleNames;
        moduleRoots.reserve(modules.size());
        moduleNames.reserve(modules.size());

        for (const auto& m : modules)
        {
            moduleRoots.push_back(m.ModuleRoot);
            moduleNames.push_back(m.Name);
        }

        index.BuildProject(moduleRoots, moduleNames);

        // 引擎索引（可选）
        if (!engineRoot.empty())
        {
            std::cout << "EngineRoot: " << engineRoot.string() << "\n";
            index.BuildEngine(engineRoot, /*bIncludeDeveloper*/ true, /*bIncludeEditor*/ true);
        }
        else
        {
            std::cout << "Engine index skipped (use --engine <UE_ROOT>)\n";
        }

        std::cout << "\n";
        std::cout << "Project Header Files: " << index.GetProjectHeaderFileCount() << "\n";
        std::cout << "Engine  Header Files: " << index.GetEngineHeaderFileCount() << "\n";
        std::cout << "Indexed Header Keys : " << index.GetHeaderKeyCount() << "\n";
        std::cout << "Conflict Keys       : " << index.GetConflictKeyCount() << "\n\n";
        std::cout << "Index build complete.\n";
        return 0;
    }

    // =============================
    // scan
    // =============================
    if (command == "scan")
    {
        std::cout << "Scanning includes (project only)...\n\n";

        IncludeScanner scanner;
        scanner.Scan(modules);

        const auto& results = scanner.GetResults();
        size_t total = 0;

        for (const auto& kv : results)
        {
            std::cout << "Module: " << kv.first << "\n";
            std::cout << "  Include Records: " << kv.second.size() << "\n\n";
            total += kv.second.size();
        }

        std::cout << "Scanned Modules: " << results.size() << "\n";
        std::cout << "Total Include Records: " << total << "\n";
        return 0;
    }
    
// report
// =============================
if (command == "report")
{
    std::cout << "Building header index...\n";

    HeaderIndex index;
    index.Reset();

    {
        std::vector<fs::path> moduleRoots;
        std::vector<std::string> moduleNames;
        moduleRoots.reserve(modules.size());
        moduleNames.reserve(modules.size());

        for (const auto& m : modules)
        {
            moduleRoots.push_back(m.ModuleRoot);
            moduleNames.push_back(m.Name);
        }
        index.BuildProject(moduleRoots, moduleNames);
    }

    if (engineRoot.empty())
    {
        std::cout << "ERROR: report requires --engine <UE_ROOT>\n";
        return 1;
    }
    index.BuildEngine(engineRoot, /*bIncludeDeveloper*/ true, /*bIncludeEditor*/ true);

    std::cout << "Scanning includes...\n";
    IncludeScanner scanner;
    scanner.Scan(modules);

    std::cout << "Analyzing...\n";
    DependencyAnalyzer analyzer;
    auto rawIssues = analyzer.Analyze(index, scanner.GetResults());
    auto issues = rawIssues;

    std::unordered_map<std::string, fs::path> buildCsMap;
    buildCsMap.reserve(modules.size());

    for (const auto& m : modules)
    {
        buildCsMap[m.Name] = m.BuildCsPath;
    }

    BuildCsReader buildReader;
    auto depsMap = buildReader.ReadAll(buildCsMap);

    UnusedDependencyAnalyzer unusedAnalyzer;
    auto unusedReport = unusedAnalyzer.Analyze(rawIssues, depsMap);

    std::vector<DependencyIssue> filtered;
    filtered.reserve(issues.size());

    for (const auto& i : issues)
    {
        if (i.Kind != "Missing")
        {
            filtered.push_back(i);
            continue;
        }

        if (i.ModuleName == i.TargetModule)
        {
            continue;
        }

        auto it = depsMap.find(i.ModuleName);
        if (it != depsMap.end())
        {
            if (it->second.Has(i.TargetModule))
            {
                continue;
            }
        }

        filtered.push_back(i);
    }

    issues = std::move(filtered);

    size_t missing = 0, amb = 0, unr = 0;
    for (const auto& i : issues)
    {
        if (i.Kind == "Missing") missing++;
        else if (i.Kind == "Ambiguous") amb++;
        else if (i.Kind == "Unresolved") unr++;
    }

    size_t unused = unusedReport.Issues.size();

    std::cout << "\nSummary:\n";
    std::cout << "  Missing   : " << missing << "\n";
    std::cout << "  Ambiguous : " << amb << "\n";
    std::cout << "  Unresolved: " << unr << "\n";
    std::cout << "  Unused    : " << unused << "\n\n";

    for (const auto& i : issues)
    {
        std::cout << "[" << i.Kind << "] " << i.ModuleName << "\n";
        std::cout << "  File   : " << i.SourceFile << "\n";
        std::cout << "  Include: " << i.HeaderKey << "\n";

        if (i.Kind == "Missing")
        {
            std::cout << "  Owner  : " << i.TargetModule << "\n";

            std::string vis = DependencyRules::SuggestVisibility(i.SourceFile);
            std::cout << "  Suggest: " << vis << "DependencyModuleNames.Add(\"" << i.TargetModule << "\")\n";
        }
        else if (i.Kind == "Ambiguous")
        {
            std::cout << "  Owners : ";
            for (size_t k = 0; k < i.Candidates.size(); ++k)
            {
                if (k) std::cout << ", ";
                std::cout << i.Candidates[k];
            }
            std::cout << "\n";
        }

        std::cout << "\n";
    }

    if (!unusedReport.Issues.empty())
    {
        std::cout << "Possibly Unused Dependencies:\n\n";

        for (const auto& u : unusedReport.Issues)
        {
            std::cout << "[Unused] " << u.ModuleName << "\n";
            std::cout << "  Visibility: " << u.Visibility << "\n";
            std::cout << "  Module    : " << u.TargetModule << "\n\n";
        }

        std::cout << "Note: these dependencies have no direct include evidence in scanned source files.\n";
        std::cout << "They may still be required by reflection, build configuration, or indirect usage.\n\n";
    }

    return 0;
}

    
    if (command == "fix")
{
    // fix = report + apply edits to Build.cs
    if (engineRoot.empty())
    {
        std::cout << "ERROR: fix requires --engine <UE_ROOT>\n";
        return 1;
    }

    std::cout << "Building header index...\n";
    HeaderIndex index;
    index.Reset();

    // build project index
    std::vector<fs::path> moduleRoots;
    std::vector<std::string> moduleNames;
    moduleRoots.reserve(modules.size());
    moduleNames.reserve(modules.size());
    for (const auto& m : modules)
    {
        moduleRoots.push_back(m.ModuleRoot);
        moduleNames.push_back(m.Name);
    }
    index.BuildProject(moduleRoots, moduleNames);

    // build engine index
    index.BuildEngine(engineRoot, true, true);

    std::cout << "Scanning includes...\n";
    IncludeScanner scanner;
    scanner.Scan(modules);

    std::cout << "Analyzing...\n";
    DependencyAnalyzer analyzer;
    auto issues = analyzer.Analyze(index, scanner.GetResults());

    // 读取每个模块 Build.cs 当前依赖（用于差集过滤）
    // 你 main 里已经有 buildCsMap 的构建方式：key=ModuleName, value=Build.cs path
    std::unordered_map<std::string, fs::path> buildCsMap;
    buildCsMap.reserve(modules.size());
    for (const auto& m : modules)
    {
        // 需要 ModuleInfo 里有 BuildCsPath（你之前就是这么做的）
        if (!m.BuildCsPath.empty())
            buildCsMap[m.Name] = m.BuildCsPath;
    }

    BuildCsReader buildReader;
    auto existingDepsMap = buildReader.ReadAll(buildCsMap);

    // 聚合 edits：按 moduleName -> edits
    std::unordered_map<std::string, BuildCsEdits> editsByModule;

    size_t addCount = 0;
    size_t skipped = 0;

    for (const auto& i : issues)
    {
        // v1：只处理 Missing，Unresolved / Ambiguous 不自动修
        if (i.Kind != "Missing")
            continue;

        // 过滤自依赖（你刚加的规则，也可以双保险）
        if (i.ModuleName == i.TargetModule)
            continue;

        // 必须能定位到 Build.cs
        auto itCs = buildCsMap.find(i.ModuleName);
        if (itCs == buildCsMap.end())
        {
            skipped++;
            continue;
        }

        // 必须能确定 owner（TargetModule）
        if (i.TargetModule.empty())
        {
            skipped++;
            continue;
        }

        // 差集过滤：如果 Build.cs 里已经有依赖，就不加
        auto itExisting = existingDepsMap.find(i.ModuleName);
        if (itExisting != existingDepsMap.end())
        {
            const auto& deps = itExisting->second;

            // v1：不区分 public/private 里是否存在，只要任意存在就算已依赖
            if (deps.PublicDeps.count(i.TargetModule) || deps.PrivateDeps.count(i.TargetModule))
                continue;
        }

        // 根据源文件路径判断加到 Public 还是 Private（你写的规则）
        std::string vis = DependencyRules::SuggestVisibility(i.SourceFile);
        auto& edits = editsByModule[i.ModuleName];
        if (vis == "Public")
            edits.AddPublic.insert(i.TargetModule);
        else
            edits.AddPrivate.insert(i.TargetModule);
    }

    // 写回
    size_t changedFiles = 0;
    for (auto& kv : editsByModule)
    {
        const std::string& moduleName = kv.first;
        const BuildCsEdits& edits = kv.second;

        auto itCs = buildCsMap.find(moduleName);
        if (itCs == buildCsMap.end())
            continue;

        if (edits.AddPublic.empty() && edits.AddPrivate.empty())
            continue;

        bool changed = BuildCsWriter::ApplyEdits(itCs->second, edits);
        if (changed)
        {
            changedFiles++;
            addCount += edits.AddPublic.size();
            addCount += edits.AddPrivate.size();
        }
    }

    std::cout << "\nFix Summary:\n";
    std::cout << "  Modules touched : " << changedFiles << "\n";
    std::cout << "  Deps added      : " << addCount << "\n";
    std::cout << "  Skipped         : " << skipped << "\n";
    std::cout << "  Backup          : *.Build.cs.bak\n\n";

    std::cout << "Done.\n";
    return 0;
}
    // =============================
    // usage
    // =============================
    std::cout << "Usage:\n";
    std::cout << "  AutoDepsCLI.exe list\n";
    std::cout << "  AutoDepsCLI.exe index [--engine <UE_ROOT>]\n";
    std::cout << "  AutoDepsCLI.exe scan\n";
    return 0;
}