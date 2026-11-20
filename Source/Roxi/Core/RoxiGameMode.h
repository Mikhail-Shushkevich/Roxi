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

	/**
	 * Возрождение по запросу игрока (GDD §5.3).
	 * PointId > 0 — на своём спальнике/кровати (проверяются владелец и перезарядка); 0 — случайная точка.
	 */
	void RequestRespawn(AController* Controller, int32 PointId);

	/** Точки возрождения владельца изменились — обновить его список у подключённого игрока. */
	void NotifyRespawnPointsChanged(const FString& OwnerId);

	/** Мир загружен из сохранения — выдать персонажей игрокам, вошедшим раньше (хост listen-сервера). */
	void OnWorldLoaded();

	/** Спящий персонаж игрока (GDD §5.4) или nullptr. */
	ARoxiCharacter* FindSleeper(const FString& PlayerId) const;

	/** Игроки, которые сейчас мертвы и не в игре (сохраняется в мире). */
	const TSet<FString>& GetDeadPlayers() const { return DeadPlayers; }
	void SetDeadPlayers(const TArray<FString>& InDeadPlayers) { DeadPlayers = TSet<FString>(InDeadPlayers); }

protected:
	virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
		const FString& Options, const FString& Portal) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	/** Не запоминать точку появления: каждый раз случайная свободная (GDD §4.4, Т8). */
	virtual bool ShouldSpawnAtStartSpot(AController* Player) override { return false; }

private:
	TSet<FString> DeadPlayers;
	TArray<TWeakObjectPtr<APlayerController>> PendingStartPlayers;
};
