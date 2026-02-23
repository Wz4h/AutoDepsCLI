#pragma once
#include <string>

// 依赖可见性推断规则（简单版，够产品用）
// - 如果 include 发生在 /Public/ 下：建议 PublicDependencyModuleNames
// - 否则：建议 PrivateDependencyModuleNames
class DependencyRules
{
public:
    static std::string SuggestVisibility(const std::string& sourceFilePath)
    {
        std::string s = sourceFilePath;
        for (char& c : s)
        {
            if (c == '\\') c = '/';
        }

        if (s.find("/Public/") != std::string::npos)
            return "Public";

        return "Private";
    }
};