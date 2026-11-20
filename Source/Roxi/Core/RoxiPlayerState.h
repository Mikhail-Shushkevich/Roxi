// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "RoxiPlayerState.generated.h"

/**
 * Данные игрока, переживающие смерть персонажа и видимые другим игрокам.
 * Пока пустое — поля появятся вместе с системами из GDD.
 */
UCLASS()
class ROXI_API ARoxiPlayerState : public APlayerState
{
	GENERATED_BODY()
};
