# BetaHub Bug Reporter

In-game bug reporting and feedback collection plugin for Unreal Engine 4.27. Integrates with [BetaHub](https://betahub.io) to streamline QA during playtesting.

> **Using Unreal Engine 5+?** Check out the full-featured plugin with video recording support: [github.com/betahub-io/unreal-plugin](https://github.com/betahub-io/unreal-plugin)

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
| `ReportFormZOrder` | Z-order of the report form in the game viewport (default: 1000) |

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

**Closing the form:** when the form closes, the plugin restores the mouse cursor and input mode the game had before it opened. Opening it from a pause menu returns the player to the pause menu with the cursor visible.

To run your own logic after the form closes, bind to `OnReportFormClosed` (Blueprint: *Bind Event to On Report Form Closed* on the BH_Subsystem node):

```cpp
BugReporter->OnReportFormClosed.AddDynamic(this, &UMyPauseMenu::HandleReportFormClosed);
```

**Form above your menus:** the form is drawn above viewport widgets with a lower `ReportFormZOrder`. UMG's *Add to Viewport* places a widget at its Z-order + 10, so the default 1000 keeps the form above menus added with Z-order below 990. Raise the setting if your menus use a higher Z-order.

## Requirements

- Unreal Engine 4.27
- C++ project (or convert Blueprint-only project)
