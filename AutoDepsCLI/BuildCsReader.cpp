
#include "BuildCsReader.h"
#include <fstream>
#include <regex>


static std::string StripCsComments(const std::string& s)
{
    std::string out;
    out.reserve(s.size());

    bool inLine = false;
    bool inBlock = false;

    for (size_t i = 0; i < s.size(); ++i)
    {
        char c = s[i];
        char n = (i + 1 < s.size()) ? s[i + 1] : '\0';

        if (!inLine && !inBlock)
        {
            if (c == '/' && n == '/')
            {
                inLine = true;
                ++i;
                continue;
            }
            if (c == '/' && n == '*')
            {
                inBlock = true;
                ++i;
                continue;
            }
            out.push_back(c);
        }
        else if (inLine)
        {
            if (c == '\n')
            {
                inLine = false;
                out.push_back('\n');
            }
        }
        else if (inBlock)
        {
            if (c == '*' && n == '/')
            {
                inBlock = false;
                ++i;
            }
        }
    }

    return out;
}

static void CollectQuotedStrings(const std::string& s, std::unordered_set<std::string>& out)
{
    // 匹配 "Core" 这种 C# 字符串字面量
    std::regex q("\"([^\"]+)\"");
    for (std::sregex_iterator it(s.begin(), s.end(), q), end; it != end; ++it)
    {
        out.insert((*it)[1].str());
    }
}

std::string BuildCsReader::ReadTextFile(const fs::path& p)
{
    std::ifstream in(p, std::ios::in);
    if (!in.is_open()) return {};
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return text;
}

void BuildCsReader::ParseStringLiteralsFromRangeAdd(
    const std::string& text,
    const std::string& listName,
    std::unordered_set<std::string>& out)
{
    // 支持：
    // PublicDependencyModuleNames.AddRange(new string[] { "Core", "Engine" });
    // PrivateDependencyModuleNames.AddRange(new[] { "Slate", "SlateCore" });
    //
    // 只解析花括号 {...} 内的 "xxx"
    std::string pattern =
        listName + R"(\s*\.\s*AddRange\s*\(\s*new\s*(?:string\s*\[\]\s*)?\s*\{([\s\S]*?)\}\s*\))";

    std::regex re(pattern);
    std::smatch m;
    std::string::const_iterator searchStart(text.cbegin());

    while (std::regex_search(searchStart, text.cend(), m, re))
    {
        CollectQuotedStrings(m[1].str(), out);
        searchStart = m.suffix().first;
    }
}

void BuildCsReader::ParseStringLiteralsFromAdd(
    const std::string& text,
    const std::string& listName,
    std::unordered_set<std::string>& out)
{
    // 支持：
    // PublicDependencyModuleNames.Add("Core");
    std::string pattern =
      listName + "\\s*\\.\\s*Add\\s*\\(\\s*\"([^\"]+)\"\\s*\\)";

    std::regex re(pattern);
    for (std::sregex_iterator it(text.begin(), text.end(), re), end; it != end; ++it)
    {
        out.insert((*it)[1].str());
    }
}

ModuleDeps BuildCsReader::ReadModuleBuildCs(const fs::path& buildCsPath)
{
    ModuleDeps deps;
    std::string text = ReadTextFile(buildCsPath);
    text = StripCsComments(text);
    if (text.empty()) return deps;

    ParseStringLiteralsFromRangeAdd(text, "PublicDependencyModuleNames", deps.PublicDeps);
    ParseStringLiteralsFromRangeAdd(text, "PrivateDependencyModuleNames", deps.PrivateDeps);

    ParseStringLiteralsFromAdd(text, "PublicDependencyModuleNames", deps.PublicDeps);
    ParseStringLiteralsFromAdd(text, "PrivateDependencyModuleNames", deps.PrivateDeps);

    return deps;
}

std::unordered_map<std::string, ModuleDeps> BuildCsReader::ReadAll(
    const std::unordered_map<std::string, fs::path>& moduleBuildCsMap)
{
    std::unordered_map<std::string, ModuleDeps> out;
    for (const auto& kv : moduleBuildCsMap)
    {
        out[kv.first] = ReadModuleBuildCs(kv.second);
    }
    return out;
}