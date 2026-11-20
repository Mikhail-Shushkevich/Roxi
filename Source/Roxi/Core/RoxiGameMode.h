// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RoxiGameMode.generated.h"

/**
 * Серверный GameMode ROXI. Существует только на сервере.
 * Классы GameState / PlayerState / PlayerController задаются здесь,
 * класс пешки — в Blueprint-наследнике (BP_RoxiGameMode), чтобы не держать путей к ассетам в C++.
 */
UCLASS(minimalapi)
class ARoxiGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARoxiGameMode();

protected:
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
};
