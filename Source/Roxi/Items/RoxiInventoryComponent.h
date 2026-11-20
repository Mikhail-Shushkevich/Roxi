// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/RoxiInventoryList.h"
#include "RoxiInventoryComponent.generated.h"

class URoxiInventoryComponent;
class URoxiItemDefinition;

DECLARE_MULTICAST_DELEGATE_OneParam(FRoxiInventoryChanged, URoxiInventoryComponent* /*Inventory*/);

/**
 * Контейнер предметов (Т3): инвентарь и пояс игрока, контейнер с добычей, в будущем — ящики.
 *
 * Сеть (Т4): содержимое реплицируется ТОЛЬКО владельцу актора (COND_OwnerOnly).
 * У контейнеров в мире владельца нет — их содержимое получают только открывшие игроки
 * через URoxiItemInteractionComponent.
 *
 * Все изменяющие методы — только на сервере.
 */
UCLASS(ClassGroup = (Roxi), meta = (BlueprintSpawnableComponent))
class ROXI_API URoxiInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URoxiInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnRegister() override;
	virtual void BeginPlay() override;

	int32 GetNumSlots() const { return Slots.Num(); }
	bool IsValidSlot(int32 Slot) const { return Slots.IsValidSlot(Slot); }
	FRoxiItemStack GetStack(int32 Slot) const { return Slots.GetStack(Slot); }
	const FRoxiInventoryList& GetList() const { return Slots; }
	bool IsEmpty() const;
	FString Describe() const { return Slots.Describe(); }

	/** Число слотов, создаваемых в BeginPlay. Для конструкторов владельцев. */
	void SetInitialNumSlots(int32 InNumSlots) { NumSlots = FMath::Max(1, InNumSlots); }

	// --- Сервер ---

	/** Задать число слотов. Допустимо, только пока контейнер пуст. */
	bool SetNumSlots(int32 InNumSlots);

	/** Добавить предметы: сначала в неполные стопки того же типа, затем в пустые слоты. Возвращает остаток, который не поместился. */
	int32 AddItem(URoxiItemDefinition* Item, int32 Quantity);

	/** Забрать Quantity (<= 0 — всю стопку) из слота. */
	FRoxiItemStack RemoveFromSlot(int32 Slot, int32 Quantity);

	/** Забрать всё содержимое (например, при смерти). */
	TArray<FRoxiItemStack> RemoveAll();

	/**
	 * Переместить Quantity (<= 0 — всю стопку) между слотами одного или двух контейнеров:
	 * в пустой слот — перенос, в слот с тем же предметом — объединение до максимума стопки,
	 * в слот с другим предметом — обмен (только при переносе стопки целиком).
	 */
	static bool MoveItem(URoxiInventoryComponent* From, int32 FromSlot, URoxiInventoryComponent* To, int32 ToSlot, int32 Quantity);

	/** Изменилось содержимое (на сервере — после операции, на клиенте — после репликации). */
	FRoxiInventoryChanged OnChanged;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Inventory", meta = (ClampMin = 1))
	int32 NumSlots = 24;

	UPROPERTY(Replicated)
	FRoxiInventoryList Slots;

private:
	bool HasAuthority() const;
	void BroadcastChanged();
	void HandleReplicated();
};
