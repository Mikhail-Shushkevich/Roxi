// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RoxiCraftingComponent.generated.h"

class ARoxiCharacter;
class URoxiItemDefinition;
class URoxiRecipeDefinition;
struct FRoxiItemAmount;

/** Одна позиция очереди крафта. Ресурсы уже списаны. */
USTRUCT()
struct ROXI_API FRoxiCraftJob
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<const URoxiRecipeDefinition> Recipe = nullptr;
};

/**
 * Очередь крафта персонажа (К2, как в Rust): ресурсы списываются при постановке в очередь,
 * предмет выдаётся по истечении времени, отмена возвращает ресурсы.
 * Логика — на сервере; очередь реплицируется только владельцу.
 */
UCLASS(ClassGroup = (Roxi))
class ROXI_API URoxiCraftingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URoxiCraftingComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	static constexpr int32 MaxQueueLength = 10;

	const TArray<FRoxiCraftJob>& GetQueue() const { return Queue; }
	/** Время завершения первой позиции во времени мира сервера (см. AGameStateBase::GetServerWorldTimeSeconds). */
	double GetCurrentJobEndTime() const { return CurrentJobEndTime; }

	/** Поставить в очередь Count раз (столько, на сколько хватит ресурсов и места в очереди). */
	UFUNCTION(Server, Reliable)
	void ServerCraft(const URoxiRecipeDefinition* Recipe, int32 Count);

	/** Отменить позицию очереди с возвратом ресурсов. */
	UFUNCTION(Server, Reliable)
	void ServerCancel(int32 QueueIndex);

	/** Сервер: отменить всё с возвратом ресурсов (например, перед смертью). */
	void CancelAll();

	/** Сервер: списать стоимость из инвентаря и пояса персонажа — всё или ничего. */
	static bool TryConsume(ARoxiCharacter* Character, const TArray<FRoxiItemAmount>& Cost);

private:
	ARoxiCharacter* GetCharacter() const;
	bool TryConsumeIngredients(const URoxiRecipeDefinition* Recipe);
	/** Положить предметы игроку: инвентарь, затем пояс, остаток — на землю. */
	void GiveToOwner(URoxiItemDefinition* Item, int32 Quantity);
	void Refund(const URoxiRecipeDefinition* Recipe);
	void StartNextJob();
	void MarkQueueDirty();

	UPROPERTY(Replicated)
	TArray<FRoxiCraftJob> Queue;

	UPROPERTY(Replicated)
	double CurrentJobEndTime = 0.0;
};
