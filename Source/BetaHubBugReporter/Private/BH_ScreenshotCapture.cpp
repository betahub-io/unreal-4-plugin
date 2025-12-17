#include "BH_ScreenshotCapture.h"
#include "BH_Log.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "HAL/PlatformFilemanager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"

FString BH_ScreenshotCapture::CaptureScreenshot()
{
    if (!GEngine || !GEngine->GameViewport)
    {
        UE_LOG(LogBetaHub, Error, TEXT("Cannot capture screenshot - no game viewport"));
        return FString();
    }

    FViewport* Viewport = GEngine->GameViewport->Viewport;
    if (!Viewport)
    {
        UE_LOG(LogBetaHub, Error, TEXT("Cannot capture screenshot - viewport is null"));
        return FString();
    }

    TArray<FColor> Bitmap;
    FIntVector Size(Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y, 0);

    if (Size.X == 0 || Size.Y == 0)
    {
        UE_LOG(LogBetaHub, Error, TEXT("Cannot capture screenshot - viewport size is zero"));
        return FString();
    }

    bool bSuccess = Viewport->ReadPixels(Bitmap);
    if (!bSuccess || Bitmap.Num() == 0)
    {
        UE_LOG(LogBetaHub, Error, TEXT("Failed to read pixels from viewport"));
        return FString();
    }

    IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
    TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);

    if (!ImageWrapper.IsValid())
    {
        UE_LOG(LogBetaHub, Error, TEXT("Failed to create image wrapper"));
        return FString();
    }

    ImageWrapper->SetRaw(Bitmap.GetData(), Bitmap.Num() * sizeof(FColor), Size.X, Size.Y, ERGBFormat::BGRA, 8);

    const TArray64<uint8>& CompressedData64 = ImageWrapper->GetCompressed(85);
    if (CompressedData64.Num() == 0)
    {
        UE_LOG(LogBetaHub, Error, TEXT("Failed to compress screenshot"));
        return FString();
    }

    // Copy to regular TArray for file saving
    TArray<uint8> CompressedData;
    CompressedData.SetNumUninitialized(CompressedData64.Num());
    FMemory::Memcpy(CompressedData.GetData(), CompressedData64.GetData(), CompressedData64.Num());

    FString ScreenshotPath = GetScreenshotPath();
    if (!FFileHelper::SaveArrayToFile(CompressedData, *ScreenshotPath))
    {
        UE_LOG(LogBetaHub, Error, TEXT("Failed to save screenshot to %s"), *ScreenshotPath);
        return FString();
    }

    UE_LOG(LogBetaHub, Log, TEXT("Screenshot saved to %s"), *ScreenshotPath);
    return ScreenshotPath;
}

FString BH_ScreenshotCapture::GetScreenshotPath()
{
    FString TempDir = FPaths::ProjectSavedDir() / TEXT("BetaHub") / TEXT("Screenshots");
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    if (!PlatformFile.DirectoryExists(*TempDir))
    {
        PlatformFile.CreateDirectoryTree(*TempDir);
    }

    FString Filename = FString::Printf(TEXT("screenshot_%s.jpg"), *FGuid::NewGuid().ToString());
    return TempDir / Filename;
}
