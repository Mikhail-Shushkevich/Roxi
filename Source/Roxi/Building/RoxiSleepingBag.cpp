// Copyright Epic Games, Inc. All Rights Reserved.

#include "Building/RoxiSleepingBag.h"
#include "Core/RoxiGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Survival/RoxiHealthComponent.h"

namespace
{
	int32 GNextRespawnPointId = 1;
}

ARoxiSleepingBag::ARoxiSleepingBag()
{
	DisplayName = NSLOCTEXT("Roxi", "SleepingBag", "Sleeping bag");
	FootprintExtent = FVector(100.f, 45.f, 10.f);
	Mesh->SetRelativeScale3D(FVector(2.f, 0.9f, 0.1f));
	Health->SetInitialMaxHealth(100.f);
}

ARoxiBed::ARoxiBed()
{
	DisplayName = NSLOCTEXT("Roxi", "Bed", "Bed");
	RespawnCooldown = 120.f;
	FootprintExtent = FVector(105.f, 55.f, 25.f);
	Mesh->SetRelativeScale3D(FVector(2.1f, 1.1f, 0.5f));
	Health->SetInitialMaxHealth(250.f);
}

void ARoxiSleepingBag::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		PointId = GNextRespawnPointId++;
		NotifyOwnerChanged();
	}
}

void ARoxiSleepingBag::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority() && EndPlayReason == EEndPlayReason::Destroyed)
	{
		PointId = 0; // уже не точка возрождения
		NotifyOwnerChanged();
	}
	Super::EndPlay(EndPlayReason);
}

void ARoxiSleepingBag::NotifyOwnerChanged() const
{
	if (ARoxiGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARoxiGameMode>() : nullptr)
	{
		GameMode->NotifyRespawnPointsChanged(OwnerId);
	}
}

bool ARoxiSleepingBag::IsAvailable() const
{
	return PointId != 0 && GetWorld() && GetWorld()->GetTimeSeconds() >= AvailableAt;
}

void ARoxiSleepingBag::MarkUsed()
{
	AvailableAt = GetWorld()->GetTimeSeconds() + RespawnCooldown;
	NotifyOwnerChanged();
}

bool ARoxiSleepingBag::CanBePlacedBy(const UWorld* World, const FString& InOwnerId, FString& OutReason) const
{
	if (FindByOwner(World, InOwnerId).Num() >= MaxPerOwner)
	{
		OutReason = FString::Printf(TEXT("Respawn point limit reached (%d)"), MaxPerOwner);
		return false;
	}
	return true;
}

ARoxiSleepingBag* ARoxiSleepingBag::FindById(const UWorld* World, int32 Id)
{
	if (World && Id > 0)
	{
		for (TActorIterator<ARoxiSleepingBag> It(World); It; ++It)
		{
			if (It->PointId == Id && !It->IsActorBeingDestroyed())
			{
				return *It;
			}
		}
	}
	return nullptr;
}

TArray<ARoxiSleepingBag*> ARoxiSleepingBag::FindByOwner(const UWorld* World, const FString& InOwnerId)
{
	TArray<ARoxiSleepingBag*> Result;
	if (World && !InOwnerId.IsEmpty())
	{
		for (TActorIterator<ARoxiSleepingBag> It(World); It; ++It)
		{
			if (It->PointId != 0 && It->OwnerId == InOwnerId && !It->IsActorBeingDestroyed())
			{
				Result.Add(*It);
			}
		}
	}
	return Result;
}
