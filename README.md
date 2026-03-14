# AutoDepsCLI

**Unreal Engine C++ Module Dependency Analyzer**  
（虚幻引擎 C++ 模块依赖关系分析工具）

AutoDepsCLI is a command-line tool for analyzing C++ module dependencies in Unreal Engine projects.

It scans project source files, resolves `#include` ownership, detects missing module dependencies, and reports potentially unused dependencies in `.Build.cs`.

The tool is designed to help developers maintain clean module dependencies in large Unreal Engine codebases.

---

# Features

- Scan Unreal Engine project and plugin modules
- Resolve `#include` → module ownership
- Detect **missing module dependencies**
- Detect **possibly unused dependencies**
- Automatically **fix missing dependencies**
- Support **project modules and plugin modules**
- Suitable for **CI pipelines and engineering workflows**

---

# Installation

Download the latest release from the **Releases** page.

Extract:

```
AutoDepsCLI.exe
```

Then either:

• Run it directly in your Unreal project directory  
• Or add it to your system `PATH`

---

# Quick Start

Run inside your Unreal project root (where `.uproject` is located).

## Option 1 — Specify engine path

```bash
AutoDepsCLI report --engine "D:\Program Files\UE_5.6"
```

## Option 2 — Use environment variable

Set environment variable:

```
UE_ENGINE_ROOT=D:\Program Files\UE_5.6
```

Then run:

```bash
AutoDepsCLI report
```

Command-line parameter `--engine` has higher priority than the environment variable.

---

# Commands

## list

List all detected modules in the project.

```bash
AutoDepsCLI list
```

Example output:

```
Total Modules: 5

- ProjectModule
- Test_0218
- ModuleBuilderEditor (Plugin: ModuleBuilder)
- PluginModule (Plugin: TestPlugin)
- TestPlugin (Plugin: TestPlugin)
```

---

## index

Build a header index used for dependency analysis.

```bash
AutoDepsCLI index --engine "D:\Program Files\UE_5.6"
```

This step scans engine and project headers to build a module ownership index.

---

## report

Analyze module dependencies and generate a report.

```bash
AutoDepsCLI report --engine "D:\Program Files\UE_5.6"
```

The report includes:

• Missing dependencies  
• Ambiguous header ownership  
• Unresolved includes  
• Possibly unused module dependencies  

Example output:

```
Summary:
  Missing   : 2
  Ambiguous : 1
  Unresolved: 0
  Unused    : 1

[Missing] MyModule
  File   : Source/MyModule/Private/Test.cpp
  Include: GameplayTagsManager.h
  Owner  : GameplayTags
  Suggest: PrivateDependencyModuleNames.Add("GameplayTags")

[Unused] MyModule
  Visibility: Private
  Module    : Niagara
```

Note:

Unused dependencies are detected based on include usage.  
Some dependencies may still be required by reflection, build configuration, or indirect usage.

---

## fix

Automatically add missing dependencies to `.Build.cs`.

```bash
AutoDepsCLI fix --engine "D:\Program Files\UE_5.6"
```

This command will:

- Detect missing module dependencies
- Update the corresponding `.Build.cs`
- Create a backup file:

```
.Build.cs.bak
```

---

# Parameters

| Parameter | Description |
|-----------|-------------|
| `--engine <path>` | Unreal Engine root directory |

If not specified, the tool will attempt to read the environment variable:

```
UE_ENGINE_ROOT
```

---

# Use Cases

AutoDepsCLI is useful for:

• Maintaining large Unreal Engine C++ projects  
• Detecting incorrect module dependencies  
• Cleaning dependency structure  
• Preparing CI validation checks  
• Understanding header → module ownership  

---

# Version

Current version:

```
v0.3
```

---

# License

For demonstration and educational purposes.
