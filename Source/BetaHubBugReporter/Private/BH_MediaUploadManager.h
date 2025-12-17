#pragma once

#include "CoreMinimal.h"
#include "BH_S3Uploader.h"
#include "BH_MediaTypes.h"

class BH_MediaUploadManager
{
public:
    BH_MediaUploadManager();
    ~BH_MediaUploadManager();

    struct FUploadProgress
    {
        int32 TotalFiles;
        int32 CompletedFiles;
        FString CurrentFile;
        float ProgressPercent;
    };

    struct FMediaUploadResult
    {
        bool bSuccess;
        int32 ScreenshotsUploaded;
        int32 LogsUploaded;
        int32 TotalFilesUploaded;
        TArray<FString> Errors;
    };

    DECLARE_DELEGATE_OneParam(FOnProgressUpdate, const FUploadProgress&);
    DECLARE_DELEGATE_OneParam(FOnUploadComplete, const FMediaUploadResult&);

    void UploadMediaFiles(
        const FString& BaseUrl,
        const FString& ProjectId,
        const FString& IssueId,
        const FString& ApiToken,
        const TArray<FBH_MediaFile>& Screenshots,
        const TArray<FBH_MediaFile>& Logs,
        const FOnProgressUpdate& OnProgress,
        const FOnUploadComplete& OnComplete
    );

    void CancelUploads();

private:
    void ProcessNextUpload();
    void UpdateProgress(const FString& CurrentFile, float Percent);
    void CompleteUpload();
    FString CreateTempFile(const FString& Contents, const FString& Extension, int32 Index = 0);
    void CleanupTempFiles();

    TSharedPtr<BH_S3Uploader> S3Uploader;

    struct FUploadTask
    {
        EBH_MediaType MediaType;
        FString FilePath;
        FString ContentType;
        FString Description;
        FString CustomName;
        bool bIsTempFile;
    };

    TArray<FUploadTask> UploadQueue;
    int32 CurrentUploadIndex;

    FString CurrentBaseUrl;
    FString CurrentProjectId;
    FString CurrentIssueId;
    FString CurrentApiToken;

    FMediaUploadResult UploadResult;
    TArray<FString> TempFilesToCleanup;

    FOnProgressUpdate ProgressCallback;
    FOnUploadComplete CompleteCallback;

    bool bCancelRequested;
};
