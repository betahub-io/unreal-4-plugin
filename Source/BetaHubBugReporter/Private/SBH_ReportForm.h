#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "BH_ReportType.h"

class UBH_Subsystem;

class SBH_ReportForm : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SBH_ReportForm)
        : _Subsystem(nullptr)
        , _ScreenshotPath(TEXT(""))
    {}
        SLATE_ARGUMENT(UBH_Subsystem*, Subsystem)
        SLATE_ARGUMENT(FString, ScreenshotPath)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    // The subsystem focuses the form when it opens; UI-only input mode requires a focusable widget.
    virtual bool SupportsKeyboardFocus() const override { return true; }

    void Show();
    void Hide();
    void ResetForm();

private:
    FReply OnSubmitClicked();
    FReply OnCancelClicked();

    void OnReportTypeChanged(EBH_ReportType NewType);
    void UpdateFormForReportType();

    void OnSubmissionComplete(bool bSuccess, const FString& ErrorMessage);

    // Type selector
    TSharedPtr<class SHorizontalBox> TypeSelectorBox;

    // Bug report specific widgets
    TSharedPtr<class SVerticalBox> BugReportFieldsBox;
    TSharedPtr<class SMultiLineEditableTextBox> StepsTextBox;
    TSharedPtr<class SCheckBox> IncludeScreenshotCheckBox;
    TSharedPtr<class SCheckBox> IncludeLogsCheckBox;

    // Suggestion specific widgets
    TSharedPtr<class SVerticalBox> FeatureRequestFieldsBox;
    TSharedPtr<class SMultiLineEditableTextBox> FeatureDescriptionTextBox;

    // Shared widgets
    TSharedPtr<class SMultiLineEditableTextBox> DescriptionTextBox;
    TSharedPtr<class STextBlock> StatusText;
    TSharedPtr<class STextBlock> FormTitleText;
    TSharedPtr<class SButton> SubmitButton;
    TSharedPtr<class SButton> CancelButton;
    TSharedPtr<class SCheckBox> BugReportCheckBox;
    TSharedPtr<class SCheckBox> FeatureRequestCheckBox;

    UBH_Subsystem* Subsystem;
    FString ScreenshotPath;
    bool bIsSubmitting;
    bool bSubmissionSucceeded;
    EBH_ReportType CurrentReportType;
};
