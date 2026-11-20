// Copyright Epic Games, Inc. All Rights Reserved.

#include "Building/RoxiDeployable.h"
#include "Building/RoxiPlacement.h"
#include "Survival/RoxiHealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

ARoxiDeployable::ARoxiDeployable()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(false);
	NetDormancy = DORM_DormantAll;
	Tags.Add(RoxiPlacement::BlockerTag);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	// Игроки проходят сквозь (спальник), но объект находится трассировками и мешает размещению.
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionObjectType(ECC_WorldDynamic);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// Визуальная заглушка (движковый ассет); модели — отдельная задача.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaceholderMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (PlaceholderMesh.Succeeded())
	{
		Mesh->SetStaticMesh(PlaceholderMesh.Object);
	}

	Health = CreateDefaultSubobject<URoxiHealthComponent>(TEXT("Health"));
}

void ARoxiDeployable::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		Health->OnDeath.AddWeakLambda(this, [this](URoxiHealthComponent*, AController*) { HandleHealthDepleted(); });
	}
}

void ARoxiDeployable::HandleHealthDepleted()
{
	Destroy();
}

bool ARoxiDeployable::ValidatePlacement(TSubclassOf<ARoxiDeployable> DeployableClass, const APawn* Placer, const FString& InOwnerId,
	const FTransform& Desired, FTransform& OutTransform, FString& OutReason)
{
	const ARoxiDeployable* CDO = DeployableClass ? DeployableClass->GetDefaultObject<ARoxiDeployable>() : nullptr;
	UWorld* World = Placer ? Placer->GetWorld() : nullptr;
	if (!CDO || !World)
	{
		OutReason = TEXT("Invalid deployable");
		return false;
	}

	const FVector Location = Desired.GetLocation();
	if (!RoxiPlacement::IsWithinReach(Placer, Location))
	{
		OutReason = TEXT("Too far");
		return false;
	}

	// Опора под объектом: земля или постройка.
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(RoxiDeployGround), false, Placer);
	if (!World->LineTraceSingleByChannel(Hit, Location + FVector(0, 0, 100.f), Location - FVector(0, 0, 300.f), ECC_Visibility, Query))
	{
		OutReason = TEXT("No ground");
		return false;
	}
	// Опора — только статичная геометрия (земля, уровень, элементы построек), а не предметы и другие объекты.
	if (!Hit.GetComponent() || Hit.GetComponent()->GetCollisionObjectType() != ECC_WorldStatic)
	{
		OutReason = TEXT("Blocked");
		return false;
	}
	if (FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Hit.ImpactNormal, FVector::UpVector))) > CDO->MaxSlopeDegrees)
	{
		OutReason = TEXT("Surface too steep");
		return false;
	}

	const FRotator Rotation(0.f, Desired.Rotator().Yaw, 0.f);
	const FVector Final = Hit.ImpactPoint;

	OutReason = RoxiPlacement::CheckRestrictedArea(World, Final);
	if (!OutReason.IsEmpty())
	{
		return false;
	}

	const FVector Extent = CDO->FootprintExtent;
	if (RoxiPlacement::OverlapsBlockers(World, Final + FVector(0, 0, Extent.Z + 2.f), Extent, Rotation.Quaternion()))
	{
		OutReason = TEXT("Blocked");
		return false;
	}

	if (!CDO->CanBePlacedBy(World, InOwnerId, OutReason))
	{
		return false;
	}

	OutTransform = FTransform(Rotation, Final);
	return true;
}
