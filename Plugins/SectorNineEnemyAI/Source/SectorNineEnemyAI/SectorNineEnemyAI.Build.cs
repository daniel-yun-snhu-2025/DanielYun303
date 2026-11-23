using UnrealBuildTool;
public class SectorNineEnemyAI : ModuleRules {
 public SectorNineEnemyAI(ReadOnlyTargetRules Target) : base(Target) {
  PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;
  PublicDependencyModuleNames.AddRange(new[]{"Core","CoreUObject","Engine"});
 }
}
