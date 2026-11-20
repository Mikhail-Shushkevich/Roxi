// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dev/RoxiDebugHUD.h"
#include "Core/RoxiPlayerController.h"
#include "Character/RoxiCharacter.h"
#include "Survival/RoxiHealthComponent.h"
#include "Crafting/RoxiCraftingComponent.h"
#include "Crafting/RoxiRecipeDefinition.h"
#include "GameFramework/GameStateBase.h"
#include "Items/RoxiInventoryComponent.h"
#include "Items/RoxiItemInteractionComponent.h"
#include "Items/RoxiLootContainer.h"
#include "Items/RoxiWorldItem.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Engine.h"

void ARoxiDebugHUD::DrawLine(const FString& Text, float& Y, const FLinearColor& Color)
{
	// Крупный шрифт с обводкой на полупрозрачной подложке — читается на любом фоне.
	UFont* Font = GEngine->GetMediumFont();
	float Width = 0.f, Height = 0.f;
	Canvas->TextSize(Font, Text, Width, Height);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), 14.f, Y - 2.f, Width + 12.f, Height + 4.f);

	FCanvasTextItem Item(FVector2D(20.f, Y), FText::FromString(Text), Font, Color);
	Item.bOutlined = true;
	Item.OutlineColor = FLinearColor::Black;
	Canvas->DrawItem(Item);
	Y += Height + 6.f;
}

void ARoxiDebugHUD::DrawMessage(const ARoxiPlayerController* PC, float& Y)
{
	if (!PC->GetLastMessage().IsEmpty() && FPlatformTime::Seconds() - PC->GetLastMessageTime() < 5.0)
	{
		DrawLine(PC->GetLastMessage(), Y, FLinearColor(1.f, 0.4f, 0.4f));
	}
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

	// Ниже середины экрана: верх занимают экранные сообщения движка (результаты команд).
	float Y = Canvas->ClipY * 0.5f;
	DrawLine(TEXT("ROXI DEBUG UI (temporary)"), Y, FLinearColor::Yellow);

	if (!PC->GetPawn())
	{
		DrawLine(TEXT("You are dead. Console: RoxiRespawn <id>   (0 = random point)"), Y, FLinearColor::Red);
		const AGameStateBase* GS = GetWorld()->GetGameState();
		const double Now = GS ? GS->GetServerWorldTimeSeconds() : 0.0;
		for (const FRoxiRespawnPointInfo& Info : PC->GetRespawnPoints())
		{
			const double Wait = FMath::Max(0.0, Info.AvailableAt - Now);
			DrawLine(FString::Printf(TEXT("  #%d %s  %s"), Info.Id, *Info.DisplayName.ToString(),
				Wait > 0.0 ? *FString::Printf(TEXT("cooldown %.0fs"), Wait) : TEXT("ready")), Y);
		}
		DrawMessage(PC, Y);
		return;
	}

	if (const ARoxiCharacter* MyCharacter = Cast<ARoxiCharacter>(PC->GetPawn()))
	{
		const URoxiHealthComponent* Health = MyCharacter->GetHealth();
		DrawLine(FString::Printf(TEXT("Health: %.0f / %.0f"), Health->GetHealth(), Health->GetMaxHealth()), Y);

		const URoxiCraftingComponent* Crafting = MyCharacter->GetCrafting();
		if (!Crafting->GetQueue().IsEmpty())
		{
			const AGameStateBase* GS = GetWorld()->GetGameState();
			const double Left = GS ? FMath::Max(0.0, Crafting->GetCurrentJobEndTime() - GS->GetServerWorldTimeSeconds()) : 0.0;
			TArray<FString> Jobs;
			for (const FRoxiCraftJob& Job : Crafting->GetQueue())
			{
				Jobs.Add(GetNameSafe(Job.Recipe.Get()));
			}
			DrawLine(FString::Printf(TEXT("Crafting (%.1fs): %s"), Left, *FString::Join(Jobs, TEXT(", "))), Y, FLinearColor(1.f, 0.7f, 0.3f));
		}
	}
	DrawMessage(PC, Y);

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
	if (Interaction->GetViewedContainer2())
	{
		DrawLine(FString::Printf(TEXT("Open container 2 (loot2): %s"), *Interaction->GetViewedSlots2().Describe()), Y, FLinearColor::Green);
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
		else if (const ARoxiCharacter* Sleeper = Cast<ARoxiCharacter>(Target))
		{
			Prompt = FString::Printf(TEXT("[E] Search sleeping %s"), *Sleeper->GetSleeperName());
		}
		DrawLine(Prompt, Y, FLinearColor(0.4f, 0.8f, 1.f));
	}

	DrawLine(TEXT("Console: RoxiGive <Item> <N> | RoxiMove <inv|belt|loot> <slot> <inv|belt|loot> <slot> <N> | RoxiDrop <c> <slot> <N> | RoxiDamage <N> | RoxiHit <N> | RoxiCraft <Recipe> <N> | RoxiPlace <c> <slot> | RoxiInv"), Y, FLinearColor::Gray);
#endif
}
