#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "BH_PluginSettings.h"
#include "BH_MediaTypes.h"
#include "BH_BugReport.generated.h"

UCLASS()
class BETAHUBBUGREPORTER_API UBH_BugReport : public UObject
{
    GENERATED_BODY()

public:
    UBH_BugReport();

    void SubmitReport(
        UBH_PluginSettings* Settings,
        const FString& Description,
        const FString& StepsToReproduce,
        const TArray<FBH_MediaFile>& Screenshots,
        const TArray<FBH_MediaFile>& Logs,
        TFunction<void()> OnSuccess,
        TFunction<void(const FString&)> OnFailure,
        const FString& ReleaseLabel = TEXT(""),
        const FString& ReleaseId = TEXT(""));

private:
    void SubmitReportInternal(
        UBH_PluginSettings* Settings,
        const FString& Description,
        const FString& StepsToReproduce,
        const TArray<FBH_MediaFile>& Screenshots,
        const TArray<FBH_MediaFile>& Logs,
        TFunction<void()> OnSuccess,
        TFunction<void(const FString&)> OnFailure,
        const FString& ReleaseLabel,
        const FString& ReleaseId);

    static FString ParseIssueIdFromResponse(const FString& Response);
    static FString ParseErrorFromResponse(const FString& Response);
    static FString ParseTokenFromResponse(const FString& Response);

    static void PublishIssue(
        UBH_PluginSettings* Settings,
        const FString& IssueId,
        const FString& ApiToken,
        TFunction<void()> OnSuccess,
        TFunction<void(const FString&)> OnFailure
    );
};
