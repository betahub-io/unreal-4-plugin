#pragma once

#include "CoreMinimal.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Dom/JsonObject.h"

class BH_S3Uploader
{
public:
    BH_S3Uploader();
    ~BH_S3Uploader();

    struct FUploadResult
    {
        bool bSuccess;
        FString ErrorMessage;
        FString BlobSignedId;
    };

    DECLARE_DELEGATE_OneParam(FOnUploadComplete, const FUploadResult&);

    void UploadFileToS3(
        const FString& BaseUrl,
        const FString& ProjectId,
        const FString& IssueId,
        const FString& ApiToken,
        const FString& MediaEndpoint,
        const FString& FilePath,
        const FString& ContentType,
        const FString& CustomName,
        const FOnUploadComplete& OnComplete
    );

private:
    void RequestPresignedUrl(
        const FString& Endpoint,
        const FString& ApiToken,
        const FString& Filename,
        int64 FileSize,
        const FString& Checksum,
        const FString& ContentType,
        TFunction<void(bool, const FString&, const FString&, TSharedPtr<FJsonObject>)> OnComplete
    );

    void UploadToS3(
        const FString& UploadUrl,
        const TSharedPtr<FJsonObject>& Headers,
        const TArray<uint8>& FileData,
        TFunction<void(bool, const FString&)> OnComplete
    );

    void ConfirmUpload(
        const FString& Endpoint,
        const FString& ApiToken,
        const FString& BlobSignedId,
        const FString& CustomName,
        TFunction<void(bool, const FString&)> OnComplete
    );

    FString CalculateMD5Checksum(const TArray<uint8>& FileData);
};
