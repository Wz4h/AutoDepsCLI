#pragma once
#include <filesystem>
#include <vector>
#include <string>

// 使用标准文件系统
namespace fs = std::filesystem;

// 描述一个 UE 模块的信息
struct ModuleInfo
{
    std::string Name;          // 模块名称
    fs::path BuildCsPath;      // Build.cs 路径
    fs::path ModuleRoot;       // 模块根目录
    bool bIsPluginModule = false; // 是否为插件模块
    std::string PluginName;    // 插件名称（如果是插件模块）
};

// 扫描项目目录，发现所有模块
std::vector<ModuleInfo> DiscoverProjectModules(const fs::path& projectRoot);