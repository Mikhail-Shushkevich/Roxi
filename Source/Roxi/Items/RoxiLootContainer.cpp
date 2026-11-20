// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/RoxiLootContainer.h"
#include "Items/RoxiInventoryComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ARoxiLootContainer::ARoxiLootContainer()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(false);
	// Сам актор неподвижен; содержимое идёт не через него — можно спать по сети.
	NetDormancy = DORM_DormantAll;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetRelativeScale3D(FVector(0.6f, 0.4f, 0.3f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionObjectType(ECC_WorldDynamic);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// Визуальная заглушка (движковый ассет). Модель контейнера — отдельная задача.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaceholderMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (PlaceholderMesh.Succeeded())
	{
		Mesh->SetStaticMesh(PlaceholderMesh.Object);
	}

	Inventory = CreateDefaultSubobject<URoxiInventoryComponent>(TEXT("Inventory"));
}

void ARoxiLootContainer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(ARoxiLootContainer, OwnerLabel, Params);
}

void ARoxiLootContainer::SetOwnerLabel(const FString& InLabel)
{
	if (HasAuthority() && OwnerLabel != InLabel)
	{
		FlushNetDormancy();
		OwnerLabel = InLabel;
		MARK_PROPERTY_DIRTY_FROM_NAME(ARoxiLootContainer, OwnerLabel, this);
	}
}
