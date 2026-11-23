using UnrealBuildTool;
public class SectorFieldUI : ModuleRules {
 public SectorFieldUI(ReadOnlyTargetRules Target) : base(Target) {
 PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;
 PrivateDependencyModuleNames.AddRange(new[]{"Core","CoreUObject","Engine","Slate","SlateCore","UMG"});
 }
}
