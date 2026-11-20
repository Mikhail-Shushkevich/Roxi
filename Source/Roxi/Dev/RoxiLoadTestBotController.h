// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "RoxiLoadTestBotController.generated.h"

/**
 * DEV ONLY. Серверный бот нагрузочного стенда: бегает в случайных направлениях
 * внутри круга (Center, Radius) и иногда прыгает. Не использует навигацию.
 * Игровой логики не содержит; существует только для замеров сетевой нагрузки.
 */
UCLASS(NotBlueprintable, Transient)
class ROXI_API ARoxiLoadTestBotController : public AAIController
{
	GENERATED_BODY()

public:
	ARoxiLoadTestBotController();

	void InitWander(const FVector& InCenter, float InRadius);

	virtual void Tick(float DeltaSeconds) override;

private:
	FVector Center = FVector::ZeroVector;
	float Radius = 1500.f;
	FVector MoveDir = FVector::ForwardVector;
	float TimeToNextDecision = 0.f;
};
