// Copyright Epic Games, Inc. All Rights Reserved.

#include "Building/RoxiBuildingPiece.h"
#include "Building/RoxiPlacement.h"
#include "Survival/RoxiHealthComponent.h"
#include "Roxi.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float SocketTolerance = 2.f;      // см
	constexpr float OccupiedTolerance = 30.f;   // см
	constexpr float OverlapShrink = 12.f;       // см: соседние элементы касаются, но не пересекаются

	float NormalizeYaw(float Yaw, float Period)
	{
		return FMath::Fmod(FMath::Fmod(Yaw, Period) + Period, Period);
	}

	bool YawMatches(float A, float B, float Period)
	{
		const float D = FMath::Abs(NormalizeYaw(A, Period) - NormalizeYaw(B, Period));
		return D < 1.f || D > Period - 1.f;
	}

	/** Симметрия формы: стена одинакова при повороте на 180°, квадратные фундамент и перекрытие — на 90°. */
	float SymmetryPeriod(ERoxiPieceType Type)
	{
		return Type == ERoxiPieceType::Wall ? 180.f : 90.f;
	}
}

ARoxiBuildingPiece::ARoxiBuildingPiece()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(false);
	NetDormancy = DORM_DormantAll;
	Tags.Add(RoxiPlacement::BlockerTag);
	Tags.Add(RoxiPlacement::BuildingPieceTag);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName); // WorldStatic: блокирует игроков и трассировки

	// Визуальная заглушка (движковый ассет); модели построек — отдельная задача.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaceholderMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (PlaceholderMesh.Succeeded())
	{
		Mesh->SetStaticMesh(PlaceholderMesh.Object);
	}

	Health = CreateDefaultSubobject<URoxiHealthComponent>(TEXT("Health"));
}

void ARoxiBuildingPiece::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(ARoxiBuildingPiece, PieceType, Params);
}

void ARoxiBuildingPiece::BeginPlay()
{
	Super::BeginPlay();
	ApplyShape();
	if (HasAuthority())
	{
		Health->OnDeath.AddWeakLambda(this, [this](URoxiHealthComponent*, AController* Killer)
		{
			UE_LOG(LogRoxi, Log, TEXT("%s destroyed by %s"), *GetName(), *GetNameSafe(Killer));
			Destroy(); // без устойчивости (С10): соседние элементы остаются
		});
	}
}

void ARoxiBuildingPiece::OnRep_PieceType()
{
	ApplyShape();
}

void ARoxiBuildingPiece::ApplyShape()
{
	// Куб движка — 100 см. Положение меша задаёт начало координат элемента (см. описание класса).
	const FVector Half = GetHalfExtent(PieceType);
	Mesh->SetRelativeScale3D(Half / 50.f);
	Mesh->SetRelativeLocation(GetBoundsCenter(PieceType, FRoxiBuildSocket()) );
}

FVector ARoxiBuildingPiece::GetHalfExtent(ERoxiPieceType Type)
{
	const float Half = RoxiBuildGrid::CellSize * 0.5f;
	switch (Type)
	{
	case ERoxiPieceType::Wall:  return FVector(Half, RoxiBuildGrid::WallThickness * 0.5f, RoxiBuildGrid::WallHeight * 0.5f);
	case ERoxiPieceType::Floor: return FVector(Half, Half, RoxiBuildGrid::FloorThickness * 0.5f);
	default:                    return FVector(Half, Half, RoxiBuildGrid::FoundationHeight * 0.5f);
	}
}

FVector ARoxiBuildingPiece::GetBoundsCenter(ERoxiPieceType Type, const FRoxiBuildSocket& Socket)
{
	switch (Type)
	{
	case ERoxiPieceType::Wall:  return Socket.Location + FVector(0, 0, RoxiBuildGrid::WallHeight * 0.5f);
	case ERoxiPieceType::Floor: return Socket.Location - FVector(0, 0, RoxiBuildGrid::FloorThickness * 0.5f);
	default:                    return Socket.Location - FVector(0, 0, RoxiBuildGrid::FoundationHeight * 0.5f);
	}
}

