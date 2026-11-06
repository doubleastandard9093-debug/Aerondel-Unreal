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
		// Keep TargetRules minimal: engine-default unity build is fine. Do not flip experimental
		// UBT flags here — Aaron's laptop is the compile authority (see root Build.bat).
	}
}
