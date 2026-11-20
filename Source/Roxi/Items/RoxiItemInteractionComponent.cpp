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
	DOREPLIFETIME_CONDITION(URoxiItemInteractionComponent, ViewedContainer2, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(URoxiItemInteractionComponent, ViewedSlots2, COND_OwnerOnly);
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
	if (Inventory != ViewedContainer && Inventory != ViewedContainer2)
	{
		return false;
	}
	// Проснувшегося (владелец вошёл в игру) обыскивать уже нельзя — даже до автозакрытия.
	const ARoxiCharacter* Sleeper = Cast<ARoxiCharacter>(Inventory->GetOwner());
	return IsInRange(Inventory->GetOwner()) && (!Sleeper || Sleeper->IsSleeping());
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
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn); // спящие персонажи
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RoxiInteract), false, Pawn);
	World->OverlapMultiByObjectType(Overlaps, Pawn->GetActorLocation(), FQuat::Identity, ObjectParams,
		FCollisionShape::MakeSphere(GetInteractRange()), Params);

	AActor* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		const ARoxiCharacter* Sleeper = Cast<ARoxiCharacter>(Actor);
		if (Actor && (Actor->IsA<ARoxiWorldItem>() || Actor->IsA<ARoxiLootContainer>() || (Sleeper && Sleeper->IsSleeping())))
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
	else if (Target)
	{
		// Контейнер с добычей или спящий персонаж — открыть/закрыть.
		if (IsViewing(Target))
		{
			ServerCloseContainer();
		}
		else
		{
			ServerOpenContainer(Target);
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
		ARoxiPlayerController::NotifyPlayer(Cast<AController>(GetOwner()), FString::Printf(TEXT("Dropped %s"), *Removed.ToString()));
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
		ARoxiPlayerController::NotifyPlayer(Cast<AController>(GetOwner()), FString::Printf(TEXT("Picked up %s"), *Stack.ToString()));
	}
	WorldItem->SetQuantity(Left);
}

void URoxiItemInteractionComponent::ServerOpenContainer_Implementation(AActor* ContainerActor)
{
	// Контейнер с добычей — один контейнер; спящий персонаж (не свой) — его инвентарь и пояс (GDD §5.4).
	URoxiInventoryComponent* Inventory = nullptr;
	URoxiInventoryComponent* Second = nullptr;
	if (const ARoxiLootContainer* Container = Cast<ARoxiLootContainer>(ContainerActor))
	{
		Inventory = Container->GetInventory();
	}
	else if (const ARoxiCharacter* Sleeper = Cast<ARoxiCharacter>(ContainerActor))
	{
		if (Sleeper->IsSleeping() && Sleeper != GetControlledPawn())
		{
			Inventory = Sleeper->GetInventory();
			Second = Sleeper->GetBelt();
		}
	}
	if (!Inventory || !GetControlledPawn() || !IsInRange(ContainerActor))
	{
		return;
	}

	CloseContainer();
	ViewedContainer = Inventory;
	ViewedSlots.SyncFrom(Inventory->GetList());
	ViewedChangedHandle = Inventory->OnChanged.AddUObject(this, &URoxiItemInteractionComponent::HandleViewedContainerChanged);
	if (Second)
	{
		ViewedContainer2 = Second;
		ViewedSlots2.SyncFrom(Second->GetList());
		ViewedChangedHandle2 = Second->OnChanged.AddUObject(this, &URoxiItemInteractionComponent::HandleViewedContainerChanged);
	}
	UE_LOG(LogRoxi, Log, TEXT("%s opened %s"), *GetNameSafe(GetOwner()), *GetNameSafe(ContainerActor));
}

bool URoxiItemInteractionComponent::IsViewing(const AActor* Target) const
{
	return Target && ViewedContainer && ViewedContainer->GetOwner() == Target;
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
	PC->ClientShowMessage(FString::Printf(TEXT("Placed %s"), *Stack.Item->DisplayName.ToString()));
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
	if (ViewedContainer2)
	{
		ViewedContainer2->OnChanged.Remove(ViewedChangedHandle2);
	}
	ViewedChangedHandle.Reset();
	ViewedChangedHandle2.Reset();
	if (ViewedContainer || ViewedSlots.Num() > 0)
	{
		ViewedContainer = nullptr;
		ViewedSlots.Reset();
	}
	if (ViewedContainer2 || ViewedSlots2.Num() > 0)
	{
		ViewedContainer2 = nullptr;
		ViewedSlots2.Reset();
	}
}

void URoxiItemInteractionComponent::HandleViewedContainerChanged(URoxiInventoryComponent* Inventory)
{
	if (Inventory == ViewedContainer)
	{
		ViewedSlots.SyncFrom(Inventory->GetList());
	}
	else if (Inventory == ViewedContainer2)
	{
		ViewedSlots2.SyncFrom(Inventory->GetList());
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
	const ARoxiCharacter* Sleeper = IsValid(ViewedContainer) ? Cast<ARoxiCharacter>(ViewedContainer->GetOwner()) : nullptr;
	if (!IsValid(ViewedContainer) || !IsInRange(ViewedContainer->GetOwner())
		|| (Sleeper && !Sleeper->IsSleeping())) // владелец вошёл в игру — обыск прекращается
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
