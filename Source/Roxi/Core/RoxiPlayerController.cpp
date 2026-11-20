// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/RoxiPlayerController.h"
#include "Building/RoxiSleepingBag.h"
#include "Building/RoxiBuildingPiece.h"
#include "Building/RoxiBuildPieceDefinition.h"
#include "Character/RoxiCharacter.h"
#include "Core/RoxiGameMode.h"
#include "Crafting/RoxiCraftingComponent.h"
#include "Crafting/RoxiRecipeDefinition.h"
#include "Items/RoxiInventoryComponent.h"
#include "Items/RoxiItemDefinition.h"
#include "Items/RoxiItemInteractionComponent.h"
#include "Survival/RoxiHealthComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "Roxi.h"

ARoxiPlayerController::ARoxiPlayerController()
{
	ItemInteraction = CreateDefaultSubobject<URoxiItemInteractionComponent>(TEXT("ItemInteraction"));
}

void ARoxiPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.Condition = COND_OwnerOnly;
	DOREPLIFETIME_WITH_PARAMS_FAST(ARoxiPlayerController, RespawnPoints, Params);
}

FString ARoxiPlayerController::GetPersistentPlayerId() const
{
	return (PlayerState && PlayerState->GetUniqueId().IsValid()) ? PlayerState->GetUniqueId().ToString() : FString();
}

void ARoxiPlayerController::RefreshRespawnPoints()
{
	if (!HasAuthority())
	{
		return;
	}
	RespawnPoints.Reset();
	for (const ARoxiSleepingBag* Point : ARoxiSleepingBag::FindByOwner(GetWorld(), GetPersistentPlayerId()))
	{
		FRoxiRespawnPointInfo& Info = RespawnPoints.AddDefaulted_GetRef();
		Info.Id = Point->GetPointId();
		Info.DisplayName = Point->GetDisplayName();
		Info.Location = Point->GetActorLocation();
		Info.AvailableAt = Point->GetAvailableAt();
	}
	MARK_PROPERTY_DIRTY_FROM_NAME(ARoxiPlayerController, RespawnPoints, this);
}

void ARoxiPlayerController::ClientShowMessage_Implementation(const FString& Message)
{
	LastMessage = Message;
	LastMessageTime = FPlatformTime::Seconds();
	UE_LOG(LogRoxi, Display, TEXT("MSG %s: %s"), *GetName(), *Message);
#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 6.f, FColor::Yellow, Message);
	}
#endif
}

void ARoxiPlayerController::NotifyPlayer(const AController* Controller, const FString& Message)
{
	if (ARoxiPlayerController* PC = const_cast<ARoxiPlayerController*>(Cast<ARoxiPlayerController>(Controller)))
	{
		PC->ClientShowMessage(Message);
	}
}

void ARoxiPlayerController::ServerRequestRespawn_Implementation(int32 PointId)
{
	if (ARoxiGameMode* GameMode = GetWorld()->GetAuthGameMode<ARoxiGameMode>())
	{
		GameMode->RequestRespawn(this, PointId);
	}
}

URoxiInventoryComponent* ARoxiPlayerController::ResolveContainer(const FString& Name) const
{
	if (Name.Equals(TEXT("inv"), ESearchCase::IgnoreCase))  { return ItemInteraction->GetPawnInventory(); }
	if (Name.Equals(TEXT("belt"), ESearchCase::IgnoreCase)) { return ItemInteraction->GetPawnBelt(); }
	if (Name.Equals(TEXT("loot"), ESearchCase::IgnoreCase)) { return ItemInteraction->GetViewedContainer(); }
	UE_LOG(LogRoxi, Warning, TEXT("Unknown container '%s' (use inv, belt, loot)"), *Name);
	return nullptr;
}

// ---------------------------------------------------------------------------
// Консольные команды

void ARoxiPlayerController::RoxiGive(const FString& ItemName, int32 Quantity)
{
	ServerDevGive(FName(*ItemName), Quantity);
}

