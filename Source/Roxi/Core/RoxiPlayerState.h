// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "RoxiPlayerState.generated.h"

/**
 * Данные игрока, переживающие смерть персонажа и видимые другим игрокам.
 */
UCLASS()
class ROXI_API ARoxiPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	/**
	 * Только на сервере: постоянный ID игрока — владелец спальников, построек, спящего персонажа и профиля.
	 * Берётся из онлайн-подсистемы. В не-Shipping сборках его можно задать параметром подключения
	 * ?RoxiDevId=<имя> — для тестов: OnlineSubsystemNull в редакторе выдаёт случайный ID при каждом запуске.
	 */
	const FString& GetPersistentId() const { return PersistentId; }
	void SetPersistentId(const FString& InId) { PersistentId = InId; }

private:
	FString PersistentId;
};
