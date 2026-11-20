// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/RoxiItemInteractionComponent.h"
#include "Items/RoxiInventoryComponent.h"
#include "Items/RoxiLootContainer.h"
#include "Items/RoxiWorldItem.h"
#include "Character/RoxiCharacter.h"
#include "Building/RoxiDeployable.h"
#include "Core/RoxiPlayerController.h"
#include "Items/RoxiItemDefinition.h"
#include "Roxi.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"

static TAutoConsoleVariable<float> CVarInteractRange(
	TEXT("roxi.InteractRange"), 250.f,
	TEXT("Interaction distance for items and containers, cm. Cheat: tests only."),
	ECVF_Cheat);

URoxiItemInteractionComponent::URoxiItemInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(true);
}

void URoxiItemInteractionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// Компонент на PlayerController — PlayerController реплицируется только своему игроку.
	DOREPLIFETIME_CONDITION(URoxiItemInteractionComponent, ViewedContainer, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(URoxiItemInteractionComponent, ViewedSlots, COND_OwnerOnly);
}

float URoxiItemInteractionComponent::GetInteractRange()
{
	return FMath::Max(50.f, CVarInteractRange.GetValueOnGameThread());
}

APawn* URoxiItemInteractionComponent::GetControlledPawn() const
{
	const AController* Controller = Cast<AController>(GetOwner());
	return Controller ? Controller->GetPawn() : nullptr;
}

URoxiInventoryComponent* URoxiItemInteractionComponent::GetPawnInventory() const
{
	const ARoxiCharacter* Character = Cast<ARoxiCharacter>(GetControlledPawn());
	return Character ? Character->GetInventory() : nullptr;
}

URoxiInventoryComponent* URoxiItemInteractionComponent::GetPawnBelt() const
{
	const ARoxiCharacter* Character = Cast<ARoxiCharacter>(GetControlledPawn());
	return Character ? Character->GetBelt() : nullptr;
}

bool URoxiItemInteractionComponent::IsInRange(const AActor* Target) const
{
	const APawn* Pawn = GetControlledPawn();
	return Pawn && Target
		&& FVector::DistSquared(Pawn->GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(GetInteractRange());
}

bool URoxiItemInteractionComponent::CanAccess(const URoxiInventoryComponent* Inventory) const
{
	if (!Inventory || !GetControlledPawn())
	{
		return false;
	}
	if (Inventory == GetPawnInventory() || Inventory == GetPawnBelt())
	{
		return true;
	}
	return Inventory == ViewedContainer && IsInRange(Inventory->GetOwner());
}

// ---------------------------------------------------------------------------
// Клиент

AActor* URoxiItemInteractionComponent::FindNearestInteractable() const
{
	const APawn* Pawn = GetControlledPawn();
	UWorld* World = GetWorld();
	if (!Pawn || !World)
	{
		return nullptr;
	}

	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams ObjectParams(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RoxiInteract), false, Pawn);
	World->OverlapMultiByObjectType(Overlaps, Pawn->GetActorLocation(), FQuat::Identity, ObjectParams,
		FCollisionShape::MakeSphere(GetInteractRange()), Params);

	AActor* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (Actor && (Actor->IsA<ARoxiWorldItem>() || Actor->IsA<ARoxiLootContainer>()))
		{
			const float DistSq = FVector::DistSquared(Pawn->GetActorLocation(), Actor->GetActorLocation());
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Best = Actor;
			}
		}
	}
	return Best;
}

void URoxiItemInteractionComponent::InteractNearest()
{
	AActor* Target = FindNearestInteractable();
	if (ARoxiWorldItem* WorldItem = Cast<ARoxiWorldItem>(Target))
	{
		ServerPickUp(WorldItem);
	}
	else if (ARoxiLootContainer* Container = Cast<ARoxiLootContainer>(Target))
	{
		if (ViewedContainer && ViewedContainer == Container->GetInventory())
		{
			ServerCloseContainer();
		}
		else
		{
			ServerOpenContainer(Container);
		}
	}
	else if (ViewedContainer)
	{
		ServerCloseContainer();
	}
}

// ---------------------------------------------------------------------------
// Сервер

void URoxiItemInteractionComponent::ServerMoveItem_Implementation(URoxiInventoryComponent* From, int32 FromSlot,
	URoxiInventoryComponent* To, int32 ToSlot, int32 Quantity)
{
	if (!CanAccess(From) || !CanAccess(To))
	{
		UE_LOG(LogRoxi, Warning, TEXT("MoveItem rejected for %s: no access"), *GetNameSafe(GetOwner()));
		return;
	}
	URoxiInventoryComponent::MoveItem(From, FromSlot, To, ToSlot, Quantity);
}

void URoxiItemInteractionComponent::ServerDropItem_Implementation(URoxiInventoryComponent* From, int32 Slot, int32 Quantity)
{
	const APawn* Pawn = GetControlledPawn();
	if (!Pawn || !CanAccess(From))
	{
		return;
	}
	const FRoxiItemStack Removed = From->RemoveFromSlot(Slot, Quantity);
	if (!Removed.IsEmpty())
	{
		const FVector DropLocation = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 80.f;
		ARoxiWorldItem::SpawnItem(GetWorld(), Removed, DropLocation);
		UE_LOG(LogRoxi, Log, TEXT("%s dropped %s"), *GetNameSafe(GetOwner()), *Removed.ToString());
	}
}

