// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "RoxiGameState.generated.h"

/**
 * Состояние матча/сервера, реплицируемое всем клиентам.
 * Пока пустое — сюда будут добавляться общие для сервера данные по мере появления систем.
 */
UCLASS()
class ROXI_API ARoxiGameState : public AGameStateBase
{
	GENERATED_BODY()
};