void ARoxiBuildingPiece::GetSockets(ERoxiPieceType ForType, TArray<FRoxiBuildSocket>& OutSockets) const
{
	const FVector Origin = GetActorLocation();
	const float Yaw = GetActorRotation().Yaw;
	const float Cell = RoxiBuildGrid::CellSize;

	if (PieceType == ERoxiPieceType::Foundation || PieceType == ERoxiPieceType::Floor)
	{
		for (int32 k = 0; k < 4; ++k)
		{
			const FVector Dir = FRotator(0.f, Yaw + 90.f * k, 0.f).Vector();
			if (ForType == ERoxiPieceType::Wall)
			{
				// Стена на краю, вдоль края.
				OutSockets.Add({ Origin + Dir * (Cell * 0.5f), Yaw + 90.f * k + 90.f });
			}
			else if (ForType == PieceType)
			{
				// Фундамент к фундаменту, перекрытие к перекрытию — край к краю.
				OutSockets.Add({ Origin + Dir * Cell, Yaw });
			}
		}
	}
	else if (PieceType == ERoxiPieceType::Wall && ForType == ERoxiPieceType::Floor)
	{
		// Перекрытие на верх стены — по обе стороны.
		const FVector Top = Origin + FVector(0, 0, RoxiBuildGrid::WallHeight);
		const FVector Normal = FRotator(0.f, Yaw + 90.f, 0.f).Vector();
		OutSockets.Add({ Top + Normal * (Cell * 0.5f), Yaw });
		OutSockets.Add({ Top - Normal * (Cell * 0.5f), Yaw });
	}
}

void ARoxiBuildingPiece::GatherNearbyPieces(const UWorld* World, const FVector& Center, float Radius, TArray<ARoxiBuildingPiece*>& OutPieces)
{
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic),
		FCollisionShape::MakeSphere(Radius), FCollisionQueryParams(SCENE_QUERY_STAT(RoxiBuildGather), false));
	for (const FOverlapResult& Overlap : Overlaps)
	{
		ARoxiBuildingPiece* Piece = Cast<ARoxiBuildingPiece>(Overlap.GetActor());
		if (Piece && !Piece->IsActorBeingDestroyed())
		{
			OutPieces.AddUnique(Piece);
		}
	}
}

bool ARoxiBuildingPiece::IsSocketOccupied(const UWorld* World, ERoxiPieceType Type, const FRoxiBuildSocket& Socket)
{
	TArray<ARoxiBuildingPiece*> Pieces;
	GatherNearbyPieces(World, Socket.Location, 200.f, Pieces);
	for (const ARoxiBuildingPiece* Piece : Pieces)
	{
		if (Piece->PieceType == Type && FVector::Dist(Piece->GetActorLocation(), Socket.Location) < OccupiedTolerance)
		{
			return true;
		}
	}
	return false;
}

bool ARoxiBuildingPiece::MatchesNearbySocket(const UWorld* World, ERoxiPieceType Type, const FRoxiBuildSocket& Socket)
{
	TArray<ARoxiBuildingPiece*> Pieces;
	GatherNearbyPieces(World, Socket.Location, RoxiBuildGrid::CellSize * 1.5f, Pieces);
	TArray<FRoxiBuildSocket> Sockets;
	for (const ARoxiBuildingPiece* Piece : Pieces)
	{
		Sockets.Reset();
		Piece->GetSockets(Type, Sockets);
		for (const FRoxiBuildSocket& Candidate : Sockets)
		{
			if (FVector::Dist(Candidate.Location, Socket.Location) < SocketTolerance
				&& YawMatches(Candidate.Yaw, Socket.Yaw, SymmetryPeriod(Type)))
			{
				return true;
			}
		}
	}
	return false;
}

bool ARoxiBuildingPiece::HasGroundSupport(const UWorld* World, const FRoxiBuildSocket& Socket)
{
	// Земля (не постройки) под центром и углами: верх фундамента на 0..100 см выше земли (может немного утопать).
	TArray<ARoxiBuildingPiece*> Pieces;
	GatherNearbyPieces(World, Socket.Location, RoxiBuildGrid::CellSize * 1.5f, Pieces);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RoxiFoundationGround), false);
	for (ARoxiBuildingPiece* Piece : Pieces)
	{
		Params.AddIgnoredActor(Piece);
	}

	const FRotator Rotation(0.f, Socket.Yaw, 0.f);
	const float Corner = RoxiBuildGrid::CellSize * 0.5f - 10.f;
	const FVector Offsets[] = { FVector::ZeroVector, FVector(Corner, Corner, 0), FVector(Corner, -Corner, 0),
		FVector(-Corner, Corner, 0), FVector(-Corner, -Corner, 0) };
	for (const FVector& Offset : Offsets)
	{
		const FVector Point = Socket.Location + Rotation.RotateVector(Offset);
		FHitResult Hit;
		if (!World->LineTraceSingleByObjectType(Hit, Point + FVector(0, 0, 50.f), Point - FVector(0, 0, 200.f),
			FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			return false;
		}
		const float Height = Socket.Location.Z - Hit.ImpactPoint.Z;
		if (Height < -50.f || Height > RoxiBuildGrid::FoundationHeight)
		{
			return false;
		}
	}
	return true;
}

