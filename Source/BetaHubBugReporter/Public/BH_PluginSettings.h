#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "InputCoreTypes.h"
#include "BH_PluginSettings.generated.h"

UCLASS(Config=Game, defaultconfig, meta=(DisplayName="BetaHub Bug Reporter"))
class BETAHUBBUGREPORTER_API UBH_PluginSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UBH_PluginSettings();

    virtual FName GetCategoryName() const override { return FName(TEXT("Plugins")); }

    UPROPERTY(EditAnywhere, Config, Category="Settings",
        meta=(ToolTip="The API endpoint for submitting bug reports."))
    FString ApiEndpoint;

    UPROPERTY(EditAnywhere, Config, Category="Settings",
        meta=(ToolTip="The Project ID of your project on BetaHub. You can find this on the project settings page on BetaHub."))
    FString ProjectId;

    UPROPERTY(EditAnywhere, Config, Category="Settings",
        meta=(ToolTip="The Project token for authentication. Should start with 'tkn-' and can be found under Project -> Integrations -> Auth Tokens."))
    FString ProjectToken;

    UPROPERTY(EditAnywhere, Config, Category="Settings",
        meta=(ToolTip="Optional release label for bug reports. If set, all bug reports will include this label."))
    FString ReleaseLabel;

    UPROPERTY(EditAnywhere, Config, Category="Settings",
        meta=(ToolTip="This will spawn the bug reporter service on game startup. Disable if you're planning to start it manually."))
    bool bSpawnServiceOnStartup;

    UPROPERTY(EditAnywhere, Config, Category="Settings",
        meta=(ToolTip="Enable or disable the shortcut key to open the bug report form."))
    bool bEnableShortcut;

    UPROPERTY(EditAnywhere, Config, Category="Settings",
        meta=(ToolTip="The shortcut key to open the bug report form."))
    FKey ShortcutKey;

    UPROPERTY(EditAnywhere, Config, Category="Settings",
        meta=(ToolTip="Z-order of the report form in the game viewport. The form is drawn above viewport widgets with a lower Z-order. UMG's Add to Viewport places a widget at its Z-order + 10, so to keep the form above a menu added with Z-order N, use a value above N + 10."))
    int32 ReportFormZOrder;
};
