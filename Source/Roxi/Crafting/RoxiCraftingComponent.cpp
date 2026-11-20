// Copyright Epic Games, Inc. All Rights Reserved.

#include "Crafting/RoxiCraftingComponent.h"
#include "Crafting/RoxiRecipeDefinition.h"
#include "Character/RoxiCharacter.h"
#include "Core/RoxiPlayerController.h"
#include "Items/RoxiInventoryComponent.h"
#include "Items/RoxiItemDefinition.h"
#include "Items/RoxiWorldItem.h"
#include "Roxi.h"
#include "Engine/World.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

URoxiCraftingComponent::URoxiCraftingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
	SetIsReplicatedByDefault(true);
}

void URoxiCraftingComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.Condition = COND_OwnerOnly;
	DOREPLIFETIME_WITH_PARAMS_FAST(URoxiCraftingComponent, Queue, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(URoxiCraftingComponent, CurrentJobEndTime, Params);
}

ARoxiCharacter* URoxiCraftingComponent::GetCharacter() const
{
	return Cast<ARoxiCharacter>(GetOwner());
}

void URoxiCraftingComponent::MarkQueueDirty()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(URoxiCraftingComponent, Queue, this);
	MARK_PROPERTY_DIRTY_FROM_NAME(URoxiCraftingComponent, CurrentJobEndTime, this);
}

bool URoxiCraftingComponent::TryConsumeIngredients(const URoxiRecipeDefinition* Recipe)
{
	return Recipe && Recipe->OutputItem && TryConsume(GetCharacter(), Recipe->Ingredients);
}

bool URoxiCraftingComponent::TryConsume(ARoxiCharacter* Character, const TArray<FRoxiItemAmount>& Cost)
{
	if (!Character || !Character->HasAuthority())
	{
		return false;
	}
	URoxiInventoryComponent* Inventory = Character->GetInventory();
	URoxiInventoryComponent* Belt = Character->GetBelt();

	// Сначала проверить всё, потом списывать — чтобы не списать часть.
	for (const FRoxiItemAmount& Entry : Cost)
	{
		if (!Entry.Item || Inventory->CountItem(Entry.Item) + Belt->CountItem(Entry.Item) < Entry.Quantity)
		{
			return false;
		}
	}
	for (const FRoxiItemAmount& Entry : Cost)
	{
		const int32 FromInventory = Inventory->RemoveItem(Entry.Item, Entry.Quantity);
		Belt->RemoveItem(Entry.Item, Entry.Quantity - FromInventory);
	}
	return true;
}

void URoxiCraftingComponent::GiveToOwner(URoxiItemDefinition* Item, int32 Quantity)
{
	ARoxiCharacter* Character = GetCharacter();
	if (!Character || !Item || Quantity <= 0)
	{
		return;
	}
	int32 Left = Character->GetInventory()->AddItem(Item, Quantity);
	Left = Character->GetBelt()->AddItem(Item, Left);
	if (Left > 0)
	{
		ARoxiWorldItem::SpawnItem(GetWorld(), FRoxiItemStack(Item, Left), Character->GetActorLocation());
	}
}

void URoxiCraftingComponent::Refund(const URoxiRecipeDefinition* Recipe)
{
	if (Recipe)
	{
		for (const FRoxiItemAmount& Ingredient : Recipe->Ingredients)
		{
			GiveToOwner(Ingredient.Item, Ingredient.Quantity);
		}
	}
}

void URoxiCraftingComponent::StartNextJob()
{
	const float Duration = (!Queue.IsEmpty() && Queue[0].Recipe) ? Queue[0].Recipe->CraftTime : 0.f;
	CurrentJobEndTime = Queue.IsEmpty() ? 0.0 : GetWorld()->GetTimeSeconds() + Duration;
	MarkQueueDirty();
}

void URoxiCraftingComponent::ServerCraft_Implementation(const URoxiRecipeDefinition* Recipe, int32 Count)
{
	if (!Recipe || Count <= 0)
	{
		return;
	}
	int32 Queued = 0;
	for (int32 i = 0; i < Count && Queue.Num() < MaxQueueLength; ++i)
	{
		if (!TryConsumeIngredients(Recipe))
		{
			break;
		}
		FRoxiCraftJob Job;
		Job.Recipe = Recipe;
		Queue.Add(Job);
		++Queued;
		if (Queue.Num() == 1)
		{
			StartNextJob();
		}
	}
	MarkQueueDirty();
	UE_LOG(LogRoxi, Log, TEXT("%s queued %d/%d x %s (queue %d)"),
		*GetNameSafe(GetOwner()), Queued, Count, *Recipe->GetName(), Queue.Num());
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		ARoxiPlayerController::NotifyPlayer(OwnerPawn->GetController(), Queued > 0
			? FString::Printf(TEXT("Crafting %d x %s"), Queued, *GetNameSafe(Recipe->OutputItem))
			: FString(TEXT("Cannot craft: not enough resources or queue full")));
	}
}

void URoxiCraftingComponent::ServerCancel_Implementation(int32 QueueIndex)
{
	if (!Queue.IsValidIndex(QueueIndex))
	{
		return;
	}
	const URoxiRecipeDefinition* Recipe = Queue[QueueIndex].Recipe;
	Queue.RemoveAt(QueueIndex);
	Refund(Recipe);
	if (QueueIndex == 0)
	{
		StartNextJob();
	}
	MarkQueueDirty();
	UE_LOG(LogRoxi, Log, TEXT("%s cancelled %s, refunded"), *GetNameSafe(GetOwner()), *GetNameSafe(Recipe));
}

void URoxiCraftingComponent::CancelAll()
{
	if (!GetOwner()->HasAuthority() || Queue.IsEmpty())
	{
		return;
	}
	TArray<FRoxiCraftJob> Jobs = MoveTemp(Queue);
	Queue.Reset();
	for (const FRoxiCraftJob& Job : Jobs)
	{
		Refund(Job.Recipe);
	}
	StartNextJob();
}

void URoxiCraftingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!GetOwner()->HasAuthority() || Queue.IsEmpty() || GetWorld()->GetTimeSeconds() < CurrentJobEndTime)
	{
		return;
	}

	const URoxiRecipeDefinition* Recipe = Queue[0].Recipe;
	Queue.RemoveAt(0);
	if (Recipe)
	{
		GiveToOwner(Recipe->OutputItem, Recipe->OutputQuantity);
		UE_LOG(LogRoxi, Log, TEXT("%s crafted %s x%d"), *GetNameSafe(GetOwner()),
			*GetNameSafe(Recipe->OutputItem), Recipe->OutputQuantity);
		if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
		{
			ARoxiPlayerController::NotifyPlayer(OwnerPawn->GetController(), FString::Printf(TEXT("Crafted %s"), *GetNameSafe(Recipe->OutputItem)));
		}
	}
	StartNextJob();
}
