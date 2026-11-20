// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/RoxiInventoryList.h"
#include "RoxiItemInteractionComponent.generated.h"

class APawn;
class ARoxiWorldItem;
class URoxiInventoryComponent;

/**
 * Действия игрока с предметами (Т5) и просмотр контейнеров в мире (Т4). Живёт на PlayerController.
 *
 * Клиент шлёт намерения (Server RPC), сервер проверяет доступ, дистанцию и выполняет.
 * Содержимое открытого контейнера сервер зеркалит в ViewedSlots — это свойство
 * реплицируется только владельцу контроллера, т.е. только открывшему игроку.
 */
UCLASS(ClassGroup = (Roxi))
class ROXI_API URoxiItemInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URoxiItemInteractionComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Дистанция взаимодействия (см. cvar roxi.InteractRange). */
	static float GetInteractRange();

	// --- Клиент ---

	/** Ближайший предмет на земле или контейнер в радиусе взаимодействия. */
	AActor* FindNearestInteractable() const;
	/** Поднять ближайший предмет / открыть или закрыть ближайший контейнер. */
	void InteractNearest();

	/** Инвентарь (0) или пояс (1) управляемого персонажа. */
	URoxiInventoryComponent* GetPawnInventory() const;
	URoxiInventoryComponent* GetPawnBelt() const;

	/** Открытый контейнер и его содержимое (для интерфейса). */
	URoxiInventoryComponent* GetViewedContainer() const { return ViewedContainer; }
	const FRoxiInventoryList& GetViewedSlots() const { return ViewedSlots; }
	/** Второй открытый контейнер: пояс обыскиваемого спящего персонажа (GDD §5.4). */
	URoxiInventoryComponent* GetViewedContainer2() const { return ViewedContainer2; }
	const FRoxiInventoryList& GetViewedSlots2() const { return ViewedSlots2; }
	/** Открыт ли сейчас контейнер, принадлежащий этому актору. */
	bool IsViewing(const AActor* Target) const;

	// --- Намерения клиента ---

	UFUNCTION(Server, Reliable)
	void ServerMoveItem(URoxiInventoryComponent* From, int32 FromSlot, URoxiInventoryComponent* To, int32 ToSlot, int32 Quantity);

	UFUNCTION(Server, Reliable)
	void ServerDropItem(URoxiInventoryComponent* From, int32 Slot, int32 Quantity);

	UFUNCTION(Server, Reliable)
	void ServerPickUp(ARoxiWorldItem* WorldItem);

	UFUNCTION(Server, Reliable)
	void ServerOpenContainer(AActor* ContainerActor);

	UFUNCTION(Server, Reliable)
	void ServerCloseContainer();

	/** Разместить предмет из своего инвентаря/пояса как объект в мире (К3). Сервер проверяет место. */
	UFUNCTION(Server, Reliable)
	void ServerPlaceDeployable(URoxiInventoryComponent* From, int32 Slot, FVector Location, float Yaw);

	/** Сервер: закрыть открытый контейнер. */
	void CloseContainer();

private:
	APawn* GetControlledPawn() const;
	bool IsInRange(const AActor* Target) const;
	/** Сервер: может ли игрок сейчас работать с этим контейнером. */
	bool CanAccess(const URoxiInventoryComponent* Inventory) const;
	void HandleViewedContainerChanged(URoxiInventoryComponent* Inventory);

	UPROPERTY(Replicated)
	TObjectPtr<URoxiInventoryComponent> ViewedContainer;

	UPROPERTY(Replicated)
	FRoxiInventoryList ViewedSlots;

	UPROPERTY(Replicated)
	TObjectPtr<URoxiInventoryComponent> ViewedContainer2;

	UPROPERTY(Replicated)
	FRoxiInventoryList ViewedSlots2;

	FDelegateHandle ViewedChangedHandle;
	FDelegateHandle ViewedChangedHandle2;
	float RangeCheckTimer = 0.f;
};
