// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/RoxiWorldItem.h"
#include "Items/RoxiItemDefinition.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ARoxiWorldItem::ARoxiWorldItem()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(false);
	NetDormancy = DORM_DormantAll;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetRelativeScale3D(FVector(0.25f));
	// Только для запросов: игроки проходят сквозь предмет, а поиск взаимодействия его находит.
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionObjectType(ECC_WorldDynamic);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// Заглушка, пока у предмета не задан WorldMesh. Движковый ассет, не контент проекта.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaceholderMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (PlaceholderMesh.Succeeded())
	{
		Mesh->SetStaticMesh(PlaceholderMesh.Object);
	}
}

void ARoxiWorldItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	DOREPLIFETIME_WITH_PARAMS_FAST(ARoxiWorldItem, Stack, Params);
}

ARoxiWorldItem* ARoxiWorldItem::SpawnItem(UWorld* World, const FRoxiItemStack& InStack, const FVector& Location)
{
	if (!World || InStack.IsEmpty())
	{
		return nullptr;
	}

	// Положить на поверхность под точкой.
	FVector SpawnLocation = Location;
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(RoxiWorldItemDrop), false);
	if (World->LineTraceSingleByChannel(Hit, Location + FVector(0, 0, 50.f), Location - FVector(0, 0, 500.f), ECC_WorldStatic, Query))
	{
		SpawnLocation = Hit.ImpactPoint + FVector(0, 0, 12.5f);
	}

	ARoxiWorldItem* Item = World->SpawnActorDeferred<ARoxiWorldItem>(ARoxiWorldItem::StaticClass(),
		FTransform(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), SpawnLocation), nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Item)
	{
		Item->Stack = InStack;
		Item->FinishSpawning(Item->GetActorTransform());
	}
	return Item;
}

void ARoxiWorldItem::BeginPlay()
{
	Super::BeginPlay();
	UpdateVisuals();
}

void ARoxiWorldItem::SetQuantity(int32 NewQuantity)
{
	if (!HasAuthority())
	{
		return;
	}
	if (NewQuantity <= 0)
	{
		Destroy();
		return;
	}
	if (NewQuantity != Stack.Quantity)
	{
		FlushNetDormancy();
		Stack.Quantity = NewQuantity;
		MARK_PROPERTY_DIRTY_FROM_NAME(ARoxiWorldItem, Stack, this);
	}
}

void ARoxiWorldItem::OnRep_Stack()
{
	UpdateVisuals();
}

void ARoxiWorldItem::UpdateVisuals()
{
	if (Stack.Item && !Stack.Item->WorldMesh.IsNull())
	{
		if (UStaticMesh* ItemMesh = Stack.Item->WorldMesh.LoadSynchronous())
		{
			Mesh->SetStaticMesh(ItemMesh);
		}
	}
}
