using UnrealBuildTool;
public class SectorThrow : ModuleRules {
 public SectorThrow(ReadOnlyTargetRules Target) : base(Target) {
  PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;
  PrivateDependencyModuleNames.AddRange(new[]{"Core","CoreUObject","Engine","EnhancedInput"});
 }
}
