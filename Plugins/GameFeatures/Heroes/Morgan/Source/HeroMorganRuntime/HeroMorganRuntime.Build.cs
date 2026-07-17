using UnrealBuildTool;

public class HeroMorganRuntime : ModuleRules
{
	public HeroMorganRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"BreakawayCoreRuntime",
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayAbilities",
				"GameplayTags",
				"GameplayTasks",
				"LyraGame",
				"ModularGameplay",
				"NetCore"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"PhysicsCore"
			}
		);
	}
}
