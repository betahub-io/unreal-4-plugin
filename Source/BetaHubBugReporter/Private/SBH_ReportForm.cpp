#include "SBH_ReportForm.h"
#include "BH_Subsystem.h"
#include "BH_PluginSettings.h"
#include "BH_BugReport.h"
#include "BH_FeatureRequest.h"
#include "BH_MediaTypes.h"
#include "BH_Log.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFilemanager.h"

#define LOCTEXT_NAMESPACE "BetaHubReportForm"

void SBH_ReportForm::Construct(const FArguments& InArgs)
{
    Subsystem = InArgs._Subsystem;
    ScreenshotPath = InArgs._ScreenshotPath;
    bIsSubmitting = false;
    bSubmissionSucceeded = false;
    CurrentReportType = EBH_ReportType::Bug;

    ChildSlot
    [
        SNew(SBox)
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        [
            SNew(SBox)
            .WidthOverride(500.0f)
            .HeightOverride(550.0f)
            [
                SNew(SBorder)
                .BorderImage(FCoreStyle::Get().GetBrush("Menu.Background"))
                .Padding(FMargin(15.0f))
                [
                    SNew(SVerticalBox)

                    // Title
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0, 0, 0, 15)
                    [
                        SAssignNew(FormTitleText, STextBlock)
                        .Text(LOCTEXT("ReportBugTitle", "Report Bug"))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
                    ]

                    // Type Selector
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0, 0, 0, 15)
                    [
                        SAssignNew(TypeSelectorBox, SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(0, 0, 20, 0)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            [
                                SAssignNew(BugReportCheckBox, SCheckBox)
                                .IsChecked(ECheckBoxState::Checked)
                                .OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
                                {
                                    if (NewState == ECheckBoxState::Checked)
                                    {
                                        OnReportTypeChanged(EBH_ReportType::Bug);
                                    }
                                    else if (CurrentReportType == EBH_ReportType::Bug)
                                    {
                                        // Don't allow unchecking the current selection
                                        BugReportCheckBox->SetIsChecked(ECheckBoxState::Checked);
                                    }
                                })
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .Padding(5, 0, 0, 0)
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("BugReport", "Bug Report"))
                            ]
                        ]

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            [
                                SAssignNew(FeatureRequestCheckBox, SCheckBox)
                                .IsChecked(ECheckBoxState::Unchecked)
                                .OnCheckStateChanged_Lambda([this](ECheckBoxState NewState)
                                {
                                    if (NewState == ECheckBoxState::Checked)
                                    {
                                        OnReportTypeChanged(EBH_ReportType::FeatureRequest);
                                    }
                                    else if (CurrentReportType == EBH_ReportType::FeatureRequest)
                                    {
                                        // Don't allow unchecking the current selection
                                        FeatureRequestCheckBox->SetIsChecked(ECheckBoxState::Checked);
                                    }
                                })
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .Padding(5, 0, 0, 0)
                            [
                                SNew(STextBlock)
                                .Text(LOCTEXT("Suggestion", "Suggestion"))
                            ]
                        ]
                    ]

                    // Suggestion Fields (Description only)
                    + SVerticalBox::Slot()
                    .FillHeight(1.0f)
                    [
                        SAssignNew(FeatureRequestFieldsBox, SVerticalBox)
                        .Visibility(EVisibility::Collapsed)

                        // Description Label
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0, 10, 0, 5)
                        [
                            SNew(STextBlock)
                            .Text(LOCTEXT("SuggestionDescriptionLabel", "Description"))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                        ]

                        // Description Input for suggestions
                        + SVerticalBox::Slot()
                        .FillHeight(1.0f)
                        .Padding(0, 0, 0, 10)
                        [
                            SAssignNew(FeatureDescriptionTextBox, SMultiLineEditableTextBox)
                            .HintText(LOCTEXT("SuggestionDescriptionHint", "Describe your suggestion in detail..."))
                            .AutoWrapText(true)
                        ]
                    ]

                    // Bug Report Fields (Description + Steps + Checkboxes)
                    + SVerticalBox::Slot()
                    .FillHeight(1.0f)
                    [
                        SAssignNew(BugReportFieldsBox, SVerticalBox)

                        // Description Label
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0, 10, 0, 5)
                        [
                            SNew(STextBlock)
                            .Text(LOCTEXT("DescriptionLabel", "Description"))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                        ]

                        // Description Input
                        + SVerticalBox::Slot()
                        .FillHeight(0.4f)
                        .Padding(0, 0, 0, 15)
                        [
                            SAssignNew(DescriptionTextBox, SMultiLineEditableTextBox)
                            .HintText(LOCTEXT("DescriptionHint", "Describe the bug..."))
                            .AutoWrapText(true)
                        ]

                        // Steps Label
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0, 10, 0, 5)
                        [
                            SNew(STextBlock)
                            .Text(LOCTEXT("StepsLabel", "Steps to Reproduce"))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                        ]

                        // Steps Input
                        + SVerticalBox::Slot()
                        .FillHeight(0.4f)
                        .Padding(0, 0, 0, 15)
                        [
                            SAssignNew(StepsTextBox, SMultiLineEditableTextBox)
                            .HintText(LOCTEXT("StepsHint", "1. Do this\n2. Then do that\n3. Bug occurs"))
                            .AutoWrapText(true)
                        ]

                        // Checkboxes
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0, 5, 0, 5)
                        [
                            SNew(SHorizontalBox)

                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .Padding(0, 0, 20, 0)
                            [
                                SNew(SHorizontalBox)
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                [
                                    SAssignNew(IncludeScreenshotCheckBox, SCheckBox)
                                    .IsChecked(ECheckBoxState::Checked)
                                ]
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .Padding(5, 0, 0, 0)
                                [
                                    SNew(STextBlock)
                                    .Text(LOCTEXT("IncludeScreenshot", "Include Screenshot"))
                                ]
                            ]

                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            [
                                SNew(SHorizontalBox)
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                [
                                    SAssignNew(IncludeLogsCheckBox, SCheckBox)
                                    .IsChecked(ECheckBoxState::Checked)
                                ]
                                + SHorizontalBox::Slot()
                                .AutoWidth()
                                .Padding(5, 0, 0, 0)
                                [
                                    SNew(STextBlock)
                                    .Text(LOCTEXT("IncludeLogs", "Include Logs"))
                                ]
                            ]
                        ]
                    ]

                    // Status Text
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0, 5, 0, 5)
                    [
                        SAssignNew(StatusText, STextBlock)
                        .Text(FText::GetEmpty())
                        .ColorAndOpacity(FSlateColor(FLinearColor::Yellow))
                    ]

                    // Buttons
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .HAlign(HAlign_Right)
                    .Padding(0, 10, 0, 0)
                    [
                        SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(0, 0, 10, 0)
                        [
                            SAssignNew(CancelButton, SButton)
                            .Text(LOCTEXT("CancelButton", "Cancel"))
                            .OnClicked(this, &SBH_ReportForm::OnCancelClicked)
                        ]

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        [
                            SAssignNew(SubmitButton, SButton)
                            .Text(LOCTEXT("SubmitButton", "Submit"))
                            .OnClicked(this, &SBH_ReportForm::OnSubmitClicked)
                        ]
                    ]
                ]
            ]
        ]
    ];
}

