// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/RoxiGameState.h"
#include "Building/RoxiBuildPieceDefinition.h"
#include "Crafting/RoxiRecipeDefinition.h"
#include "Items/RoxiItemDefinition.h"
#include "Roxi.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

void ARoxiGameState::BeginPlay()
{
	Super::BeginPlay();

	// Описаний немного и они маленькие — держим загруженными всё время матча.
	const FPrimaryAssetType Types[] = {
		URoxiItemDefinition::ItemAssetType,
		URoxiRecipeDefinition::RecipeAssetType,
		URoxiBuildPieceDefinition::PieceAssetType,
	};
	UAssetManager& AssetManager = UAssetManager::Get();
	for (const FPrimaryAssetType& Type : Types)
	{
		TArray<FPrimaryAssetId> Ids;
		AssetManager.GetPrimaryAssetIdList(Type, Ids);
		if (TSharedPtr<FStreamableHandle> Handle = AssetManager.LoadPrimaryAssets(Ids))
		{
			Handle->WaitUntilComplete();
			DefinitionHandles.Add(Handle);
		}
		UE_LOG(LogRoxi, Log, TEXT("Preloaded %d %s definitions"), Ids.Num(), *Type.ToString());
	}
}

void ARoxiGameState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (const TSharedPtr<FStreamableHandle>& Handle : DefinitionHandles)
	{
		if (Handle.IsValid())
		{
			Handle->ReleaseHandle();
		}
	}
	DefinitionHandles.Reset();
	Super::EndPlay(EndPlayReason);
}
