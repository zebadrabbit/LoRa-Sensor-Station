# Code Quality Tools Setup

This project uses automated code formatting and static analysis.

## Tools Configured

### 🎨 Formatter: clang-format

- **Config**: `.clang-format` (Google style, 100-char lines)
- **Auto-format on save**: Enabled in VS Code
- **Manual format**: Right-click → Format Document (or Shift+Alt+F)

### 🔍 Linter: cppcheck + clang-tidy

- **Config**: `.clang-tidy` + `platformio.ini`
- **Checks**: Bug detection, performance, readability
- **Run manually**: `pio check` (from terminal)

## Usage

### Automatic (Recommended)

- Just save files - formatting happens automatically
- VS Code will show warnings/errors inline

### Manual Formatting

```bash
# Format all C/C++ files
clang-format -i src/**/*.cpp include/**/*.h

# Format specific file
clang-format -i src/main.cpp
```

### Static Analysis

```bash
# Run full check on base_station
pio check -e base_station

# Run full check on sensor
pio check -e sensor

# Check specific file
pio check -e base_station --src-filters="+<src/main.cpp>"
```

## VS Code Extensions (Recommended)

1. **C/C++** (Microsoft) - IntelliSense, formatting
2. **Clang-Format** - Formatter integration
3. **PlatformIO IDE** - Build, upload, check

## Configuration Files

- `.clang-format` - Formatting rules
- `.clang-tidy` - Linting rules
- `.vscode/settings.json` - Editor integration
- `platformio.ini` - Check tool configuration

## Suppressing Warnings

```cpp
// Disable specific warning for next line
// NOLINTNEXTLINE(bugprone-narrowing-conversions)
int x = 3.14;

// Disable for block
// NOLINTBEGIN(readability-magic-numbers)
setLED(0xFF0000);  // Red color
// NOLINTEND(readability-magic-numbers)
```

## Tips

- Format before committing (auto on save)
- Run `pio check` periodically to catch issues
- Magic numbers disabled (embedded systems have many)
- Arduino String type allowed (performance check exemption)
