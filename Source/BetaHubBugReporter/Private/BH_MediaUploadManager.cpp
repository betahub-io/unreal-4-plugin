#include "BH_MediaUploadManager.h"
#include "BH_Log.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Guid.h"

BH_MediaUploadManager::BH_MediaUploadManager()
    : CurrentUploadIndex(0)
    , bCancelRequested(false)
{
    S3Uploader = MakeShareable(new BH_S3Uploader());
}

BH_MediaUploadManager::~BH_MediaUploadManager()
{
    CleanupTempFiles();
}

void BH_MediaUploadManager::UploadMediaFiles(
    const FString& BaseUrl,
    const FString& ProjectId,
    const FString& IssueId,
    const FString& ApiToken,
    const TArray<FBH_MediaFile>& Screenshots,
    const TArray<FBH_MediaFile>& Logs,
    const FOnProgressUpdate& OnProgress,
    const FOnUploadComplete& OnComplete)
{
    UploadQueue.Empty();
    CurrentUploadIndex = 0;
    bCancelRequested = false;
    UploadResult = FMediaUploadResult();
    UploadResult.bSuccess = true;
    UploadResult.ScreenshotsUploaded = 0;
    UploadResult.LogsUploaded = 0;
    UploadResult.TotalFilesUploaded = 0;

    CurrentBaseUrl = BaseUrl;
    CurrentProjectId = ProjectId;
    CurrentIssueId = IssueId;
    CurrentApiToken = ApiToken;
    ProgressCallback = OnProgress;
    CompleteCallback = OnComplete;

    // Build upload queue for screenshots
    int32 ScreenshotIndex = 1;
    for (const FBH_MediaFile& Screenshot : Screenshots)
    {
        bool bHasFilePath = !Screenshot.FilePath.IsEmpty();
        bool bHasContent = !Screenshot.Content.IsEmpty();

        if (!bHasFilePath && !bHasContent)
        {
            continue;
        }

        if (bHasFilePath && bHasContent)
        {
            UE_LOG(LogBetaHub, Error, TEXT("Screenshot entry skipped: both FilePath and Content are set"));
            continue;
        }

        FUploadTask Task;
        Task.MediaType = EBH_MediaType::Screenshot;
        Task.ContentType = BH_MediaTypeHelper::GetDefaultContentType(EBH_MediaType::Screenshot, Screenshot.FilePath);
        Task.CustomName = Screenshot.Name;

        if (bHasFilePath)
        {
            if (!FPaths::FileExists(Screenshot.FilePath))
            {
                UE_LOG(LogBetaHub, Warning, TEXT("Screenshot file not found, skipping: %s"), *Screenshot.FilePath);
                continue;
            }

            Task.FilePath = Screenshot.FilePath;
            Task.bIsTempFile = false;
        }
        else
        {
            UE_LOG(LogBetaHub, Error, TEXT("Screenshot entry skipped: Content mode not supported for screenshots"));
            continue;
        }

        Task.Description = (Screenshots.Num() > 1)
            ? FString::Printf(TEXT("Screenshot %d of %d"), ScreenshotIndex, Screenshots.Num())
            : TEXT("screenshot");

        UploadQueue.Add(Task);
        ScreenshotIndex++;
    }

    // Build upload queue for log files
    int32 LogIndex = 1;
    for (const FBH_MediaFile& Log : Logs)
    {
        bool bHasFilePath = !Log.FilePath.IsEmpty();
        bool bHasContent = !Log.Content.IsEmpty();

        if (!bHasFilePath && !bHasContent)
        {
            continue;
        }

        if (bHasFilePath && bHasContent)
        {
            UE_LOG(LogBetaHub, Error, TEXT("Log entry skipped: both FilePath and Content are set"));
            continue;
        }

        FUploadTask Task;
        Task.MediaType = EBH_MediaType::LogFile;
        Task.CustomName = Log.Name;

        if (bHasFilePath)
        {
            if (!FPaths::FileExists(Log.FilePath))
            {
                UE_LOG(LogBetaHub, Warning, TEXT("Log file not found, skipping: %s"), *Log.FilePath);
                continue;
            }

            FString FileContent;
            if (!FFileHelper::LoadFileToString(FileContent, *Log.FilePath))
            {
                UE_LOG(LogBetaHub, Error, TEXT("Failed to read log file, skipping: %s"), *Log.FilePath);
                continue;
            }

            FString TempLogPath = CreateTempFile(FileContent, TEXT("log"), LogIndex - 1);
            if (TempLogPath.IsEmpty())
            {
                continue;
            }

            Task.FilePath = TempLogPath;
            Task.ContentType = BH_MediaTypeHelper::GetDefaultContentType(EBH_MediaType::LogFile, TempLogPath);
            Task.bIsTempFile = true;
            TempFilesToCleanup.Add(TempLogPath);
        }
        else
        {
            if (Log.Content.IsEmpty())
            {
                continue;
            }

            FString TempLogPath = CreateTempFile(Log.Content, TEXT("log"), LogIndex - 1);
            if (TempLogPath.IsEmpty())
            {
                continue;
            }

            Task.FilePath = TempLogPath;
            Task.ContentType = BH_MediaTypeHelper::GetDefaultContentType(EBH_MediaType::LogFile, TempLogPath);
            Task.bIsTempFile = true;
            TempFilesToCleanup.Add(TempLogPath);
        }

        Task.Description = (Logs.Num() > 1)
            ? FString::Printf(TEXT("Log file %d of %d"), LogIndex, Logs.Num())
            : TEXT("log file");

        UploadQueue.Add(Task);
        LogIndex++;
    }

    if (UploadQueue.Num() == 0)
    {
        UE_LOG(LogBetaHub, Warning, TEXT("No media files to upload"));
        UploadResult.bSuccess = true;
        CompleteCallback.ExecuteIfBound(UploadResult);
        return;
    }

    UE_LOG(LogBetaHub, Log, TEXT("Starting upload of %d media files"), UploadQueue.Num());
    ProcessNextUpload();
}

