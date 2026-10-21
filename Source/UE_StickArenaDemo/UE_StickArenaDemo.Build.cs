// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UE_StickArenaDemo : ModuleRules
{
	public UE_StickArenaDemo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "GameplayTags", "UMG", "Niagara" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Sockets", "Slate", "SlateCore" });
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("RenderCore"); // Flush the editor minimap bake before reading pixels.
		}

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
