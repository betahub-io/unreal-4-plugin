#include "BH_PluginSettings.h"

namespace
{
    // Above typical game menus: UMG's Add to Viewport places a widget at its Z-order + 10.
    constexpr int32 DefaultReportFormZOrder = 1000;
}

UBH_PluginSettings::UBH_PluginSettings()
{
    ApiEndpoint = TEXT("https://app.betahub.io");
    ProjectId = TEXT("pr-5287510306");
    ProjectToken = TEXT("tkn-15e6fbc1470613d5cfd2199edbde52157379b8c6dcd365441eabf8fea62d76a7");
    bSpawnServiceOnStartup = true;
    bEnableShortcut = true;
    ShortcutKey = EKeys::F12;
    ReportFormZOrder = DefaultReportFormZOrder;
}
