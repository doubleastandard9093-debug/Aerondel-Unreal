using UnrealBuildTool;

public class Gielinor : ModuleRules
{
	public Gielinor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Folder-relative includes: #include "Data/ItemDef.h"
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"UMG", "Slate", "SlateCore", "AIModule", "NavigationSystem",
			"HeadMountedDisplay", "XRBase", "GameplayTags", "DeveloperSettings",
			"Json", "JsonUtilities"
		});
		// No networking modules: multiplayer is paused.
	}
}
