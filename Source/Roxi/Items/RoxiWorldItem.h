// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Items/RoxiItemStack.h"
#include "RoxiWorldItem.generated.h"

class UStaticMeshComponent;

/**
 * Предмет, лежащий на земле (Т6): одна стопка. Неподвижен и «спит» по сети (Dormancy) —
 * обновления уходят клиентам только при изменении количества.
 */
UCLASS()
class ROXI_API ARoxiWorldItem : public AActor
{
	GENERATED_BODY()

public:
	ARoxiWorldItem();

	/** Сервер: создать предмет на земле под точкой Location. */
	static ARoxiWorldItem* SpawnItem(UWorld* World, const FRoxiItemStack& Stack, const FVector& Location);

	const FRoxiItemStack& GetStack() const { return Stack; }

	/** Сервер: изменить количество; при 0 предмет удаляется. */
	void SetQuantity(int32 NewQuantity);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_Stack();

	void UpdateVisuals();

	UPROPERTY(VisibleAnywhere, Category = "Item")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(ReplicatedUsing = OnRep_Stack)
	FRoxiItemStack Stack;
};
