// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RoxiDeployable.generated.h"

class APawn;
class UStaticMeshComponent;
class URoxiHealthComponent;

/**
 * Размещаемый объект (К3): ставится из предмета, имеет здоровье и владельца, «спит» по сети.
 * Размещение всегда проверяет сервер (ValidatePlacement).
 */
UCLASS(Abstract)
class ROXI_API ARoxiDeployable : public AActor
{
	GENERATED_BODY()

public:
	ARoxiDeployable();

	/**
	 * Сервер: проверить размещение класса DeployableClass игроком Placer в точке Desired.
	 * При успехе OutTransform — итоговое положение (на поверхности), иначе OutReason — причина.
	 */
	static bool ValidatePlacement(TSubclassOf<ARoxiDeployable> DeployableClass, const APawn* Placer, const FString& OwnerId,
		const FTransform& Desired, FTransform& OutTransform, FString& OutReason);

	/** Сервер: задать владельца (постоянный ID игрока). */
	void InitOwner(const FString& InOwnerId) { OwnerId = InOwnerId; }
	const FString& GetOwnerId() const { return OwnerId; }

protected:
	virtual void BeginPlay() override;

	/** Дополнительные правила конкретного типа (например, лимит на игрока). Вызывается на CDO. */
	virtual bool CanBePlacedBy(const UWorld* World, const FString& InOwnerId, FString& OutReason) const { return true; }

	UFUNCTION()
	void HandleHealthDepleted();

	UPROPERTY(VisibleAnywhere, Category = "Deployable")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Deployable")
	TObjectPtr<URoxiHealthComponent> Health;

	/** Половинные размеры объекта для проверки пересечений, см. */
	UPROPERTY(EditDefaultsOnly, Category = "Deployable")
	FVector FootprintExtent = FVector(50.f, 50.f, 20.f);

	/** Максимальный наклон поверхности, градусы. */
	UPROPERTY(EditDefaultsOnly, Category = "Deployable")
	float MaxSlopeDegrees = 35.f;

	/** Только на сервере. */
	FString OwnerId;
};
