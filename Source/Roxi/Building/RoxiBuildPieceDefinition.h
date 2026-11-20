// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Crafting/RoxiRecipeDefinition.h"
#include "Engine/DataAsset.h"
#include "RoxiBuildPieceDefinition.generated.h"

/** Тип элемента постройки. Набор по С8: фундамент, стена, перекрытие. */
UENUM(BlueprintType)
enum class ERoxiPieceType : uint8
{
	Foundation,
	Wall,
	Floor,
};

/** Описание элемента постройки (К5). Ассеты лежат в /Game/Roxi/Building. */
UCLASS(BlueprintType, Const)
class ROXI_API URoxiBuildPieceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType PieceAssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	static URoxiBuildPieceDefinition* LoadByName(FName PieceName);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building")
	ERoxiPieceType PieceType = ERoxiPieceType::Foundation;

	/** Стоимость постройки (тестовая, из DEV-предметов — К9). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building")
	TArray<FRoxiItemAmount> Cost;

	/** Один уровень материала, без улучшений (С9). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Building", meta = (ClampMin = 1))
	float MaxHealth = 500.f;
};
