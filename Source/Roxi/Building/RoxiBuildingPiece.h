// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Building/RoxiBuildPieceDefinition.h"
#include "RoxiBuildingPiece.generated.h"

class APawn;
class UStaticMeshComponent;
class URoxiHealthComponent;

/** Размеры сетки строительства, см. */
namespace RoxiBuildGrid
{
	constexpr float CellSize = 300.f;
	constexpr float WallHeight = 300.f;
	constexpr float FoundationHeight = 100.f;
	constexpr float WallThickness = 20.f;
	constexpr float FloorThickness = 20.f;
}

/** Гнездо для привязки элемента: положение начала элемента и поворот. */
struct FRoxiBuildSocket
{
	FVector Location = FVector::ZeroVector;
	float Yaw = 0.f;
};

/**
 * Элемент постройки (К5): один актор на элемент (Р3), своё здоровье, «спит» по сети.
 *
 * Начало координат элемента:
 *  - фундамент и перекрытие — центр верхней поверхности;
 *  - стена — середина нижнего края.
 * Фундамент ставится на землю; стена — на край фундамента/перекрытия; перекрытие — на верх стены
 * или рядом с другим перекрытием. Устойчивости нет (С10): разрушенный элемент просто исчезает.
 */
UCLASS()
class ROXI_API ARoxiBuildingPiece : public AActor
{
	GENERATED_BODY()

public:
	ARoxiBuildingPiece();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	ERoxiPieceType GetPieceType() const { return PieceType; }
	const URoxiBuildPieceDefinition* GetDefinition() const { return Definition; }
	const FString& GetOwnerId() const { return OwnerId; }
	URoxiHealthComponent* GetHealth() const { return Health; }

	/** Гнёзда этого элемента, к которым можно присоединить элемент типа ForType. */
	void GetSockets(ERoxiPieceType ForType, TArray<FRoxiBuildSocket>& OutSockets) const;

	/** Клиент: подобрать положение для элемента рядом с точкой Aim (Viewer — позиция игрока). false — подходящего места нет. */
	static bool FindPlacement(const UWorld* World, ERoxiPieceType Type, const FVector& Aim, const FVector& Viewer,
		float AimYaw, FRoxiBuildSocket& OutSocket);

	/** Сервер: проверить размещение. */
	static bool ValidatePlacement(const URoxiBuildPieceDefinition* Definition, const APawn* Builder, const FRoxiBuildSocket& Socket, FString& OutReason);

	/** Сервер: создать элемент (без проверок и списания стоимости). */
	static ARoxiBuildingPiece* SpawnPiece(UWorld* World, const URoxiBuildPieceDefinition* Definition, const FRoxiBuildSocket& Socket, const FString& OwnerId);

	/** Половинные размеры и центр объёма элемента (для проверок пересечений). */
	static FVector GetHalfExtent(ERoxiPieceType Type);
	static FVector GetBoundsCenter(ERoxiPieceType Type, const FRoxiBuildSocket& Socket);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnRep_PieceType();

	void ApplyShape();

	/** Есть ли уже элемент такого типа в этой точке. */
	static bool IsSocketOccupied(const UWorld* World, ERoxiPieceType Type, const FRoxiBuildSocket& Socket);
	/** Опора фундамента о землю: центр и углы. */
	static bool HasGroundSupport(const UWorld* World, const FRoxiBuildSocket& Socket);
	/** Совпадает ли положение с гнездом соседнего элемента. */
	static bool MatchesNearbySocket(const UWorld* World, ERoxiPieceType Type, const FRoxiBuildSocket& Socket);
	static void GatherNearbyPieces(const UWorld* World, const FVector& Center, float Radius, TArray<ARoxiBuildingPiece*>& OutPieces);

	UPROPERTY(VisibleAnywhere, Category = "Building")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Building")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Building")
	TObjectPtr<URoxiHealthComponent> Health;

	UPROPERTY(ReplicatedUsing = OnRep_PieceType)
	ERoxiPieceType PieceType = ERoxiPieceType::Foundation;

	/** Только на сервере. */
	UPROPERTY(Transient)
	TObjectPtr<const URoxiBuildPieceDefinition> Definition;

	/** Только на сервере. */
	FString OwnerId;
};