void URoxiItemInteractionComponent::ServerPickUp_Implementation(ARoxiWorldItem* WorldItem)
{
	URoxiInventoryComponent* Inventory = GetPawnInventory();
	URoxiInventoryComponent* Belt = GetPawnBelt();
	if (!WorldItem || !Inventory || WorldItem->IsActorBeingDestroyed() || !IsInRange(WorldItem))
	{
		return;
	}

	const FRoxiItemStack Stack = WorldItem->GetStack();
	int32 Left = Belt ? Belt->AddItem(Stack.Item, Stack.Quantity) : Stack.Quantity;
	Left = Inventory->AddItem(Stack.Item, Left);
	if (Left != Stack.Quantity)
	{
		UE_LOG(LogRoxi, Log, TEXT("%s picked up %s (left on ground: %d)"), *GetNameSafe(GetOwner()), *Stack.ToString(), Left);
	}
	WorldItem->SetQuantity(Left);
}

void URoxiItemInteractionComponent::ServerOpenContainer_Implementation(AActor* ContainerActor)
{
	const ARoxiLootContainer* Container = Cast<ARoxiLootContainer>(ContainerActor);
	URoxiInventoryComponent* Inventory = Container ? Container->GetInventory() : nullptr;
	if (!Inventory || !GetControlledPawn() || !IsInRange(Container))
	{
		return;
	}

	CloseContainer();
	ViewedContainer = Inventory;
	ViewedSlots.SyncFrom(Inventory->GetList());
	ViewedChangedHandle = Inventory->OnChanged.AddUObject(this, &URoxiItemInteractionComponent::HandleViewedContainerChanged);
	UE_LOG(LogRoxi, Log, TEXT("%s opened %s"), *GetNameSafe(GetOwner()), *GetNameSafe(Container));
}

void URoxiItemInteractionComponent::ServerPlaceDeployable_Implementation(URoxiInventoryComponent* From, int32 Slot, FVector Location, float Yaw)
{
	ARoxiPlayerController* PC = Cast<ARoxiPlayerController>(GetOwner());
	APawn* Pawn = GetControlledPawn();
	// Только из своего инвентаря или пояса.
	if (!PC || !Pawn || !From || (From != GetPawnInventory() && From != GetPawnBelt()))
	{
		return;
	}

	const FRoxiItemStack Stack = From->GetStack(Slot);
	TSubclassOf<ARoxiDeployable> DeployableClass = (!Stack.IsEmpty() && !Stack.Item->DeployableClass.IsNull())
		? Stack.Item->DeployableClass.LoadSynchronous() : nullptr;
	if (!DeployableClass)
	{
		PC->ClientShowMessage(TEXT("This item cannot be placed"));
		return;
	}

	FTransform Final;
	FString Reason;
	const FString OwnerId = PC->GetPersistentPlayerId();
	if (!ARoxiDeployable::ValidatePlacement(DeployableClass, Pawn, OwnerId, FTransform(FRotator(0.f, Yaw, 0.f), Location), Final, Reason))
	{
		PC->ClientShowMessage(FString::Printf(TEXT("Cannot place: %s"), *Reason));
		return;
	}

	From->RemoveFromSlot(Slot, 1);
	ARoxiDeployable* Deployable = GetWorld()->SpawnActorDeferred<ARoxiDeployable>(DeployableClass, Final, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	Deployable->InitOwner(OwnerId);
	Deployable->FinishSpawning(Final);
	UE_LOG(LogRoxi, Log, TEXT("%s placed %s at %s"), *GetNameSafe(PC), *Deployable->GetName(), *Final.GetLocation().ToCompactString());
}

void URoxiItemInteractionComponent::ServerCloseContainer_Implementation()
{
	CloseContainer();
}

void URoxiItemInteractionComponent::CloseContainer()
{
	if (ViewedContainer)
	{
		ViewedContainer->OnChanged.Remove(ViewedChangedHandle);
	}
	ViewedChangedHandle.Reset();
	if (ViewedContainer || ViewedSlots.Num() > 0)
	{
		ViewedContainer = nullptr;
		ViewedSlots.Reset();
	}
}

void URoxiItemInteractionComponent::HandleViewedContainerChanged(URoxiInventoryComponent* Inventory)
{
	if (Inventory == ViewedContainer)
	{
		ViewedSlots.SyncFrom(Inventory->GetList());
	}
}

void URoxiItemInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Сервер: закрыть контейнер, если игрок отошёл, умер или контейнер исчез.
	if (!GetOwner()->HasAuthority() || !ViewedContainer)
	{
		return;
	}
	RangeCheckTimer -= DeltaTime;
	if (RangeCheckTimer > 0.f)
	{
		return;
	}
	RangeCheckTimer = 0.5f;
	if (!IsValid(ViewedContainer) || !IsInRange(ViewedContainer->GetOwner()))
	{
		CloseContainer();
	}
}

void URoxiItemInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		CloseContainer();
	}
	Super::EndPlay(EndPlayReason);
}