void BH_MediaUploadManager::ProcessNextUpload()
{
    if (bCancelRequested)
    {
        UE_LOG(LogBetaHub, Warning, TEXT("Upload cancelled"));
        UploadResult.bSuccess = false;
        UploadResult.Errors.Add(TEXT("Upload cancelled by user"));
        CompleteUpload();
        return;
    }

    if (CurrentUploadIndex >= UploadQueue.Num())
    {
        CompleteUpload();
        return;
    }

    const FUploadTask& Task = UploadQueue[CurrentUploadIndex];
    UpdateProgress(Task.Description, 0.0f);

    UE_LOG(LogBetaHub, Log, TEXT("Uploading %s (%d of %d)"),
        *Task.Description, CurrentUploadIndex + 1, UploadQueue.Num());

    BH_S3Uploader::FOnUploadComplete UploadDelegate;
    UploadDelegate.BindLambda([this, Task](const BH_S3Uploader::FUploadResult& Result)
    {
        if (Result.bSuccess)
        {
            UE_LOG(LogBetaHub, Log, TEXT("%s uploaded successfully"), *Task.Description);

            switch (Task.MediaType)
            {
                case EBH_MediaType::Screenshot:
                    UploadResult.ScreenshotsUploaded++;
                    break;
                case EBH_MediaType::LogFile:
                    UploadResult.LogsUploaded++;
                    break;
            }

            UploadResult.TotalFilesUploaded++;
        }
        else
        {
            UE_LOG(LogBetaHub, Error, TEXT("Failed to upload %s: %s"),
                *Task.Description, *Result.ErrorMessage);

            UploadResult.Errors.Add(FString::Printf(TEXT("%s: %s"),
                *Task.Description, *Result.ErrorMessage));
        }

        if (Task.bIsTempFile && FPaths::FileExists(Task.FilePath))
        {
            IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
            PlatformFile.DeleteFile(*Task.FilePath);
        }

        CurrentUploadIndex++;
        ProcessNextUpload();
    });

    FString MediaEndpoint = BH_MediaTypeHelper::GetAPIEndpoint(Task.MediaType);

    S3Uploader->UploadFileToS3(
        CurrentBaseUrl,
        CurrentProjectId,
        CurrentIssueId,
        CurrentApiToken,
        MediaEndpoint,
        Task.FilePath,
        Task.ContentType,
        Task.CustomName,
        UploadDelegate
    );
}

void BH_MediaUploadManager::UpdateProgress(const FString& CurrentFile, float Percent)
{
    FUploadProgress Progress;
    Progress.TotalFiles = UploadQueue.Num();
    Progress.CompletedFiles = CurrentUploadIndex;
    Progress.CurrentFile = CurrentFile;
    Progress.ProgressPercent = ((float)CurrentUploadIndex + Percent) / (float)UploadQueue.Num() * 100.0f;

    ProgressCallback.ExecuteIfBound(Progress);
}

void BH_MediaUploadManager::CompleteUpload()
{
    CleanupTempFiles();

    UploadResult.bSuccess = (UploadResult.TotalFilesUploaded > 0) || (UploadQueue.Num() == 0);

    UE_LOG(LogBetaHub, Log, TEXT("Media upload complete:"));
    UE_LOG(LogBetaHub, Log, TEXT("  - Total files uploaded: %d of %d"),
        UploadResult.TotalFilesUploaded, UploadQueue.Num());
    UE_LOG(LogBetaHub, Log, TEXT("  - Screenshots: %d"), UploadResult.ScreenshotsUploaded);
    UE_LOG(LogBetaHub, Log, TEXT("  - Log files: %d"), UploadResult.LogsUploaded);

    CompleteCallback.ExecuteIfBound(UploadResult);
}

void BH_MediaUploadManager::CancelUploads()
{
    bCancelRequested = true;
}

FString BH_MediaUploadManager::CreateTempFile(const FString& Contents, const FString& Extension, int32 Index)
{
    FString TempDir = FPaths::ProjectSavedDir() / TEXT("BetaHub") / TEXT("Temp");
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    if (!PlatformFile.DirectoryExists(*TempDir))
    {
        PlatformFile.CreateDirectoryTree(*TempDir);
    }

    FString Filename = (Index > 0)
        ? FString::Printf(TEXT("betahub_%s_%d.%s"), *FGuid::NewGuid().ToString(), Index, *Extension)
        : FString::Printf(TEXT("betahub_%s.%s"), *FGuid::NewGuid().ToString(), *Extension);

    FString FilePath = TempDir / Filename;

    if (FFileHelper::SaveStringToFile(Contents, *FilePath))
    {
        return FilePath;
    }

    UE_LOG(LogBetaHub, Error, TEXT("Failed to create temp file: %s"), *FilePath);
    return FString();
}

void BH_MediaUploadManager::CleanupTempFiles()
{
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    for (const FString& TempFile : TempFilesToCleanup)
    {
        if (PlatformFile.FileExists(*TempFile))
        {
            PlatformFile.DeleteFile(*TempFile);
        }
    }

    TempFilesToCleanup.Empty();
}