void SBH_ReportForm::OnReportTypeChanged(EBH_ReportType NewType)
{
    if (CurrentReportType == NewType)
    {
        return;
    }

    CurrentReportType = NewType;

    // Update checkboxes
    BugReportCheckBox->SetIsChecked(NewType == EBH_ReportType::Bug ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
    FeatureRequestCheckBox->SetIsChecked(NewType == EBH_ReportType::FeatureRequest ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);

    UpdateFormForReportType();
}

void SBH_ReportForm::UpdateFormForReportType()
{
    if (CurrentReportType == EBH_ReportType::Bug)
    {
        FormTitleText->SetText(LOCTEXT("ReportBugTitle", "Report Bug"));
        BugReportFieldsBox->SetVisibility(EVisibility::Visible);
        FeatureRequestFieldsBox->SetVisibility(EVisibility::Collapsed);
    }
    else
    {
        FormTitleText->SetText(LOCTEXT("SuggestionTitle", "Submit Suggestion"));
        BugReportFieldsBox->SetVisibility(EVisibility::Collapsed);
        FeatureRequestFieldsBox->SetVisibility(EVisibility::Visible);
    }
}

void SBH_ReportForm::Show()
{
    SetVisibility(EVisibility::Visible);
    ResetForm();
}

void SBH_ReportForm::Hide()
{
    SetVisibility(EVisibility::Collapsed);
}

void SBH_ReportForm::ResetForm()
{
    if (DescriptionTextBox.IsValid())
    {
        DescriptionTextBox->SetText(FText::GetEmpty());
    }
    if (StepsTextBox.IsValid())
    {
        StepsTextBox->SetText(FText::GetEmpty());
    }
    if (FeatureDescriptionTextBox.IsValid())
    {
        FeatureDescriptionTextBox->SetText(FText::GetEmpty());
    }
    if (StatusText.IsValid())
    {
        StatusText->SetText(FText::GetEmpty());
    }

    // Reset to bug report mode
    CurrentReportType = EBH_ReportType::Bug;
    BugReportCheckBox->SetIsChecked(ECheckBoxState::Checked);
    FeatureRequestCheckBox->SetIsChecked(ECheckBoxState::Unchecked);
    UpdateFormForReportType();

    // Reset button states
    SubmitButton->SetContent(
        SNew(STextBlock)
        .Text(LOCTEXT("SubmitButton", "Submit"))
    );
    CancelButton->SetVisibility(EVisibility::Visible);

    bIsSubmitting = false;
    bSubmissionSucceeded = false;
}

FReply SBH_ReportForm::OnSubmitClicked()
{
    // If submission already succeeded, this button is now "Close"
    if (bSubmissionSucceeded)
    {
        if (Subsystem)
        {
            Subsystem->HideReportForm();
        }
        return FReply::Handled();
    }

    if (bIsSubmitting)
    {
        return FReply::Handled();
    }

    // Get description from the appropriate text box based on report type
    FString Description;
    if (CurrentReportType == EBH_ReportType::Bug)
    {
        Description = DescriptionTextBox->GetText().ToString();
    }
    else
    {
        Description = FeatureDescriptionTextBox->GetText().ToString();
    }

    if (Description.IsEmpty())
    {
        StatusText->SetText(LOCTEXT("DescriptionRequired", "Description is required"));
        StatusText->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
        return FReply::Handled();
    }

    bIsSubmitting = true;
    StatusText->SetText(LOCTEXT("Submitting", "Submitting..."));
    StatusText->SetColorAndOpacity(FSlateColor(FLinearColor::Yellow));
    SubmitButton->SetEnabled(false);

    UBH_PluginSettings* Settings = GetMutableDefault<UBH_PluginSettings>();

    TWeakPtr<SBH_ReportForm> WeakForm = SharedThis(this);
    FString CapturedScreenshotPath = ScreenshotPath;

    if (CurrentReportType == EBH_ReportType::Bug)
    {
        // Submit bug report
        FString Steps = StepsTextBox->GetText().ToString();

        TArray<FBH_MediaFile> Screenshots;
        TArray<FBH_MediaFile> Logs;

        if (IncludeScreenshotCheckBox->IsChecked() && !ScreenshotPath.IsEmpty())
        {
            FBH_MediaFile Screenshot;
            Screenshot.FilePath = ScreenshotPath;
            Screenshots.Add(Screenshot);
        }

        if (IncludeLogsCheckBox->IsChecked() && Subsystem)
        {
            FString LogContent = Subsystem->GetCapturedLogs();
            if (!LogContent.IsEmpty())
            {
                FBH_MediaFile Log;
                Log.Content = LogContent;
                Logs.Add(Log);
            }
        }

        UBH_BugReport* BugReport = NewObject<UBH_BugReport>();
        BugReport->AddToRoot(); // Prevent GC during async operation

        BugReport->SubmitReport(
            Settings,
            Description,
            Steps,
            Screenshots,
            Logs,
            [WeakForm, BugReport, CapturedScreenshotPath]()
            {
                BugReport->RemoveFromRoot();

                // Cleanup screenshot file
                if (!CapturedScreenshotPath.IsEmpty())
                {
                    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
                    if (PlatformFile.FileExists(*CapturedScreenshotPath))
                    {
                        PlatformFile.DeleteFile(*CapturedScreenshotPath);
                    }
                }

                if (TSharedPtr<SBH_ReportForm> Form = WeakForm.Pin())
                {
                    Form->OnSubmissionComplete(true, TEXT(""));
                }
            },
            [WeakForm, BugReport, CapturedScreenshotPath](const FString& ErrorMessage)
            {
                BugReport->RemoveFromRoot();

                // Cleanup screenshot file on failure too
                if (!CapturedScreenshotPath.IsEmpty())
                {
                    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
                    if (PlatformFile.FileExists(*CapturedScreenshotPath))
                    {
                        PlatformFile.DeleteFile(*CapturedScreenshotPath);
                    }
                }

                if (TSharedPtr<SBH_ReportForm> Form = WeakForm.Pin())
                {
                    Form->OnSubmissionComplete(false, ErrorMessage);
                }
            }
        );
    }
    else
    {
        // Submit suggestion
        UBH_FeatureRequest* FeatureRequest = NewObject<UBH_FeatureRequest>();
        FeatureRequest->AddToRoot(); // Prevent GC during async operation

        // Cleanup screenshot since feature requests don't use it
        if (!CapturedScreenshotPath.IsEmpty())
        {
            IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
            if (PlatformFile.FileExists(*CapturedScreenshotPath))
            {
                PlatformFile.DeleteFile(*CapturedScreenshotPath);
            }
        }

        FeatureRequest->SubmitFeatureRequest(
            Settings,
            TEXT(""),  // No title for suggestions
            Description,
            [WeakForm, FeatureRequest]()
            {
                FeatureRequest->RemoveFromRoot();

                if (TSharedPtr<SBH_ReportForm> Form = WeakForm.Pin())
                {
                    Form->OnSubmissionComplete(true, TEXT(""));
                }
            },
            [WeakForm, FeatureRequest](const FString& ErrorMessage)
            {
                FeatureRequest->RemoveFromRoot();

                if (TSharedPtr<SBH_ReportForm> Form = WeakForm.Pin())
                {
                    Form->OnSubmissionComplete(false, ErrorMessage);
                }
            }
        );
    }

    return FReply::Handled();
}

FReply SBH_ReportForm::OnCancelClicked()
{
    if (Subsystem)
    {
        Subsystem->HideReportForm();
    }

    // Cleanup screenshot if canceling
    if (!ScreenshotPath.IsEmpty())
    {
        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        if (PlatformFile.FileExists(*ScreenshotPath))
        {
            PlatformFile.DeleteFile(*ScreenshotPath);
        }
    }

    return FReply::Handled();
}

void SBH_ReportForm::OnSubmissionComplete(bool bSuccess, const FString& ErrorMessage)
{
    bIsSubmitting = false;
    SubmitButton->SetEnabled(true);

    if (bSuccess)
    {
        if (CurrentReportType == EBH_ReportType::Bug)
        {
            StatusText->SetText(LOCTEXT("SubmitSuccess", "Bug report submitted successfully!"));
        }
        else
        {
            StatusText->SetText(LOCTEXT("SuggestionSubmitSuccess", "Suggestion submitted successfully!"));
        }
        StatusText->SetColorAndOpacity(FSlateColor(FLinearColor::Green));

        // Change Submit button to Close
        SubmitButton->SetContent(
            SNew(STextBlock)
            .Text(LOCTEXT("CloseButton", "Close"))
        );
        CancelButton->SetVisibility(EVisibility::Collapsed);
        bSubmissionSucceeded = true;
    }
    else
    {
        StatusText->SetText(FText::FromString(FString::Printf(TEXT("Failed: %s"), *ErrorMessage)));
        StatusText->SetColorAndOpacity(FSlateColor(FLinearColor::Red));
    }
}

#undef LOCTEXT_NAMESPACE
