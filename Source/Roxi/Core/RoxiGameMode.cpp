// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/RoxiGameMode.h"
#include "Core/RoxiGameState.h"
#include "Core/RoxiPlayerController.h"
#include "Core/RoxiPlayerState.h"
#include "Character/RoxiCharacter.h"
#include "Roxi.h"

ARoxiGameMode::ARoxiGameMode()
{
	GameStateClass = ARoxiGameState::StaticClass();
	PlayerStateClass = ARoxiPlayerState::StaticClass();
	PlayerControllerClass = ARoxiPlayerController::StaticClass();

	// Переопределяется в BP_RoxiGameMode на BP-персонажа с мешем и анимациями.
	DefaultPawnClass = ARoxiCharacter::StaticClass();
}

void ARoxiGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// Super::PostLogin спавнит пешку (HandleStartingNewPlayer -> RestartPlayer), поэтому она уже доступна.
	const APawn* Pawn = NewPlayer ? NewPlayer->GetPawn() : nullptr;
	UE_LOG(LogRoxi, Log, TEXT("Player joined: %s, pawn: %s (players: %d)"),
		*GetNameSafe(NewPlayer ? NewPlayer->PlayerState : nullptr),
		Pawn ? *Pawn->GetClass()->GetName() : TEXT("none"), GetNumPlayers());
}

void ARoxiGameMode::Logout(AController* Exiting)
{
	UE_LOG(LogRoxi, Log, TEXT("Player left: %s"),
		*GetNameSafe(Exiting ? Exiting->PlayerState : nullptr));

	Super::Logout(Exiting);
}
