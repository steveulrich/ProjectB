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
			new string[] { 
				"AIModule",
				"CommonUI",
				"Core", 
				"CoreUObject", 
				"EnhancedInput",
				"Engine", 
				"GameplayAbilities",
				"GameFeatures",
				"GameplayTags",
				"GameplayTasks",
				"InputCore",
				"LyraGame",
				"ModularGameplay",        // For UGameStateComponent
				"ModularGameplayActors",
				"NavigationSystem",
				"Niagara",
				"Projects",
				"UMG"
			}
		);
        
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AssetRegistry",
				"CommonGame",
				"CommonUser",
				"DeveloperSettings",
				"GameplayMessageRuntime",
				"MediaAssets",
				"NetCore",
				"PhysicsCore",
				"Slate",
				"SlateCore"
			}
		);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
