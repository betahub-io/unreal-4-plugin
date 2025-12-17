# BetaHub Bug Reporter

In-game bug reporting and feedback collection plugin for Unreal Engine 4.27. Integrates with [BetaHub](https://betahub.io) to streamline QA during playtesting.

## Features

- **Bug Reports** - Submit issues with automatic screenshot and log capture
- **Suggestions** - Collect feature requests and feedback from players
- **Keyboard Shortcut** - Press F12 (configurable) to open the report form
- **Zero Setup UI** - Pure Slate implementation, no Blueprint assets required

## Setup

1. Copy plugin to your project's `Plugins/` folder
2. Open **Project Settings > Plugins > BetaHub Bug Reporter**
3. Enter your **Project ID** and **Project Token** from [BetaHub](https://app.betahub.io)

## Configuration

| Setting | Description |
|---------|-------------|
| `ApiEndpoint` | BetaHub API URL (default: `https://app.betahub.io`) |
| `ProjectId` | Your BetaHub project ID (e.g., `pr-xxxxxxxxxx`) |
| `ProjectToken` | Auth token from Project > Integrations > Auth Tokens |
| `ReleaseLabel` | Optional build/version label for reports |
| `bSpawnServiceOnStartup` | Auto-start the service (default: true) |
| `bEnableShortcut` | Enable keyboard shortcut (default: true) |
| `ShortcutKey` | Key to open report form (default: F12) |

## Usage

**In-game:** Press the shortcut key (F12) to open the bug report form.

**Programmatic:**
```cpp
#include "BH_Subsystem.h"

// Get subsystem
UBH_Subsystem* BugReporter = GetGameInstance()->GetSubsystem<UBH_Subsystem>();

// Show form
BugReporter->ShowReportForm();

// Get captured logs
FString Logs = BugReporter->GetCapturedLogs();
```

## Requirements

- Unreal Engine 4.27
- C++ project (or convert Blueprint-only project)
