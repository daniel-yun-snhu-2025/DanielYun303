using UnrealBuildTool;
public class SectorDemo : ModuleRules {
 public SectorDemo(ReadOnlyTargetRules Target) : base(Target) {
  PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;
  PrivateDependencyModuleNames.AddRange(new[]{"Core","CoreUObject","Engine","InputCore","UMG","Slate","SlateCore"});
 }
}
