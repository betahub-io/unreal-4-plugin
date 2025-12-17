#pragma once

#include "CoreMinimal.h"
#include "BH_MediaTypes.generated.h"

UENUM()
enum class EBH_MediaType : uint8
{
    Screenshot      UMETA(DisplayName = "Screenshot"),
    LogFile        UMETA(DisplayName = "Log File"),
};

USTRUCT(BlueprintType)
struct BETAHUBBUGREPORTER_API FBH_MediaFile
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "BetaHub")
    FString FilePath;

    UPROPERTY(BlueprintReadWrite, Category = "BetaHub")
    FString Content;

    UPROPERTY(BlueprintReadWrite, Category = "BetaHub")
    FString Name;

    FBH_MediaFile()
        : FilePath(TEXT(""))
        , Content(TEXT(""))
        , Name(TEXT(""))
    {
    }

    FBH_MediaFile(const FString& InFilePath, const FString& InName = TEXT(""))
        : FilePath(InFilePath)
        , Content(TEXT(""))
        , Name(InName)
    {
    }
};

class BETAHUBBUGREPORTER_API BH_MediaTypeHelper
{
public:
    static FString GetAPIEndpoint(EBH_MediaType MediaType)
    {
        switch (MediaType)
        {
            case EBH_MediaType::Screenshot:
                return TEXT("screenshots");
            case EBH_MediaType::LogFile:
                return TEXT("log_files");
            default:
                return TEXT("");
        }
    }

    static FString GetDefaultContentType(EBH_MediaType MediaType, const FString& FilePath = TEXT(""))
    {
        if (!FilePath.IsEmpty())
        {
            FString Extension = FPaths::GetExtension(FilePath).ToLower();

            if (Extension == TEXT("png")) return TEXT("image/png");
            if (Extension == TEXT("jpg") || Extension == TEXT("jpeg")) return TEXT("image/jpeg");
            if (Extension == TEXT("log") || Extension == TEXT("txt")) return TEXT("text/plain");
        }

        switch (MediaType)
        {
            case EBH_MediaType::Screenshot:
                return TEXT("image/jpeg");
            case EBH_MediaType::LogFile:
                return TEXT("text/plain");
            default:
                return TEXT("application/octet-stream");
        }
    }
};
