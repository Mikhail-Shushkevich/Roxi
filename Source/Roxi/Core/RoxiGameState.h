// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "RoxiGameState.generated.h"

struct FStreamableHandle;

/**
 * Состояние матча/сервера, реплицируемое всем клиентам.
 * Также держит загруженными описания предметов, рецептов и построек (на сервере и клиентах),
 * чтобы их не выгружал сборщик мусора и не приходилось синхронно загружать во время обработки сети.
 */
UCLASS()
class ROXI_API ARoxiGameState : public AGameStateBase
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TArray<TSharedPtr<FStreamableHandle>> DefinitionHandles;
};
