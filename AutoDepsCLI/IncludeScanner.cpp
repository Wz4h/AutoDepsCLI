#include "IncludeScanner.h"
#include <fstream>
#include <regex>
#include <iostream>

bool IncludeScanner::IsSourceFile(const fs::path& p)
{
    const std::string ext = p.extension().string();
    return ext == ".cpp" ||
           ext == ".cc"  ||
           ext == ".cxx" ||
           ext == ".h"   ||
           ext == ".hpp" ||
           ext == ".inl";
}

// 统一 include key 规则：
// - 使用 /
// - 去掉 ./
// - 过滤 generated
std::string IncludeScanner::NormalizeIncludeKey(const std::string& raw)
{
    std::string s = raw;

    // 统一路径分隔符
    for (char& c : s)
    {
        if (c == '\\')
            c = '/';
    }

    // 去掉 ./ 开头
    if (s.rfind("./", 0) == 0)
        s = s.substr(2);

    // 去掉尾部空白
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
        s.pop_back();

    // 跳过 generated
    if (s.find(".generated.h") != std::string::npos)
        return "";

    return s;
}

void IncludeScanner::Scan(const std::vector<ModuleInfo>& modules)
{
    ModuleIncludes.clear();
    TotalIncludes = 0;

    // 只匹配 #include "xxx"
    std::regex pattern("^\\s*#\\s*include\\s*\"([^\"]+)\"");

    for (const auto& module : modules)
    {
        std::vector<fs::path> roots;

        fs::path publicDir  = module.ModuleRoot / "Public";
        fs::path privateDir = module.ModuleRoot / "Private";

        if (fs::exists(publicDir))
            roots.push_back(publicDir);

        if (fs::exists(privateDir))
            roots.push_back(privateDir);

        if (roots.empty())
            continue;

        auto& output = ModuleIncludes[module.Name];

        for (const auto& root : roots)
        {
            for (const auto& entry : fs::recursive_directory_iterator(root))
            {
                if (!entry.is_regular_file())
                    continue;

                if (!IsSourceFile(entry.path()))
                    continue;

                std::ifstream in(entry.path());
                if (!in.is_open())
                    continue;

                std::string line;
                while (std::getline(in, line))
                {
                    std::smatch match;
                    if (!std::regex_search(line, match, pattern))
                        continue;

                    std::string includePath = match[1].str();
                    std::string key = NormalizeIncludeKey(includePath);

                    if (key.empty())
                        continue;

                    IncludeRecord rec;
                    rec.HeaderKey  = key;
                    rec.SourceFile = entry.path().generic_string();

                    output.push_back(std::move(rec));
                    TotalIncludes++;
                }
            }
        }
    }
}