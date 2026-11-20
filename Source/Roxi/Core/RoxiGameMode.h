// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RoxiGameMode.generated.h"

class ARoxiCharacter;

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

	/** Смерть персонажа (GDD §5.2): все предметы — в контейнер на месте смерти, персонаж удаляется. */
	void HandleCharacterDeath(ARoxiCharacter* Victim, AController* Killer);

	/** Возрождение по запросу игрока (GDD §5.3). Пока только случайная точка; спальники и кровати — Этап 3. */
	void RequestRespawn(AController* Controller);

protected:
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/** Не запоминать точку появления: каждый раз случайная свободная (GDD §4.4, Т8). */
	virtual bool ShouldSpawnAtStartSpot(AController* Player) override { return false; }
};
