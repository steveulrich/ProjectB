// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class InteractiveObject : ModuleRules
{
	public InteractiveObject(ReadOnlyTargetRules Target) : base(Target)
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
				"CoreUObject",
				"Engine",
				"GameplayAbilities",  // Add this
				"GameplayTags",       // Add this
				"GameplayTasks",      // Add this
				"LyraGame"           // Add this
			}
		);

		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"InputCore",
				"EnhancedInput",
				"ModularGameplay"
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
