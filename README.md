# AutoDepsCLI

Unreal Engine C++ Module Dependency Analyzer  
（虚幻引擎 C++ 模块依赖关系分析工具）

AutoDepsCLI is a command-line tool for scanning Unreal Engine projects,
listing C++ modules, and generating dependency analysis reports.

---

## Features

- Scan project and plugin C++ modules
- Generate module structure reports
- Provide dependency fix suggestions
- Support environment-based engine configuration
- Designed for automation and engineering workflows

---

## Installation

Download the latest release from the Releases page.

Extract `AutoDepsCLI.exe` and run directly, or add it to your system PATH.

---

## Quick Start

### Option 1 — Specify engine path explicitly

```bash
AutoDepsCLI report --engine "D:\Program Files\UE_5.6"
```

### Option 2 — Use environment variable

Set environment variable:

```
UE_ENGINE_ROOT=D:\Program Files\UE_5.6
```

Then run:

```bash
AutoDepsCLI report
```

Command-line argument `--engine` has higher priority than environment variable.

---

## Commands

### list

List detected modules.

```bash
AutoDepsCLI list --engine "D:\Program Files\UE_5.6"
```

---

### index

Build module index for dependency analysis.

```bash
AutoDepsCLI index --engine "D:\Program Files\UE_5.6"
```

---

### report

Generate module structure report.

```bash
AutoDepsCLI report --engine "D:\Program Files\UE_5.6"
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

### fix

Generate dependency fix suggestions based on analysis.

```bash
AutoDepsCLI fix --engine "D:\Program Files\UE_5.6"
```

---

## Parameters

| Parameter | Description |
|------------|-------------|
| `--engine <path>` | Unreal Engine root directory |

If not specified, the tool attempts to read:

```
UE_ENGINE_ROOT
```

---

## Use Cases

- Reviewing project module structure
- Detecting dependency issues
- Preparing CI checks
- Organizing large C++ projects

---

## Version

Current version: v0.1.0

---

## License

For demonstration and educational purposes.