// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RoxiItemDefinition.generated.h"

class UStaticMesh;
class UTexture2D;

/**
 * Описание типа предмета (Т1). Один ассет на тип, лежат в /Game/Roxi/Items.
 * Статичные данные: не реплицируются как содержимое, по сети передаётся только ссылка.
 */
UCLASS(BlueprintType, Const)
class ROXI_API URoxiItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType ItemAssetType;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/** Загрузка по имени ассета, например "DEV_TestItemA". Синхронная — для серверной логики и отладки. */
	static URoxiItemDefinition* LoadByName(FName ItemName);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (MultiLine = true))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Меш предмета, лежащего на земле. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TSoftObjectPtr<UStaticMesh> WorldMesh;

	/** Максимальный размер стопки (В2). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (ClampMin = 1))
	int32 MaxStackSize = 1;
};
