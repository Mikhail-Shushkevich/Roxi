// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RoxiRecipeDefinition.generated.h"

class URoxiItemDefinition;

/** Предмет + количество — ингредиент рецепта или стоимость постройки. */
USTRUCT(BlueprintType)
struct ROXI_API FRoxiItemAmount
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<URoxiItemDefinition> Item = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = 1))
	int32 Quantity = 1;
};

/** Рецепт крафта (К1). Ассеты лежат в /Game/Roxi/Recipes. */
UCLASS(BlueprintType, Const)
class ROXI_API URoxiRecipeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType RecipeAssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	static URoxiRecipeDefinition* LoadByName(FName RecipeName);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe")
	TArray<FRoxiItemAmount> Ingredients;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe")
	TObjectPtr<URoxiItemDefinition> OutputItem = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe", meta = (ClampMin = 1))
	int32 OutputQuantity = 1;

	/** Время крафта одной единицы рецепта, секунды (С2). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe", meta = (ClampMin = 0))
	float CraftTime = 5.f;

	/** Требуемая станция крафта. Зарезервировано: верстаки не утверждены (С4). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Recipe")
	FName RequiredStation;
};
