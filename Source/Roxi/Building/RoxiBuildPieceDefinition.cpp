// Copyright Epic Games, Inc. All Rights Reserved.

#include "Building/RoxiBuildPieceDefinition.h"
#include "Engine/AssetManager.h"

const FPrimaryAssetType URoxiBuildPieceDefinition::PieceAssetType(TEXT("RoxiBuildPiece"));

FPrimaryAssetId URoxiBuildPieceDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(PieceAssetType, GetFName());
}

URoxiBuildPieceDefinition* URoxiBuildPieceDefinition::LoadByName(FName PieceName)
{
	const FSoftObjectPath Path = UAssetManager::Get().GetPrimaryAssetPath(FPrimaryAssetId(PieceAssetType, PieceName));
	return Path.IsValid() ? Cast<URoxiBuildPieceDefinition>(Path.TryLoad()) : nullptr;
}
