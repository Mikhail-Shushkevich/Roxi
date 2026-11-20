// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/RoxiItemDefinition.h"
#include "Engine/AssetManager.h"

const FPrimaryAssetType URoxiItemDefinition::ItemAssetType(TEXT("RoxiItem"));

FPrimaryAssetId URoxiItemDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(ItemAssetType, GetFName());
}

URoxiItemDefinition* URoxiItemDefinition::LoadByName(FName ItemName)
{
	const FSoftObjectPath Path = UAssetManager::Get().GetPrimaryAssetPath(FPrimaryAssetId(ItemAssetType, ItemName));
	return Path.IsValid() ? Cast<URoxiItemDefinition>(Path.TryLoad()) : nullptr;
}
