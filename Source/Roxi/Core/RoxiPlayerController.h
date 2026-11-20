// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RoxiPlayerController.generated.h"

/**
 * Контроллер игрока ROXI. Существует на сервере и у владеющего клиента.
 * Точка для серверных RPC «намерений» игрока (модель авторитетности Р1).
 */
UCLASS()
class ROXI_API ARoxiPlayerController : public APlayerController
{
	GENERATED_BODY()
};
