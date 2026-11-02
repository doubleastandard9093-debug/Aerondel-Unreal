using UnrealBuildTool;
using System.Collections.Generic;

public class AerondelEditorTarget : TargetRules
{
	public AerondelEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Aerondel");
	}
}
