// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RoxiPlayerController.generated.h"

class URoxiBuildPieceDefinition;
class URoxiInventoryComponent;
class URoxiItemInteractionComponent;

/** Точка возрождения игрока, как её видит клиент (только свои — К4). */
USTRUCT()
struct ROXI_API FRoxiRespawnPointInfo
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Id = 0;

	UPROPERTY()
	FText DisplayName;

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	/** Время мира сервера, когда точка снова доступна. */
	UPROPERTY()
	double AvailableAt = 0.0;
};

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

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	URoxiItemInteractionComponent* GetItemInteraction() const { return ItemInteraction; }

	/** Постоянный ID игрока (UniqueNetId онлайн-подсистемы) — владелец спальников и построек. */
	FString GetPersistentPlayerId() const;

	/** Сервер: пересобрать список точек возрождения игрока. */
	void RefreshRespawnPoints();
	const TArray<FRoxiRespawnPointInfo>& GetRespawnPoints() const { return RespawnPoints; }

	/** Сообщение от сервера (причина отказа и т.п.). */
	UFUNCTION(Client, Reliable)
	void ClientShowMessage(const FString& Message);
	const FString& GetLastMessage() const { return LastMessage; }
	double GetLastMessageTime() const { return LastMessageTime; }

	/** Запрос возрождения после смерти (GDD §5.3). PointId 0 — случайная точка. */
	UFUNCTION(Server, Reliable)
	void ServerRequestRespawn(int32 PointId);

	// --- Отладочные консольные команды (Т9, К8). Серверная часть dev-команд не работает в Shipping. ---
	// Контейнеры: inv — инвентарь, belt — пояс, loot — открытый контейнер.

	/** RoxiGive <ItemName> <Quantity> — выдать предмет себе. */
	UFUNCTION(Exec)
	void RoxiGive(const FString& ItemName, int32 Quantity);

	/** RoxiInv — вывести в лог состояние игрока (как его видит клиент). */
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

	/** RoxiRespawn <PointId> — возродиться (0 — случайная точка). Временная замена экрана возрождения. */
	UFUNCTION(Exec)
	void RoxiRespawn(int32 PointId);

	/** RoxiDamage <Amount> — нанести урон себе. */
	UFUNCTION(Exec)
	void RoxiDamage(float Amount);

	/** RoxiHit <Amount> — нанести урон ближайшему объекту со здоровьем (спальник, постройка). */
	UFUNCTION(Exec)
	void RoxiHit(float Amount);

	/** RoxiCraft <RecipeName> <Count> — поставить в очередь крафта. */
	UFUNCTION(Exec)
	void RoxiCraft(const FString& RecipeName, int32 Count);

	/** RoxiCraftCancel <QueueIndex> — отменить позицию очереди. */
	UFUNCTION(Exec)
	void RoxiCraftCancel(int32 QueueIndex);

	/** RoxiPlace <inv|belt> <Slot> — поставить размещаемый предмет перед собой. */
	UFUNCTION(Exec)
	void RoxiPlace(const FString& From, int32 Slot);

	/** RoxiBuild <PieceName> — построить элемент перед собой (с привязкой к соседним). */
	UFUNCTION(Exec)
	void RoxiBuild(const FString& PieceName);

	/** RoxiBuildAt <PieceName> <dX> <dY> <dZ> <Yaw> — отправить серверу произвольное положение относительно себя, без привязки (проверка защиты). */
	UFUNCTION(Exec)
	void RoxiBuildAt(const FString& PieceName, float X, float Y, float Z, float Yaw);

	/** Построить элемент (К5). Сервер проверяет место и списывает стоимость. */
	UFUNCTION(Server, Reliable)
	void ServerBuild(const URoxiBuildPieceDefinition* Definition, FVector Location, float Yaw);

protected:
	UFUNCTION(Server, Reliable)
	void ServerDevGive(FName ItemName, int32 Quantity);

	UFUNCTION(Server, Reliable)
	void ServerDevDamage(float Amount);

	UFUNCTION(Server, Reliable)
	void ServerDevHit(float Amount);

	URoxiInventoryComponent* ResolveContainer(const FString& Name) const;

	UPROPERTY(VisibleAnywhere, Category = "Items")
	TObjectPtr<URoxiItemInteractionComponent> ItemInteraction;

	UPROPERTY(Replicated)
	TArray<FRoxiRespawnPointInfo> RespawnPoints;

private:
	FString LastMessage;
	double LastMessageTime = 0.0;
};
