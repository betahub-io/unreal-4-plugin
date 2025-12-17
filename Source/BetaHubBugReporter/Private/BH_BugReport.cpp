#include "BH_BugReport.h"
#include "BH_Log.h"
#include "BH_HttpRequest.h"
#include "BH_MediaUploadManager.h"
#include "Json.h"
#include "Async/Async.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Engine/Engine.h"

UBH_BugReport::UBH_BugReport()
{
}

void UBH_BugReport::SubmitReport(
    UBH_PluginSettings* Settings,
    const FString& Description,
    const FString& StepsToReproduce,
    const TArray<FBH_MediaFile>& Screenshots,
    const TArray<FBH_MediaFile>& Logs,
    TFunction<void()> OnSuccess,
    TFunction<void(const FString&)> OnFailure,
    const FString& ReleaseLabel,
    const FString& ReleaseId)
{
    SubmitReportInternal(Settings, Description, StepsToReproduce,
        Screenshots, Logs, OnSuccess, OnFailure, ReleaseLabel, ReleaseId);
}

void UBH_BugReport::SubmitReportInternal(
    UBH_PluginSettings* Settings,
    const FString& Description,
    const FString& StepsToReproduce,
    const TArray<FBH_MediaFile>& Screenshots,
    const TArray<FBH_MediaFile>& Logs,
    TFunction<void()> OnSuccess,
    TFunction<void(const FString&)> OnFailure,
    const FString& ReleaseLabel,
    const FString& ReleaseId)
{
    if (!Settings)
    {
        UE_LOG(LogBetaHub, Error, TEXT("Settings is null"));
        if (OnFailure)
        {
            OnFailure(TEXT("Invalid settings"));
        }
        return;
    }

    TSharedPtr<BH_HttpRequest> InitialRequest = MakeShared<BH_HttpRequest>();
    InitialRequest->SetURL(Settings->ApiEndpoint + TEXT("/projects/") + Settings->ProjectId + TEXT("/issues.json"));
    InitialRequest->SetVerb("POST");

    if (!Settings->ProjectToken.IsEmpty())
    {
        InitialRequest->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("FormUser %s"), *Settings->ProjectToken));
    }
    else
    {
        InitialRequest->SetHeader(TEXT("Authorization"), TEXT("FormUser anonymous"));
    }

    InitialRequest->SetHeader(TEXT("BetaHub-Project-ID"), Settings->ProjectId);
    InitialRequest->SetHeader(TEXT("Accept"), TEXT("application/json"));
    InitialRequest->AddField(TEXT("issue[description]"), Description);
    InitialRequest->AddField(TEXT("issue[unformatted_steps_to_reproduce]"), StepsToReproduce);
    InitialRequest->AddField(TEXT("draft"), TEXT("true"));

    FString FinalReleaseLabel = ReleaseLabel;
    if (FinalReleaseLabel.IsEmpty() && !Settings->ReleaseLabel.IsEmpty())
    {
        FinalReleaseLabel = Settings->ReleaseLabel;
    }

    if (!ReleaseId.IsEmpty())
    {
        InitialRequest->AddField(TEXT("issue[release_id]"), ReleaseId);
    }
    else if (!FinalReleaseLabel.IsEmpty())
    {
        InitialRequest->AddField(TEXT("issue[release_label]"), FinalReleaseLabel);
    }

    InitialRequest->FinalizeFormData();

    TWeakObjectPtr<UBH_PluginSettings> WeakSettings = Settings;

    InitialRequest->ProcessRequest(
        [WeakSettings, Screenshots, Logs, InitialRequest, OnSuccess, OnFailure]
        (FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
    {
        if (!WeakSettings.IsValid())
        {
            UE_LOG(LogBetaHub, Warning, TEXT("Settings destroyed during async operation"));
            if (OnFailure)
            {
                OnFailure(TEXT("Operation cancelled - settings no longer valid"));
            }
            return;
        }

        UBH_PluginSettings* Settings = WeakSettings.Get();

        if (bWasSuccessful && Response.IsValid() && (Response->GetResponseCode() == 200 || Response->GetResponseCode() == 201))
        {
            FString ContentAsString = Response->GetContentAsString();
            FString IssueId = UBH_BugReport::ParseIssueIdFromResponse(ContentAsString);
            FString ApiToken = UBH_BugReport::ParseTokenFromResponse(ContentAsString);

            if (!IssueId.IsEmpty())
            {
                UE_LOG(LogBetaHub, Log, TEXT("Draft issue created successfully with ID: %s"), *IssueId);

                FString FormattedIssueId = FString::Printf(TEXT("g-%s"), *IssueId);

                TSharedPtr<BH_MediaUploadManager> MediaManager = MakeShareable(new BH_MediaUploadManager());

                BH_MediaUploadManager::FOnUploadComplete UploadCompleteDelegate;
                UploadCompleteDelegate.BindLambda([WeakSettings, FormattedIssueId, ApiToken, OnSuccess, OnFailure, MediaManager]
                    (const BH_MediaUploadManager::FMediaUploadResult& Result)
                {
                    if (!WeakSettings.IsValid())
                    {
                        UE_LOG(LogBetaHub, Warning, TEXT("Settings destroyed during media upload"));
                        if (OnFailure)
                        {
                            OnFailure(TEXT("Operation cancelled - settings destroyed during upload"));
                        }
                        return;
                    }

                    UBH_PluginSettings* Settings = WeakSettings.Get();

                    if (Result.bSuccess)
                    {
                        UE_LOG(LogBetaHub, Log, TEXT("Media upload completed successfully"));
                        UE_LOG(LogBetaHub, Log, TEXT("  - Screenshots: %d"), Result.ScreenshotsUploaded);
                        UE_LOG(LogBetaHub, Log, TEXT("  - Log files: %d"), Result.LogsUploaded);
                    }
                    else if (Result.TotalFilesUploaded > 0)
                    {
                        UE_LOG(LogBetaHub, Warning, TEXT("Some media uploads succeeded (%d of attempted)"), Result.TotalFilesUploaded);
                    }
                    else
                    {
                        UE_LOG(LogBetaHub, Warning, TEXT("All media uploads failed, publishing issue without media"));
                    }

                    PublishIssue(Settings, FormattedIssueId, ApiToken, OnSuccess, OnFailure);
                });

                BH_MediaUploadManager::FOnProgressUpdate ProgressDelegate;
                ProgressDelegate.BindLambda([](const BH_MediaUploadManager::FUploadProgress& Progress)
                {
                    UE_LOG(LogBetaHub, Verbose, TEXT("Upload progress: %s (%.1f%%)"),
                        *Progress.CurrentFile, Progress.ProgressPercent);
                });

                MediaManager->UploadMediaFiles(
                    Settings->ApiEndpoint,
                    Settings->ProjectId,
                    FormattedIssueId,
                    ApiToken,
                    Screenshots,
                    Logs,
                    ProgressDelegate,
                    UploadCompleteDelegate
                );
            }
            else
            {
                UE_LOG(LogBetaHub, Error, TEXT("Failed to parse Issue ID from response: %s"), *ContentAsString);
                if (OnFailure)
                {
                    OnFailure(FString::Printf(TEXT("Failed to parse Issue ID from response: %s"), *ContentAsString));
                }
            }
        }
        else
        {
            FString ErrorMessage = TEXT("Unknown error submitting bug report.");
            FString ResponseContentForLog = TEXT("No response content available.");
            if (Response.IsValid())
            {
                ResponseContentForLog = Response->GetContentAsString();
                ErrorMessage = UBH_BugReport::ParseErrorFromResponse(ResponseContentForLog);
            }
            else if (!bWasSuccessful)
            {
                ErrorMessage = TEXT("HTTP request failed.");
                ResponseContentForLog = ErrorMessage;
            }

            UE_LOG(LogBetaHub, Error, TEXT("Failed to submit bug report: %s"), *ResponseContentForLog);
            if (OnFailure)
            {
                OnFailure(ErrorMessage);
            }
        }
    });
}

