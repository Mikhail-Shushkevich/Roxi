// Copyright Epic Games, Inc. All Rights Reserved.

#include "Building/RoxiPlacement.h"
#include "Building/RoxiNoBuildZone.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PhysicsVolume.h"

namespace RoxiPlacement
{
	const FName BlockerTag(TEXT("RoxiPlacementBlocker"));
	const FName BuildingPieceTag(TEXT("RoxiBuildingPiece"));

	FString CheckRestrictedArea(const UWorld* World, const FVector& Location)
	{
		if (ARoxiNoBuildZone::IsInAnyZone(World, Location))
		{
			return TEXT("No-build zone");
		}
		// Вода: пока только водные PhysicsVolume (система воды карты не выбрана — К6).
		for (TActorIterator<APhysicsVolume> It(World); It; ++It)
		{
			if (It->bWaterVolume && It->EncompassesPoint(Location))
			{
				return TEXT("Cannot build on water");
			}
		}
		return FString();
	}

	bool OverlapsBlockers(const UWorld* World, const FVector& Center, const FVector& HalfExtent, const FQuat& Rotation,
		bool bIgnoreBuildingPieces)
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionObjectQueryParams Objects;
		Objects.AddObjectTypesToQuery(ECC_Pawn);
		Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
		Objects.AddObjectTypesToQuery(ECC_WorldStatic);
		World->OverlapMultiByObjectType(Overlaps, Center, Rotation, Objects, FCollisionShape::MakeBox(HalfExtent),
			FCollisionQueryParams(SCENE_QUERY_STAT(RoxiPlacementOverlap), false));

		for (const FOverlapResult& Overlap : Overlaps)
		{
			const AActor* Actor = Overlap.GetActor();
			if (Actor && (Actor->IsA<APawn>()
				|| (Actor->ActorHasTag(BlockerTag) && !(bIgnoreBuildingPieces && Actor->ActorHasTag(BuildingPieceTag)))))
			{
				return true;
			}
		}
		return false;
	}

	bool IsWithinReach(const APawn* Placer, const FVector& Location)
	{
		return Placer && FVector::DistSquared(Placer->GetActorLocation(), Location) <= FMath::Square(MaxPlaceDistance);
	}
}
