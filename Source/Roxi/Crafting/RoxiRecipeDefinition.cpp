// Copyright Epic Games, Inc. All Rights Reserved.

#include "Crafting/RoxiRecipeDefinition.h"
#include "Engine/AssetManager.h"

const FPrimaryAssetType URoxiRecipeDefinition::RecipeAssetType(TEXT("RoxiRecipe"));

FPrimaryAssetId URoxiRecipeDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(RecipeAssetType, GetFName());
}

URoxiRecipeDefinition* URoxiRecipeDefinition::LoadByName(FName RecipeName)
{
	const FSoftObjectPath Path = UAssetManager::Get().GetPrimaryAssetPath(FPrimaryAssetId(RecipeAssetType, RecipeName));
	return Path.IsValid() ? Cast<URoxiRecipeDefinition>(Path.TryLoad()) : nullptr;
}