FString UBH_BugReport::ParseIssueIdFromResponse(const FString& Response)
{
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response);

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        FString IssueId;
        if (JsonObject->TryGetStringField(TEXT("id"), IssueId))
        {
            return IssueId;
        }
    }

    return FString();
}

FString UBH_BugReport::ParseErrorFromResponse(const FString& Response)
{
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response);

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        FString ErrorMessage;
        if (JsonObject->TryGetStringField(TEXT("error"), ErrorMessage))
        {
            return ErrorMessage;
        }
        return Response;
    }

    return Response;
}

FString UBH_BugReport::ParseTokenFromResponse(const FString& Response)
{
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response);

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        FString Token;
        if (JsonObject->TryGetStringField(TEXT("token"), Token))
        {
            return Token;
        }
    }

    return FString();
}

void UBH_BugReport::PublishIssue(
    UBH_PluginSettings* Settings,
    const FString& IssueId,
    const FString& ApiToken,
    TFunction<void()> OnSuccess,
    TFunction<void(const FString&)> OnFailure)
{
    if (!Settings)
    {
        UE_LOG(LogBetaHub, Error, TEXT("Settings is null in PublishIssue"));
        if (OnFailure)
        {
            OnFailure(TEXT("Invalid settings"));
        }
        return;
    }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    FString Url = Settings->ApiEndpoint + TEXT("/projects/") + Settings->ProjectId + TEXT("/issues/") + IssueId + TEXT("/publish");

    Request->SetURL(Url);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiToken));
    Request->SetHeader(TEXT("BetaHub-Project-ID"), Settings->ProjectId);

    TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
    JsonObject->SetBoolField(TEXT("email_my_report"), false);

    FString JsonString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
    Request->SetContentAsString(JsonString);

    Request->OnProcessRequestComplete().BindLambda(
        [OnSuccess, OnFailure](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bWasSuccessful)
        {
            if (bWasSuccessful && HttpResponse.IsValid() &&
                (HttpResponse->GetResponseCode() == 200 || HttpResponse->GetResponseCode() == 201))
            {
                UE_LOG(LogBetaHub, Log, TEXT("Issue published successfully"));
                if (OnSuccess)
                {
                    OnSuccess();
                }
            }
            else
            {
                FString ErrorMessage = TEXT("Failed to publish issue");
                if (HttpResponse.IsValid())
                {
                    ErrorMessage = FString::Printf(TEXT("Failed to publish issue: HTTP %d - %s"),
                        HttpResponse->GetResponseCode(), *HttpResponse->GetContentAsString());
                }
                else if (!bWasSuccessful)
                {
                    ErrorMessage = TEXT("Failed to publish issue: Network request failed");
                }

                UE_LOG(LogBetaHub, Error, TEXT("%s"), *ErrorMessage);
                if (OnFailure)
                {
                    OnFailure(ErrorMessage);
                }
            }
        }
    );

    Request->ProcessRequest();
}
