// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RoxiLootContainer.generated.h"

class UStaticMeshComponent;
class URoxiInventoryComponent;

/**
 * Контейнер с добычей на месте смерти игрока (GDD §5.2, Т7).
 * Открыть может любой игрок (В5), время жизни не ограничено (В9 — открытый вопрос №4).
 * Содержимое не реплицируется всем: его получают только открывшие (Т4).
 */
UCLASS()
class ROXI_API ARoxiLootContainer : public AActor
{
	GENERATED_BODY()

public:
	ARoxiLootContainer();

	URoxiInventoryComponent* GetInventory() const { return Inventory; }

	/** Чей это контейнер — для подсказки в интерфейсе. */
	const FString& GetOwnerLabel() const { return OwnerLabel; }
	void SetOwnerLabel(const FString& InLabel);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Loot")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Loot")
	TObjectPtr<URoxiInventoryComponent> Inventory;

	UPROPERTY(Replicated)
	FString OwnerLabel;
};
