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
#include "Save/RoxiPersistenceSubsystem.h"
#include "Survival/RoxiHealthComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
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

FString ARoxiGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
	const FString& Options, const FString& Portal)
{
	const FString Error = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);

	if (ARoxiPlayerState* RoxiState = NewPlayerController ? NewPlayerController->GetPlayerState<ARoxiPlayerState>() : nullptr)
	{
		FString Id = UniqueId.IsValid() ? UniqueId.ToString() : FString();
#if !UE_BUILD_SHIPPING
		// Тесты: стабильный ID поверх случайного ID OnlineSubsystemNull.
		if (UGameplayStatics::HasOption(Options, TEXT("RoxiDevId")))
		{
			Id = TEXT("dev:") + UGameplayStatics::ParseOption(Options, TEXT("RoxiDevId"));
		}
#endif
		RoxiState->SetPersistentId(Id);
	}
	return Error;
}

ARoxiCharacter* ARoxiGameMode::FindSleeper(const FString& PlayerId) const
{
	if (!PlayerId.IsEmpty())
	{
		for (TActorIterator<ARoxiCharacter> It(GetWorld()); It; ++It)
		{
			if (It->IsSleeping() && It->GetSleeperId() == PlayerId && !It->IsActorBeingDestroyed())
			{
				return *It;
			}
		}
	}
	return nullptr;
}

void ARoxiGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// Пока мир не загружен из сохранения — ждать: иначе новый персонаж появится рядом с ещё не загруженным спящим.
	const URoxiPersistenceSubsystem* Persistence = GetWorld()->GetSubsystem<URoxiPersistenceSubsystem>();
	if (Persistence && !Persistence->IsReady())
	{
		PendingStartPlayers.AddUnique(NewPlayer);
		return;
	}

	const ARoxiPlayerController* PC = Cast<ARoxiPlayerController>(NewPlayer);
	const FString PlayerId = PC ? PC->GetPersistentPlayerId() : FString();

	// 1. Свой спящий персонаж жив — продолжить им (GDD §5.4).
	if (ARoxiCharacter* Sleeper = FindSleeper(PlayerId))
	{
		Sleeper->ExitSleep();
		NewPlayer->Possess(Sleeper);
		UE_LOG(LogRoxi, Log, TEXT("%s woke up their character at %s"), *PlayerId, *Sleeper->GetActorLocation().ToCompactString());
		return;
	}

	// 2. Игрок мёртв (умер и вышел, или спящего убили) — без персонажа, выбор точки возрождения.
	if (DeadPlayers.Contains(PlayerId))
	{
		UE_LOG(LogRoxi, Log, TEXT("%s is dead, waiting for respawn choice"), *PlayerId);
		return;
	}

	// 3. Новый игрок — случайная точка (GDD §4.4).
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

void ARoxiGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	ARoxiPlayerController* PC = Cast<ARoxiPlayerController>(NewPlayer);
	const APawn* Pawn = NewPlayer ? NewPlayer->GetPawn() : nullptr;
	UE_LOG(LogRoxi, Log, TEXT("Player joined: %s (%s), pawn: %s at %s (players: %d)"),
		*GetNameSafe(NewPlayer ? NewPlayer->PlayerState : nullptr), PC ? *PC->GetPersistentPlayerId() : TEXT("?"),
		Pawn ? *Pawn->GetClass()->GetName() : TEXT("none"),
		Pawn ? *Pawn->GetActorLocation().ToCompactString() : TEXT("-"), GetNumPlayers());

	if (PC)
	{
		PC->RefreshRespawnPoints(); // точки, поставленные в прошлых сеансах
		if (URoxiPersistenceSubsystem* Persistence = GetWorld()->GetSubsystem<URoxiPersistenceSubsystem>())
		{
			Persistence->TouchProfile(PC->GetPersistentPlayerId(), PC->PlayerState ? PC->PlayerState->GetPlayerName() : FString());
		}
	}
}

void ARoxiGameMode::Logout(AController* Exiting)
{
	const ARoxiPlayerController* PC = Cast<ARoxiPlayerController>(Exiting);
	const FString PlayerId = PC ? PC->GetPersistentPlayerId() : FString();

	// Живой персонаж уже уснул в ARoxiPlayerController::PawnLeavingGame (вызывается раньше Logout).
	if (Exiting && Exiting->GetPawn() == nullptr && !PlayerId.IsEmpty() && !FindSleeper(PlayerId))
	{
		DeadPlayers.Add(PlayerId);
		UE_LOG(LogRoxi, Log, TEXT("Player left: %s (dead)"), *PlayerId);
	}

	Super::Logout(Exiting);
}

void ARoxiGameMode::HandleCharacterDeath(ARoxiCharacter* Victim, AController* Killer)
{
	if (!Victim)
	{
		return;
	}

	AController* VictimController = Victim->GetController();
	FString VictimName = (VictimController && VictimController->PlayerState)
		? VictimController->PlayerState->GetPlayerName() : Victim->GetName();
	if (Victim->IsSleeping())
	{
		// Спящего убили — при входе владелец выберет точку возрождения (GDD §5.4).
		VictimName = Victim->GetSleeperName();
		DeadPlayers.Add(Victim->GetSleeperId());
	}

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

	UE_LOG(LogRoxi, Log, TEXT("%s died%s (killer: %s), %d stacks -> %s"), *VictimName,
		Victim->IsSleeping() ? TEXT(" while sleeping") : TEXT(""), *GetNameSafe(Killer), Items.Num(),
		Container ? *Container->GetName() : TEXT("no container"));

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

	if (PC && Controller->GetPawn())
	{
		DeadPlayers.Remove(PC->GetPersistentPlayerId());
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

void ARoxiGameMode::OnWorldLoaded()
{
	URoxiPersistenceSubsystem* Persistence = GetWorld()->GetSubsystem<URoxiPersistenceSubsystem>();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		ARoxiPlayerController* PC = Cast<ARoxiPlayerController>(It->Get());
		if (PC && Persistence)
		{
			Persistence->TouchProfile(PC->GetPersistentPlayerId(), PC->PlayerState ? PC->PlayerState->GetPlayerName() : FString());
		}
	}

	TArray<TWeakObjectPtr<APlayerController>> Pending = MoveTemp(PendingStartPlayers);
	PendingStartPlayers.Reset();
	for (const TWeakObjectPtr<APlayerController>& PC : Pending)
	{
		if (PC.IsValid() && !PC->GetPawn())
		{
			HandleStartingNewPlayer(PC.Get());
		}
	}
}
