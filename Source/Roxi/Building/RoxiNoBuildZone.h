// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RoxiNoBuildZone.generated.h"

class USphereComponent;

/**
 * Зона запрета строительства (GDD §6.1, К6). Дизайнер ставит на карту вокруг POI;
 * радиус задаётся на каждом экземпляре (открытый вопрос №6, по умолчанию 100 м — С14).
 * Не реплицируется: зоны — часть уровня, проверка идёт на сервере.
 */
UCLASS()
class ROXI_API ARoxiNoBuildZone : public AActor
{
	GENERATED_BODY()

public:
	ARoxiNoBuildZone();

	float GetRadius() const;
	void SetRadius(float InRadius);

	/** Находится ли точка внутри любой зоны запрета. */
	static bool IsInAnyZone(const UWorld* World, const FVector& Location);

protected:
	UPROPERTY(VisibleAnywhere, Category = "NoBuild")
	TObjectPtr<USphereComponent> Zone;
};
