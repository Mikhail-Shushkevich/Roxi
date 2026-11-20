// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Roxi : ModuleRules
{
	public Roxi(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Includes relative to the module root: "Core/RoxiGameMode.h", "Character/RoxiCharacter.h".
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });

		// NetCore: Push Model (MARK_PROPERTY_DIRTY) for replicated properties.
		// AIModule: AAIController for load-test bots (Dev/). CoreOnline: persistent player IDs (FUniqueNetIdWrapper).
		PrivateDependencyModuleNames.AddRange(new string[] { "NetCore", "AIModule", "CoreOnline" });
	}
}
