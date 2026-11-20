// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RoxiPlayerController.generated.h"

class URoxiInventoryComponent;
class URoxiItemInteractionComponent;

/**
 * Контроллер игрока ROXI. Существует на сервере и у владеющего клиента.
 * Точка для серверных RPC «намерений» игрока (модель авторитетности Р1).
 */
UCLASS()
class ROXI_API ARoxiPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARoxiPlayerController();

	URoxiItemInteractionComponent* GetItemInteraction() const { return ItemInteraction; }

	/** Запрос возрождения после смерти (GDD §5.3). Пока только случайная точка. */
	UFUNCTION(Server, Reliable)
	void ServerRequestRespawn();

	/** RoxiRespawn — возродиться (временная замена экрана возрождения). */
	UFUNCTION(Exec)
	void RoxiRespawn();

	/** RoxiDamage <Amount> — нанести урон себе (отладка). */
	UFUNCTION(Exec)
	void RoxiDamage(float Amount);

	// --- Отладочные консольные команды (Т9). Серверная часть не работает в Shipping. ---
	// Контейнеры: inv — инвентарь, belt — пояс, loot — открытый контейнер.

	/** RoxiGive <ItemName> <Quantity> — выдать предмет себе. */
	UFUNCTION(Exec)
	void RoxiGive(const FString& ItemName, int32 Quantity);

	/** RoxiInv — вывести в лог содержимое своих контейнеров (как их видит клиент). */
	UFUNCTION(Exec)
	void RoxiInv();

	/** RoxiMove <From> <FromSlot> <To> <ToSlot> <Quantity> — переместить (Quantity 0 — всю стопку). */
	UFUNCTION(Exec)
	void RoxiMove(const FString& From, int32 FromSlot, const FString& To, int32 ToSlot, int32 Quantity);

	/** RoxiDrop <From> <Slot> <Quantity> — выбросить на землю (Quantity 0 — всю стопку). */
	UFUNCTION(Exec)
	void RoxiDrop(const FString& From, int32 Slot, int32 Quantity);

	/** RoxiInteract — то же, что клавиша взаимодействия. */
	UFUNCTION(Exec)
	void RoxiInteract();

protected:
	UFUNCTION(Server, Reliable)
	void ServerDevGive(FName ItemName, int32 Quantity);

	UFUNCTION(Server, Reliable)
	void ServerDevDamage(float Amount);

	URoxiInventoryComponent* ResolveContainer(const FString& Name) const;

	UPROPERTY(VisibleAnywhere, Category = "Items")
	TObjectPtr<URoxiItemInteractionComponent> ItemInteraction;
};
