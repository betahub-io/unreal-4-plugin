// Copyright Upsoft sp. z o. o. All Rights Reserved.

#include "BetaHubBugReporterModule.h"
#include "BH_Log.h"

#define LOCTEXT_NAMESPACE "FBetaHubBugReporterModule"

void FBetaHubBugReporterModule::StartupModule()
{
	UE_LOG(LogBetaHub, Log, TEXT("BetaHub Bug Reporter module started"));
}

void FBetaHubBugReporterModule::ShutdownModule()
{
	UE_LOG(LogBetaHub, Log, TEXT("BetaHub Bug Reporter module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBetaHubBugReporterModule, BetaHubBugReporter)