void ARoxiPlayerController::RoxiInv()
{
	if (const ARoxiCharacter* MyCharacter = Cast<ARoxiCharacter>(GetPawn()))
	{
		UE_LOG(LogRoxi, Display, TEXT("INV %s health: %.0f / %.0f"), *GetName(),
			MyCharacter->GetHealth()->GetHealth(), MyCharacter->GetHealth()->GetMaxHealth());

		TArray<FString> Jobs;
		for (const FRoxiCraftJob& Job : MyCharacter->GetCrafting()->GetQueue())
		{
			Jobs.Add(GetNameSafe(Job.Recipe.Get()));
		}
		UE_LOG(LogRoxi, Display, TEXT("INV %s craft queue: %s"), *GetName(), Jobs.IsEmpty() ? TEXT("(empty)") : *FString::Join(Jobs, TEXT(", ")));
	}
	const URoxiInventoryComponent* Inventory = ItemInteraction->GetPawnInventory();
	const URoxiInventoryComponent* Belt = ItemInteraction->GetPawnBelt();
	UE_LOG(LogRoxi, Display, TEXT("INV %s inv: %s"), *GetName(), Inventory ? *Inventory->Describe() : TEXT("(no pawn)"));
	UE_LOG(LogRoxi, Display, TEXT("INV %s belt: %s"), *GetName(), Belt ? *Belt->Describe() : TEXT("(no pawn)"));
	UE_LOG(LogRoxi, Display, TEXT("INV %s loot: %s"), *GetName(),
		ItemInteraction->GetViewedContainer() ? *ItemInteraction->GetViewedSlots().Describe() : TEXT("(not open)"));

	const AGameStateBase* GS = GetWorld()->GetGameState();
	const double Now = GS ? GS->GetServerWorldTimeSeconds() : 0.0;
	TArray<FString> Points;
	for (const FRoxiRespawnPointInfo& Info : RespawnPoints)
	{
		const double Wait = FMath::Max(0.0, Info.AvailableAt - Now);
		Points.Add(FString::Printf(TEXT("#%d %s %s"), Info.Id, *Info.DisplayName.ToString(),
			Wait > 0.0 ? *FString::Printf(TEXT("(cooldown %.0fs)"), Wait) : TEXT("(ready)")));
	}
	UE_LOG(LogRoxi, Display, TEXT("INV %s respawn points: %s"), *GetName(), Points.IsEmpty() ? TEXT("(none)") : *FString::Join(Points, TEXT(", ")));

#if !UE_BUILD_SHIPPING
	// То же на экран (для ручной проверки в редакторе).
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Cyan, FString::Printf(TEXT("Respawn points: %s"),
			Points.IsEmpty() ? TEXT("(none)") : *FString::Join(Points, TEXT(", "))));
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Cyan, FString::Printf(TEXT("Belt: %s"), Belt ? *Belt->Describe() : TEXT("(no character)")));
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Cyan, FString::Printf(TEXT("Inventory: %s"), Inventory ? *Inventory->Describe() : TEXT("(no character)")));
	}
#endif
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

void ARoxiPlayerController::RoxiRespawn(int32 PointId)
{
	ServerRequestRespawn(PointId);
}

void ARoxiPlayerController::RoxiDamage(float Amount)
{
	ServerDevDamage(Amount);
}

void ARoxiPlayerController::RoxiHit(float Amount)
{
	ServerDevHit(Amount);
}

void ARoxiPlayerController::RoxiCraft(const FString& RecipeName, int32 Count)
{
	const ARoxiCharacter* MyCharacter = Cast<ARoxiCharacter>(GetPawn());
	URoxiRecipeDefinition* Recipe = URoxiRecipeDefinition::LoadByName(FName(*RecipeName));
	if (!MyCharacter || !Recipe)
	{
		UE_LOG(LogRoxi, Warning, TEXT("RoxiCraft: no character or unknown recipe '%s'"), *RecipeName);
		return;
	}
	MyCharacter->GetCrafting()->ServerCraft(Recipe, FMath::Max(1, Count));
}

void ARoxiPlayerController::RoxiCraftCancel(int32 QueueIndex)
{
	if (const ARoxiCharacter* MyCharacter = Cast<ARoxiCharacter>(GetPawn()))
	{
		MyCharacter->GetCrafting()->ServerCancel(QueueIndex);
	}
}

void ARoxiPlayerController::RoxiPlace(const FString& From, int32 Slot)
{
	if (const APawn* MyPawn = GetPawn())
	{
		const FVector Location = MyPawn->GetActorLocation() + MyPawn->GetActorForwardVector() * 200.f;
		ItemInteraction->ServerPlaceDeployable(ResolveContainer(From), Slot, Location, MyPawn->GetActorRotation().Yaw);
	}
}

void ARoxiPlayerController::RoxiBuild(const FString& PieceName)
{
	const APawn* MyPawn = GetPawn();
	const URoxiBuildPieceDefinition* Definition = URoxiBuildPieceDefinition::LoadByName(FName(*PieceName));
	if (!MyPawn || !Definition)
	{
		UE_LOG(LogRoxi, Warning, TEXT("RoxiBuild: no character or unknown piece '%s'"), *PieceName);
		return;
	}

	// Прицел: точка перед игроком; для перекрытий — на высоте верха стен.
	const FVector Forward = MyPawn->GetActorForwardVector();
	FVector Aim = MyPawn->GetActorLocation() + Forward * RoxiBuildGrid::CellSize;
	if (Definition->PieceType == ERoxiPieceType::Floor)
	{
		Aim.Z += RoxiBuildGrid::WallHeight;
	}

	FRoxiBuildSocket Socket;
	if (!ARoxiBuildingPiece::FindPlacement(GetWorld(), Definition->PieceType, Aim, MyPawn->GetActorLocation(),
		MyPawn->GetActorRotation().Yaw, Socket))
	{
		ClientShowMessage_Implementation(TEXT("No place to build here"));
		return;
	}
	ServerBuild(Definition, Socket.Location, Socket.Yaw);
}

