// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dev/RoxiDebugHUD.h"
#include "Core/RoxiPlayerController.h"
#include "Items/RoxiInventoryComponent.h"
#include "Items/RoxiItemInteractionComponent.h"
#include "Items/RoxiLootContainer.h"
#include "Items/RoxiWorldItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

void ARoxiDebugHUD::DrawLine(const FString& Text, float& Y, const FLinearColor& Color)
{
	DrawText(Text, Color, 20.f, Y, GEngine->GetSmallFont());
	Y += 16.f;
}

void ARoxiDebugHUD::DrawHUD()
{
	Super::DrawHUD();

#if !UE_BUILD_SHIPPING
	const ARoxiPlayerController* PC = Cast<ARoxiPlayerController>(GetOwningPlayerController());
	const URoxiItemInteractionComponent* Interaction = PC ? PC->GetItemInteraction() : nullptr;
	if (!Interaction || !Canvas)
	{
		return;
	}

	float Y = 20.f;
	DrawLine(TEXT("ROXI DEBUG UI (temporary)"), Y, FLinearColor::Yellow);

	if (!PC->GetPawn())
	{
		DrawLine(TEXT("No character."), Y, FLinearColor::Red);
		return;
	}

	if (const URoxiInventoryComponent* Inventory = Interaction->GetPawnInventory())
	{
		DrawLine(FString::Printf(TEXT("Inventory: %s"), *Inventory->Describe()), Y);
	}
	if (const URoxiInventoryComponent* Belt = Interaction->GetPawnBelt())
	{
		DrawLine(FString::Printf(TEXT("Belt: %s"), *Belt->Describe()), Y);
	}
	if (Interaction->GetViewedContainer())
	{
		DrawLine(FString::Printf(TEXT("Open container: %s"), *Interaction->GetViewedSlots().Describe()), Y, FLinearColor::Green);
	}

	if (const AActor* Target = Interaction->FindNearestInteractable())
	{
		FString Prompt;
		if (const ARoxiWorldItem* WorldItem = Cast<ARoxiWorldItem>(Target))
		{
			Prompt = FString::Printf(TEXT("[E] Pick up %s"), *WorldItem->GetStack().ToString());
		}
		else if (const ARoxiLootContainer* Container = Cast<ARoxiLootContainer>(Target))
		{
			Prompt = FString::Printf(TEXT("[E] Open/close loot of %s"), *Container->GetOwnerLabel());
		}
		DrawLine(Prompt, Y, FLinearColor(0.4f, 0.8f, 1.f));
	}

	DrawLine(TEXT("Console: RoxiGive <Item> <N> | RoxiMove <inv|belt|loot> <slot> <inv|belt|loot> <slot> <N> | RoxiDrop <c> <slot> <N> | RoxiInv"), Y, FLinearColor::Gray);
#endif
}
