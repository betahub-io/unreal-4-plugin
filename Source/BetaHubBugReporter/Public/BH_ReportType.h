#pragma once

#include "CoreMinimal.h"

UENUM(BlueprintType)
enum class EBH_ReportType : uint8
{
    Bug UMETA(DisplayName = "Bug Report"),
    FeatureRequest UMETA(DisplayName = "Feature Request")
};
