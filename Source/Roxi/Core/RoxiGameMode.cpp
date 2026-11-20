// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/RoxiGameMode.h"
#include "Core/RoxiGameState.h"
#include "Core/RoxiPlayerController.h"
#include "Core/RoxiPlayerState.h"
#include "Dev/RoxiDebugHUD.h"
#include "Character/RoxiCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Items/RoxiInventoryComponent.h"
#include "Items/RoxiLootContainer.h"
#include "Building/RoxiSleepingBag.h"
#include "Crafting/RoxiCraftingComponent.h"
#include "GameFramework/PlayerState.h"
#include "Roxi.h"

ARoxiGameMode::ARoxiGameMode()
{
	GameStateClass = ARoxiGameState::StaticClass();
	PlayerStateClass = ARoxiPlayerState::StaticClass();
	PlayerControllerClass = ARoxiPlayerController::StaticClass();
	HUDClass = ARoxiDebugHUD::StaticClass(); // временный отладочный UI (Т9)

	// Переопределяется в BP_RoxiGameMode на BP-персонажа с мешем и анимациями.
	DefaultPawnClass = ARoxiCharacter::StaticClass();
}

void ARoxiGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// Super::PostLogin спавнит пешку (HandleStartingNewPlayer -> RestartPlayer), поэтому она уже доступна.
	const APawn* Pawn = NewPlayer ? NewPlayer->GetPawn() : nullptr;
	UE_LOG(LogRoxi, Log, TEXT("Player joined: %s, pawn: %s at %s (players: %d)"),
		*GetNameSafe(NewPlayer ? NewPlayer->PlayerState : nullptr),
		Pawn ? *Pawn->GetClass()->GetName() : TEXT("none"),
		Pawn ? *Pawn->GetActorLocation().ToCompactString() : TEXT("-"), GetNumPlayers());

	if (ARoxiPlayerController* PC = Cast<ARoxiPlayerController>(NewPlayer))
	{
		PC->RefreshRespawnPoints(); // точки, поставленные в прошлых сеансах
	}
}

void ARoxiGameMode::Logout(AController* Exiting)
{
	UE_LOG(LogRoxi, Log, TEXT("Player left: %s"),
		*GetNameSafe(Exiting ? Exiting->PlayerState : nullptr));

	Super::Logout(Exiting);
}

void ARoxiGameMode::HandleCharacterDeath(ARoxiCharacter* Victim, AController* Killer)
{
	if (!Victim)
	{
		return;
	}

	AController* VictimController = Victim->GetController();
	const FString VictimName = (VictimController && VictimController->PlayerState)
		? VictimController->PlayerState->GetPlayerName() : Victim->GetName();

	// 0. Незавершённый крафт — вернуть ресурсы в инвентарь (К2), чтобы они попали в контейнер.
	Victim->GetCrafting()->CancelAll();

	// 1. Все предметы игрока (инвентарь и пояс — В8) — в один список.
	TArray<FRoxiItemStack> Items = Victim->GetInventory()->RemoveAll();
	Items.Append(Victim->GetBelt()->RemoveAll());

	// 2. Контейнер с добычей на месте смерти (только если есть что положить).
	ARoxiLootContainer* Container = nullptr;
	if (!Items.IsEmpty())
	{
		const float HalfHeight = Victim->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const FVector Location = Victim->GetActorLocation() - FVector(0.f, 0.f, HalfHeight - 15.f);

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Container = GetWorld()->SpawnActor<ARoxiLootContainer>(Location, Victim->GetActorRotation(), Params);
		if (Container)
		{
			Container->SetOwnerLabel(VictimName);
			URoxiInventoryComponent* Loot = Container->GetInventory();
			Loot->SetNumSlots(Items.Num());
			for (const FRoxiItemStack& Stack : Items)
			{
				Loot->AddItem(Stack.Item, Stack.Quantity);
			}
		}
	}

	UE_LOG(LogRoxi, Log, TEXT("%s died (killer: %s), %d stacks -> %s"),
		*VictimName, *GetNameSafe(Killer), Items.Num(), Container ? *Container->GetName() : TEXT("no container"));

	// 3. Персонаж удаляется; контроллер остаётся без пешки до запроса возрождения.
	Victim->DetachFromControllerPendingDestroy();
	Victim->Destroy();
}

void ARoxiGameMode::RequestRespawn(AController* Controller, int32 PointId)
{
	if (!Controller || Controller->GetPawn())
	{
		return; // жив — возрождать нечего
	}

	ARoxiPlayerController* PC = Cast<ARoxiPlayerController>(Controller);
	if (PointId > 0)
	{
		ARoxiSleepingBag* Point = ARoxiSleepingBag::FindById(GetWorld(), PointId);
		if (!Point || !PC || Point->GetOwnerId() != PC->GetPersistentPlayerId() || !Point->IsAvailable())
		{
			if (PC)
			{
				PC->ClientShowMessage(TEXT("Respawn point unavailable"));
			}
			UE_LOG(LogRoxi, Log, TEXT("%s: respawn point %d rejected"), *GetNameSafe(Controller->PlayerState), PointId);
			return;
		}
		Point->MarkUsed();
		RestartPlayerAtTransform(Controller, FTransform(Point->GetActorRotation(), Point->GetActorLocation() + FVector(0, 0, 100.f)));
	}
	else
	{
		RestartPlayer(Controller);
	}

	const APawn* Pawn = Controller->GetPawn();
	UE_LOG(LogRoxi, Log, TEXT("%s respawned (point %d) at %s"), *GetNameSafe(Controller->PlayerState), PointId,
		Pawn ? *Pawn->GetActorLocation().ToCompactString() : TEXT("(failed)"));
}

void ARoxiGameMode::NotifyRespawnPointsChanged(const FString& OwnerId)
{
	if (OwnerId.IsEmpty())
	{
		return;
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ARoxiPlayerController* PC = Cast<ARoxiPlayerController>(It->Get());
		if (PC && PC->GetPersistentPlayerId() == OwnerId)
		{
			PC->RefreshRespawnPoints();
		}
	}
}