void ARoxiPlayerController::RoxiBuildAt(const FString& PieceName, float X, float Y, float Z, float Yaw)
{
	if (const APawn* MyPawn = GetPawn())
	{
		ServerBuild(URoxiBuildPieceDefinition::LoadByName(FName(*PieceName)), MyPawn->GetActorLocation() + FVector(X, Y, Z), Yaw);
	}
}

void ARoxiPlayerController::ServerBuild_Implementation(const URoxiBuildPieceDefinition* Definition, FVector Location, float Yaw)
{
	ARoxiCharacter* MyCharacter = Cast<ARoxiCharacter>(GetPawn());
	if (!MyCharacter || !Definition)
	{
		return;
	}

	FRoxiBuildSocket Socket;
	Socket.Location = Location;
	Socket.Yaw = Yaw;
	FString Reason;
	if (!ARoxiBuildingPiece::ValidatePlacement(Definition, MyCharacter, Socket, Reason))
	{
		ClientShowMessage(FString::Printf(TEXT("Cannot build %s: %s"), *Definition->GetName(), *Reason));
		return;
	}
	if (!URoxiCraftingComponent::TryConsume(MyCharacter, Definition->Cost))
	{
		ClientShowMessage(FString::Printf(TEXT("Cannot build %s: not enough resources"), *Definition->GetName()));
		return;
	}
	const ARoxiBuildingPiece* Piece = ARoxiBuildingPiece::SpawnPiece(GetWorld(), Definition, Socket, GetPersistentPlayerId());
	UE_LOG(LogRoxi, Log, TEXT("%s built %s at %s yaw %.0f"), *GetName(), *GetNameSafe(Piece), *Location.ToCompactString(), Yaw);
	ClientShowMessage(FString::Printf(TEXT("Built %s"), *Definition->DisplayName.ToString()));
}

// ---------------------------------------------------------------------------
// Серверная часть dev-команд

void ARoxiPlayerController::ServerDevGive_Implementation(FName ItemName, int32 Quantity)
{
#if !UE_BUILD_SHIPPING
	URoxiInventoryComponent* Inventory = ItemInteraction->GetPawnInventory();
	URoxiItemDefinition* Item = URoxiItemDefinition::LoadByName(ItemName);
	if (!Inventory || !Item || Quantity <= 0)
	{
		UE_LOG(LogRoxi, Warning, TEXT("RoxiGive failed: item '%s' x%d"), *ItemName.ToString(), Quantity);
		ClientShowMessage(FString::Printf(TEXT("RoxiGive: unknown item '%s'"), *ItemName.ToString()));
		return;
	}
	const int32 Left = Inventory->AddItem(Item, Quantity);
	UE_LOG(LogRoxi, Log, TEXT("RoxiGive: %s x%d to %s (did not fit: %d)"), *ItemName.ToString(), Quantity, *GetName(), Left);
	ClientShowMessage(FString::Printf(TEXT("Received %s x%d%s"), *Item->DisplayName.ToString(), Quantity - Left,
		Left > 0 ? *FString::Printf(TEXT(" (no room for %d)"), Left) : TEXT("")));
#endif
}

void ARoxiPlayerController::ServerDevDamage_Implementation(float Amount)
{
#if !UE_BUILD_SHIPPING
	if (APawn* MyPawn = GetPawn())
	{
		UGameplayStatics::ApplyDamage(MyPawn, Amount, this, nullptr, UDamageType::StaticClass());
	}
#endif
}

void ARoxiPlayerController::ServerDevHit_Implementation(float Amount)
{
#if !UE_BUILD_SHIPPING
	APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return;
	}
	// Ближайший актор со здоровьем (кроме себя) в радиусе взаимодействия.
	TArray<FOverlapResult> Overlaps;
	FCollisionObjectQueryParams Objects(FCollisionObjectQueryParams::AllObjects);
	GetWorld()->OverlapMultiByObjectType(Overlaps, MyPawn->GetActorLocation(), FQuat::Identity, Objects,
		FCollisionShape::MakeSphere(URoxiItemInteractionComponent::GetInteractRange()),
		FCollisionQueryParams(SCENE_QUERY_STAT(RoxiDevHit), false, MyPawn));

	AActor* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (Actor && !Actor->IsA<APawn>() && Actor->FindComponentByClass<URoxiHealthComponent>())
		{
			const float DistSq = FVector::DistSquared(MyPawn->GetActorLocation(), Actor->GetActorLocation());
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Best = Actor;
			}
		}
	}
	if (Best)
	{
		UE_LOG(LogRoxi, Log, TEXT("RoxiHit: %s hits %s for %.0f"), *GetName(), *Best->GetName(), Amount);
		UGameplayStatics::ApplyDamage(Best, Amount, this, MyPawn, UDamageType::StaticClass());
	}
#endif
}
