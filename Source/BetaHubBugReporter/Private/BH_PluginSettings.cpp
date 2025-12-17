#include "BH_PluginSettings.h"

UBH_PluginSettings::UBH_PluginSettings()
{
    ApiEndpoint = TEXT("https://app.betahub.io");
    ProjectId = TEXT("pr-5287510306");
    ProjectToken = TEXT("tkn-15e6fbc1470613d5cfd2199edbde52157379b8c6dcd365441eabf8fea62d76a7");
    bSpawnServiceOnStartup = true;
    bEnableShortcut = true;
    ShortcutKey = EKeys::F12;
}
