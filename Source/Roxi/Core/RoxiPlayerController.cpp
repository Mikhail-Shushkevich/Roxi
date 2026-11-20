// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/RoxiPlayerController.h"
#include "Items/RoxiInventoryComponent.h"
#include "Items/RoxiItemDefinition.h"
#include "Items/RoxiItemInteractionComponent.h"
#include "Roxi.h"

ARoxiPlayerController::ARoxiPlayerController()
{
	ItemInteraction = CreateDefaultSubobject<URoxiItemInteractionComponent>(TEXT("ItemInteraction"));
}

URoxiInventoryComponent* ARoxiPlayerController::ResolveContainer(const FString& Name) const
{
	if (Name.Equals(TEXT("inv"), ESearchCase::IgnoreCase))  { return ItemInteraction->GetPawnInventory(); }
	if (Name.Equals(TEXT("belt"), ESearchCase::IgnoreCase)) { return ItemInteraction->GetPawnBelt(); }
	if (Name.Equals(TEXT("loot"), ESearchCase::IgnoreCase)) { return ItemInteraction->GetViewedContainer(); }
	UE_LOG(LogRoxi, Warning, TEXT("Unknown container '%s' (use inv, belt, loot)"), *Name);
	return nullptr;
}

void ARoxiPlayerController::RoxiGive(const FString& ItemName, int32 Quantity)
{
	ServerDevGive(FName(*ItemName), Quantity);
}

void ARoxiPlayerController::ServerDevGive_Implementation(FName ItemName, int32 Quantity)
{
#if !UE_BUILD_SHIPPING
	URoxiInventoryComponent* Inventory = ItemInteraction->GetPawnInventory();
	URoxiItemDefinition* Item = URoxiItemDefinition::LoadByName(ItemName);
	if (!Inventory || !Item || Quantity <= 0)
	{
		UE_LOG(LogRoxi, Warning, TEXT("RoxiGive failed: item '%s' x%d"), *ItemName.ToString(), Quantity);
		return;
	}
	const int32 Left = Inventory->AddItem(Item, Quantity);
	UE_LOG(LogRoxi, Log, TEXT("RoxiGive: %s x%d to %s (did not fit: %d)"), *ItemName.ToString(), Quantity, *GetName(), Left);
#endif
}

void ARoxiPlayerController::RoxiInv()
{
	const URoxiInventoryComponent* Inventory = ItemInteraction->GetPawnInventory();
	const URoxiInventoryComponent* Belt = ItemInteraction->GetPawnBelt();
	UE_LOG(LogRoxi, Display, TEXT("INV %s inv: %s"), *GetName(), Inventory ? *Inventory->Describe() : TEXT("(no pawn)"));
	UE_LOG(LogRoxi, Display, TEXT("INV %s belt: %s"), *GetName(), Belt ? *Belt->Describe() : TEXT("(no pawn)"));
	UE_LOG(LogRoxi, Display, TEXT("INV %s loot: %s"), *GetName(),
		ItemInteraction->GetViewedContainer() ? *ItemInteraction->GetViewedSlots().Describe() : TEXT("(not open)"));
}

void ARoxiPlayerController::RoxiMove(const FString& From, int32 FromSlot, const FString& To, int32 ToSlot, int32 Quantity)
{
	ItemInteraction->ServerMoveItem(ResolveContainer(From), FromSlot, ResolveContainer(To), ToSlot, Quantity);
}

void ARoxiPlayerController::RoxiDrop(const FString& From, int32 Slot, int32 Quantity)
{
	ItemInteraction->ServerDropItem(ResolveContainer(From), Slot, Quantity);
}

void ARoxiPlayerController::RoxiInteract()
{
	ItemInteraction->InteractNearest();
}
