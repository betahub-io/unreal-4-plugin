// Copyright Upsoft sp. z o. o. All Rights Reserved.

using UnrealBuildTool;

public class BetaHubBugReporter : ModuleRules
{
	public BetaHubBugReporter(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
			}
			);

		PrivateIncludePaths.AddRange(
			new string[] {
			}
			);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"InputCore",
				"HTTP",
				"Json",
				"JsonUtilities",
				"ImageWrapper",
				"RenderCore",
				"RHI",
				"DeveloperSettings",
			}
			);

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
			);
	}
}