bool ARoxiBuildingPiece::FindPlacement(const UWorld* World, ERoxiPieceType Type, const FVector& Aim, const FVector& Viewer,
	float AimYaw, FRoxiBuildSocket& OutSocket)
{
	if (!World)
	{
		return false;
	}

	// 1. Ближайшее свободное гнездо соседних элементов.
	TArray<ARoxiBuildingPiece*> Pieces;
	GatherNearbyPieces(World, Aim, RoxiBuildGrid::CellSize * 2.5f, Pieces);
	const float MaxSnap = RoxiBuildGrid::CellSize * 1.2f;
	float BestDist = MaxSnap;
	bool bFound = false;
	TArray<FRoxiBuildSocket> Sockets;
	for (const ARoxiBuildingPiece* Piece : Pieces)
	{
		Sockets.Reset();
		Piece->GetSockets(Type, Sockets);
		for (const FRoxiBuildSocket& Candidate : Sockets)
		{
			// При равной близости к прицелу — гнездо дальше от игрока (не строить у него под ногами).
			const float Dist = FVector::Dist(Candidate.Location, Aim) - 0.05f * FVector::Dist2D(Candidate.Location, Viewer);
			if (Dist < BestDist && !IsSocketOccupied(World, Type, Candidate))
			{
				BestDist = Dist;
				OutSocket = Candidate;
				bFound = true;
			}
		}
	}
	if (bFound || Type != ERoxiPieceType::Foundation)
	{
		return bFound;
	}

	// 2. Фундамент — свободно на землю под точкой.
	FHitResult Hit;
	if (World->LineTraceSingleByObjectType(Hit, Aim + FVector(0, 0, 200.f), Aim - FVector(0, 0, 400.f),
		FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionQueryParams(SCENE_QUERY_STAT(RoxiFoundationFree), false))
		&& !Cast<ARoxiBuildingPiece>(Hit.GetActor()))
	{
		OutSocket.Location = Hit.ImpactPoint + FVector(0, 0, RoxiBuildGrid::FoundationHeight * 0.5f);
		OutSocket.Yaw = AimYaw;
		return true;
	}
	return false;
}

bool ARoxiBuildingPiece::ValidatePlacement(const URoxiBuildPieceDefinition* Definition, const APawn* Builder, const FRoxiBuildSocket& Socket, FString& OutReason)
{
	const UWorld* World = Builder ? Builder->GetWorld() : nullptr;
	if (!Definition || !World)
	{
		OutReason = TEXT("Invalid piece");
		return false;
	}

	const ERoxiPieceType Type = Definition->PieceType;
	const FVector Center = GetBoundsCenter(Type, Socket);
	if (!RoxiPlacement::IsWithinReach(Builder, Center))
	{
		OutReason = TEXT("Too far");
		return false;
	}

	OutReason = RoxiPlacement::CheckRestrictedArea(World, Socket.Location);
	if (!OutReason.IsEmpty())
	{
		return false;
	}

	const FVector Half = GetHalfExtent(Type);
	const FVector Extent(FMath::Max(1.f, Half.X - OverlapShrink), FMath::Max(1.f, Half.Y - OverlapShrink), FMath::Max(1.f, Half.Z - 2.f));
	const FQuat Rotation = FRotator(0.f, Socket.Yaw, 0.f).Quaternion();

	if (Type == ERoxiPieceType::Foundation)
	{
		if (!HasGroundSupport(World, Socket))
		{
			OutReason = TEXT("Foundation needs ground under it");
			return false;
		}
		if (RoxiPlacement::OverlapsBlockers(World, Center, Extent, Rotation))
		{
			OutReason = TEXT("Blocked");
			return false;
		}
		return true;
	}

	// Стена и перекрытие — только к гнёздам существующих элементов.
	if (!MatchesNearbySocket(World, Type, Socket))
	{
		OutReason = TEXT("Must attach to an existing piece");
		return false;
	}
	if (IsSocketOccupied(World, Type, Socket))
	{
		OutReason = TEXT("Already built here");
		return false;
	}
	if (RoxiPlacement::OverlapsBlockers(World, Center, Extent, Rotation, /*bIgnoreBuildingPieces*/ true))
	{
		OutReason = TEXT("Blocked");
		return false;
	}
	return true;
}

ARoxiBuildingPiece* ARoxiBuildingPiece::SpawnPiece(UWorld* World, const URoxiBuildPieceDefinition* Definition, const FRoxiBuildSocket& Socket, const FString& InOwnerId)
{
	if (!World || !Definition)
	{
		return nullptr;
	}
	const FTransform Transform(FRotator(0.f, Socket.Yaw, 0.f), Socket.Location);
	ARoxiBuildingPiece* Piece = World->SpawnActorDeferred<ARoxiBuildingPiece>(ARoxiBuildingPiece::StaticClass(), Transform,
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Piece)
	{
		Piece->PieceType = Definition->PieceType;
		Piece->OwnerId = InOwnerId;
		Piece->Health->SetInitialMaxHealth(Definition->MaxHealth);
		MARK_PROPERTY_DIRTY_FROM_NAME(ARoxiBuildingPiece, PieceType, Piece);
		Piece->FinishSpawning(Transform);
	}
	return Piece;
}
