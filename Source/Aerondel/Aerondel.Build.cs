using UnrealBuildTool;

public class Aerondel : ModuleRules
{
	public Aerondel(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Folder-relative includes: #include "Data/ItemDef.h"
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"UMG", "Slate", "SlateCore", "AIModule", "NavigationSystem",
			// HMD/XRBase stay: XR/*.cpp and PlatformModeSubsystem call HMD APIs even when the
			// OpenXR *plugin* is Android-only (Windows flat PC). Do not delete VR sources.
			"HeadMountedDisplay", "XRBase", "GameplayTags", "DeveloperSettings",
			"Json", "JsonUtilities"
		});
		// No networking modules: multiplayer is paused.
	}
}
