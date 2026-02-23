#include "BuildCsWriter.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>

static std::unordered_set<std::string> ExtractQuotedStrings(const std::string& text)
{
    std::unordered_set<std::string> result;
    std::regex q(R"REGEX("([^"]+)")REGEX");
    for (std::sregex_iterator it(text.begin(), text.end(), q), end; it != end; ++it)
    {
        result.insert((*it)[1].str());
    }
    return result;
}
bool BuildCsWriter::MergeAddRange(
    std::string& fileText,
    const std::string& listName,
    const std::unordered_set<std::string>& newModules)
{
    // 匹配 AddRange(...) 整段
    std::regex pattern(
        listName + R"(\s*\.\s*AddRange\s*\(\s*(?:new\s+string\s*\[\]\s*)?\{([\s\S]*?)\}\s*\)\s*;)",
        std::regex::icase);

    std::smatch match;
    if (!std::regex_search(fileText, match, pattern))
        return false;

    std::string inside = match[1].str();
    auto existing = ExtractQuotedStrings(inside);

    bool changed = false;

    for (const auto& m : newModules)
    {
        if (existing.find(m) == existing.end())
        {
            existing.insert(m);
            changed = true;
        }
    }

    if (!changed)
        return false;

    // 重新构造数组
    std::string rebuilt = listName + ".AddRange(new string[] { ";

    bool first = true;
    for (const auto& m : existing)
    {
        if (!first) rebuilt += ", ";
        rebuilt += "\"" + m + "\"";
        first = false;
    }

    rebuilt += " });";

    // 替换整段
    fileText.replace(match.position(0), match.length(0), rebuilt);

    return true;
}

static std::string NormalizeNL(const std::string& in)
{
    // v1：不做复杂换行归一化，保持原样即可
    return in;
}

std::string BuildCsWriter::ReadAllText(const fs::path& p)
{
    std::ifstream in(p, std::ios::in | std::ios::binary);
    if (!in.is_open()) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool BuildCsWriter::WriteAllText(const fs::path& p, const std::string& text)
{
    std::ofstream out(p, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;
    out.write(text.data(), (std::streamsize)text.size());
    return true;
}

bool BuildCsWriter::BackupFile(const fs::path& p)
{
    fs::path bak = p;
    bak += ".bak";
    std::error_code ec;
    fs::copy_file(p, bak, fs::copy_options::overwrite_existing, ec);
    return !ec;
}

std::vector<std::string> BuildCsWriter::ToSortedVector(const std::unordered_set<std::string>& s)
{
    std::vector<std::string> v(s.begin(), s.end());
    std::sort(v.begin(), v.end());
    return v;
}

std::string BuildCsWriter::JoinQuoted(const std::vector<std::string>& items)
{
    // 输出："A", "B", "C"
    std::string out;
    for (size_t i = 0; i < items.size(); ++i)
    {
        if (i) out += ", ";
        out += "\"";
        out += items[i];
        out += "\"";
    }
    return out;
}

bool BuildCsWriter::TryAppendToAddRange(std::string& text, const std::string& listName,
                                       const std::vector<std::string>& toAdd)
{
    if (toAdd.empty()) return false;

    // 匹配：
    // PublicDependencyModuleNames.AddRange(new string[] { ... });
    // PrivateDependencyModuleNames.AddRange(new string[] { ... });
    //
    // 说明：v1 用 regex 找到 AddRange 的 {...}，然后在 } 前插入缺失项（避免复杂 AST）
    std::string pattern =
        listName +
        R"(\s*\.\s*AddRange\s*\(\s*new\s+string\s*\[\s*\]\s*\{\s*([\s\S]*?)\s*\}\s*\)\s*;)";

    std::regex re(pattern);
    std::smatch m;
    if (!std::regex_search(text, m, re))
        return false;

    std::string inside = m[1].str();

    // 粗略去重：如果里面已经有 "X" 就不加
    // v1：直接查找 "\"X\"" 子串
    std::vector<std::string> actuallyAdd;
    actuallyAdd.reserve(toAdd.size());

    for (const auto& mod : toAdd)
    {
        std::string needle = "\"" + mod + "\"";
        if (inside.find(needle) == std::string::npos)
            actuallyAdd.push_back(mod);
    }

    if (actuallyAdd.empty())
        return false;

    // 追加格式：如果原本 inside 非空，前面补一个逗号
    std::string appended = inside;
    if (!appended.empty())
    {
        // 保守：如果最后不是逗号/换行，补逗号+空格
        // （inside 可能有换行/空格，v1 不做精确格式化）
        appended += ", ";
    }
    appended += JoinQuoted(actuallyAdd);

    // 用替换的方式把分组内容替换掉
    // 注意：m.position(1) 是捕获组起始位置
    size_t start = (size_t)m.position(1);
    size_t len = (size_t)m.length(1);
    text.replace(start, len, appended);
    return true;
}

bool BuildCsWriter::InsertAddRangeBlock(std::string& text, const std::string& listName,
                                       const std::vector<std::string>& toAdd)
{
    if (toAdd.empty()) return false;

    // 找 class Xxx : ModuleRules 的构造函数块开头
    // v1：找到第一处 "{", 在其后插入一段
    // 这是最保守的“能用就行”策略
    size_t brace = text.find('{');
    if (brace == std::string::npos) return false;

    std::string block;
    block += "\n        // AutoDepsCLI: auto-added dependencies\n        ";
    block += listName;
    block += ".AddRange(new string[] { ";
    block += JoinQuoted(toAdd);
    block += " });\n";

    text.insert(brace + 1, block);
    return true;
}

bool BuildCsWriter::ApplyEdits(const fs::path& buildCsPath, const BuildCsEdits& edits)
{
    if (!fs::exists(buildCsPath))
        return false;

    auto pub = ToSortedVector(edits.AddPublic);
    auto pri = ToSortedVector(edits.AddPrivate);

    if (pub.empty() && pri.empty())
        return false;

    std::string text = ReadAllText(buildCsPath);
    if (text.empty())
        return false;

    // 用于“是否真的发生文本变化”的更可靠判定
    const std::string before = text;

    bool changed = false;

    // 一个小封装：对某个 listName 做“先合并已有，再插入块”
    auto ApplyOneList = [&](const char* listName, const std::vector<std::string>& items)
    {
        if (items.empty())
            return;

        // 1) 优先：追加到已有 AddRange(...) 段（你已有的实现）
        if (TryAppendToAddRange(text, listName, items))
        {
            return;
        }

        // 2) 否则：插入一个新的 AddRange 块（你已有的实现）
        // InsertAddRangeBlock 返回 true 才表示插入成功
        InsertAddRangeBlock(text, listName, items);
    };

    // 处理 Public / Private
    ApplyOneList("PublicDependencyModuleNames", pub);
    ApplyOneList("PrivateDependencyModuleNames", pri);

    // 这里用“文本是否变化”来判定 changed，避免“只加了注释/插入失败但你仍然当成功”
    changed = (text != before);

    if (!changed)
        return false;

    // 备份 + 写回
    if (!BackupFile(buildCsPath))
        return false;

    return WriteAllText(buildCsPath, NormalizeNL(text));
}