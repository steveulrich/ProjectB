// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BreakawayCoreRuntime : ModuleRules
{
	public BreakawayCoreRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"LyraGame",
				"ModularGameplay",   //REQUIRED BY LYRA TO EXTEND LyraGameplayAbility_RangedComponent
				"CommonGame",		 //REQUIRED BY LYRA TO EXTEND LyraGameplayAbility_RangedComponent
				"GameplayAbilities", //REQUIRED BY LYRA TO EXTEND LyraGameplayAbility_RangedComponent
				"GameplayTags",		 //REQUIRED BY LYRA TO EXTEND LyraGameplayAbility_RangedComponent
				"AIModule",			 //REQUIRED BY LYRA TO EXTEND LyraGameplayAbility_RangedComponent
				"GameplayTasks",	 //REQUIRED BY LYRA TO EXTEND LyraGameplayAbility_RangedComponent

				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"LyraGame",
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
