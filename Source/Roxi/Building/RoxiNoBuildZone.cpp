// Copyright Epic Games, Inc. All Rights Reserved.

#include "Building/RoxiNoBuildZone.h"
#include "Components/SphereComponent.h"
#include "EngineUtils.h"

ARoxiNoBuildZone::ARoxiNoBuildZone()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	Zone = CreateDefaultSubobject<USphereComponent>(TEXT("Zone"));
	SetRootComponent(Zone);
	Zone->InitSphereRadius(10000.f); // 100 м (С14)
	Zone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Zone->ShapeColor = FColor::Red;
}

float ARoxiNoBuildZone::GetRadius() const
{
	return Zone->GetScaledSphereRadius();
}

void ARoxiNoBuildZone::SetRadius(float InRadius)
{
	Zone->SetSphereRadius(FMath::Max(0.f, InRadius));
}

bool ARoxiNoBuildZone::IsInAnyZone(const UWorld* World, const FVector& Location)
{
	if (!World)
	{
		return false;
	}
	for (TActorIterator<ARoxiNoBuildZone> It(World); It; ++It)
	{
		if (FVector::DistSquared(Location, It->GetActorLocation()) <= FMath::Square(It->GetRadius()))
		{
			return true;
		}
	}
	return false;
}
