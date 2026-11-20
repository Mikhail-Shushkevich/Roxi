// Copyright Epic Games, Inc. All Rights Reserved.

#include "Save/RoxiPersistenceSubsystem.h"
#include "Save/RoxiSaveStorage.h"
#include "Building/RoxiBuildingPiece.h"
#include "Building/RoxiBuildPieceDefinition.h"
#include "Building/RoxiDeployable.h"
#include "Building/RoxiSleepingBag.h"
#include "Character/RoxiCharacter.h"
#include "Core/RoxiGameMode.h"
#include "Core/RoxiPlayerController.h"
#include "Items/RoxiInventoryComponent.h"
#include "Items/RoxiItemDefinition.h"
#include "Items/RoxiLootContainer.h"
#include "Items/RoxiWorldItem.h"
#include "Survival/RoxiHealthComponent.h"
#include "Roxi.h"
#include "Async/Async.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace
{
	TAutoConsoleVariable<float> CVarSaveInterval(TEXT("roxi.Save.Interval"), 300.f,
		TEXT("World autosave interval, seconds (0 = off). Applied at world start."));
	TAutoConsoleVariable<bool> CVarSaveInPIE(TEXT("roxi.Save.InPIE"), false,
		TEXT("Enable world saving in Play-In-Editor (player IDs are random there)."));

	const TCHAR* WorldRecord = TEXT("World");
	const TCHAR* ProfilesRecord = TEXT("Profiles");

	template<typename T>
	T* LoadDefinition(const FPrimaryAssetId& Id)
	{
		const FSoftObjectPath Path = UAssetManager::Get().GetPrimaryAssetPath(Id);
		return Path.IsValid() ? Cast<T>(Path.TryLoad()) : nullptr;
	}
}

bool URoxiPersistenceSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void URoxiPersistenceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (!FParse::Value(FCommandLine::Get(), TEXT("RoxiWorld="), WorldName) || WorldName.IsEmpty())
	{
		WorldName = TEXT("default");
	}
	TearDownHandle = FWorldDelegates::OnWorldBeginTearDown.AddUObject(this, &URoxiPersistenceSubsystem::HandleBeginTearDown);
}

void URoxiPersistenceSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldBeginTearDown.Remove(TearDownHandle);
	if (PendingWrite.IsValid())
	{
		PendingWrite.Wait();
	}
	Super::Deinitialize();
}

bool URoxiPersistenceSubsystem::IsActive() const
{
	const UWorld* World = GetWorld();
	const ENetMode NetMode = World ? World->GetNetMode() : NM_Client;
	if (NetMode != NM_ListenServer && NetMode != NM_DedicatedServer)
	{
		return false; // клиент или одиночный режим (например, клиент после отключения от сервера)
	}
	return !World->IsPlayInEditor() || CVarSaveInPIE.GetValueOnGameThread();
}

void URoxiPersistenceSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Загрузка — на следующем тике: к этому моменту у всех акторов уровня прошёл BeginPlay,
	// и восстановленным объектам не перезапишут состояние их собственные BeginPlay.
	InWorld.GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		EnsureLoaded();
		if (ARoxiGameMode* GameMode = GetWorld()->GetAuthGameMode<ARoxiGameMode>())
		{
			GameMode->OnWorldLoaded();
		}
	}));

	const float Interval = CVarSaveInterval.GetValueOnGameThread();
	if (IsActive() && Interval > 0.f)
	{
		InWorld.GetTimerManager().SetTimer(AutosaveTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { SaveNow(false); }),
			Interval, /*bLoop*/ true);
	}
}

void URoxiPersistenceSubsystem::HandleBeginTearDown(UWorld* InWorld)
{
	if (InWorld == GetWorld() && bLoaded)
	{
		SaveNow(/*bBlocking*/ true);
	}
}

// ---------------------------------------------------------------------------
// Загрузка

void URoxiPersistenceSubsystem::EnsureLoaded()
{
	if (bLoaded || bLoading || !IsActive())
	{
		return;
	}
	bLoading = true;
	Storage = MakeUnique<FRoxiFileSaveStorage>(WorldName);
	const FString MapName = UWorld::RemovePIEPrefix(GetWorld()->GetMapName());

	// Профили
	TArray<uint8> Bytes;
	FString Source;
	auto IsProfileSave = [](const TArray<uint8>& Data) { return Cast<URoxiProfileSave>(UGameplayStatics::LoadGameFromMemory(Data)) != nullptr; };
	Profiles = Storage->Read(ProfilesRecord, IsProfileSave, Bytes, Source)
		? Cast<URoxiProfileSave>(UGameplayStatics::LoadGameFromMemory(Bytes)) : nullptr;
	if (!Profiles)
	{
		Profiles = NewObject<URoxiProfileSave>(this);
	}

	// Мир
	auto IsWorldSave = [](const TArray<uint8>& Data) { return Cast<URoxiWorldSave>(UGameplayStatics::LoadGameFromMemory(Data)) != nullptr; };
	Bytes.Reset();
	if (Storage->Read(WorldRecord, IsWorldSave, Bytes, Source))
	{
		const URoxiWorldSave* Save = Cast<URoxiWorldSave>(UGameplayStatics::LoadGameFromMemory(Bytes));
		if (Save->Version != URoxiWorldSave::CurrentVersion || Save->MapName != MapName)
		{
			bSaveBlocked = true;
			UE_LOG(LogRoxi, Error, TEXT("Save: %s is for map '%s' v%d, current map '%s' v%d — NOT loaded, saving disabled to protect it"),
				*Source, *Save->MapName, Save->Version, *MapName, URoxiWorldSave::CurrentVersion);
		}
		else
		{
			const double Start = FPlatformTime::Seconds();
			ApplyWorld(*Save);
			UE_LOG(LogRoxi, Log, TEXT("Save: loaded world '%s' from %s (saved %s): %d pieces, %d deployables, %d containers, %d items, %d characters, %d dead — %.1f ms"),
				*WorldName, *Source, *Save->SavedAt.ToString(), Save->Pieces.Num(), Save->Deployables.Num(), Save->Containers.Num(),
				Save->WorldItems.Num(), Save->Characters.Num(), Save->DeadPlayers.Num(), (FPlatformTime::Seconds() - Start) * 1000.0);
		}
	}
	else if (IFileManager::Get().FileExists(*FPaths::Combine(Storage->GetDirectory(), FString(WorldRecord) + TEXT(".sav"))))
	{
		bSaveBlocked = true;
		UE_LOG(LogRoxi, Error, TEXT("Save: world '%s' exists but no readable copy found — NOT loaded, saving disabled to protect files"), *WorldName);
	}
	else
	{
		UE_LOG(LogRoxi, Log, TEXT("Save: no saved world '%s' — starting fresh (%s)"), *WorldName, *Storage->GetDirectory());
	}

	bLoading = false;
	bLoaded = true;
}

bool URoxiPersistenceSubsystem::ResolveStack(const FRoxiSavedStack& Saved, FRoxiItemStack& Out)
{
	URoxiItemDefinition* Item = LoadDefinition<URoxiItemDefinition>(Saved.Item);
	if (!Item || Saved.Quantity <= 0)
	{
		UE_LOG(LogRoxi, Warning, TEXT("Save: item '%s' no longer exists — skipped"), *Saved.Item.ToString());
		return false;
	}
	Out = FRoxiItemStack(Item, Saved.Quantity);
	return true;
}

void URoxiPersistenceSubsystem::LoadInventory(URoxiInventoryComponent* Inventory, const TArray<FRoxiSavedStack>& Stacks)
{
	for (const FRoxiSavedStack& Saved : Stacks)
	{
		FRoxiItemStack Stack;
		if (ResolveStack(Saved, Stack))
		{
			Inventory->SetSlot(Saved.Slot, Stack);
		}
	}
}

void URoxiPersistenceSubsystem::ApplyWorld(const URoxiWorldSave& Save)
{
	UWorld* World = GetWorld();
	ARoxiGameMode* GameMode = World->GetAuthGameMode<ARoxiGameMode>();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Описания построек — один раз на тип, не на каждый элемент.
	TArray<const URoxiBuildPieceDefinition*> PieceDefinitions;
	for (const FPrimaryAssetId& Id : Save.PieceDefinitions)
	{
		const URoxiBuildPieceDefinition* Definition = LoadDefinition<URoxiBuildPieceDefinition>(Id);
		if (!Definition)
		{
			UE_LOG(LogRoxi, Warning, TEXT("Save: building piece '%s' no longer exists — its pieces are skipped"), *Id.ToString());
		}
		PieceDefinitions.Add(Definition);
	}
	for (const FRoxiSavedPiece& Saved : Save.Pieces)
	{
		const URoxiBuildPieceDefinition* Definition = PieceDefinitions.IsValidIndex(Saved.DefinitionIndex) ? PieceDefinitions[Saved.DefinitionIndex] : nullptr;
		if (!Definition)
		{
			continue;
		}
		FRoxiBuildSocket Socket;
		Socket.Location = FVector(Saved.Location);
		Socket.Yaw = Saved.Yaw;
		const FString& OwnerId = Save.Owners.IsValidIndex(Saved.OwnerIndex) ? Save.Owners[Saved.OwnerIndex] : FString();
		if (ARoxiBuildingPiece* Piece = ARoxiBuildingPiece::SpawnPiece(World, Definition, Socket, OwnerId))
		{
			Piece->GetHealth()->SetCurrentHealth(Saved.Health);
		}
	}

	for (const FRoxiSavedDeployable& Saved : Save.Deployables)
	{
		UClass* Class = Saved.Class.LoadSynchronous();
		if (!Class)
		{
			UE_LOG(LogRoxi, Warning, TEXT("Save: deployable class '%s' no longer exists — skipped"), *Saved.Class.ToString());
			continue;
		}
		ARoxiDeployable* Deployable = World->SpawnActorDeferred<ARoxiDeployable>(Class, Saved.Transform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		Deployable->InitOwner(Saved.OwnerId);
		Deployable->FinishSpawning(Saved.Transform);
		Deployable->GetHealth()->SetCurrentHealth(Saved.Health);
		if (ARoxiSleepingBag* Bag = Cast<ARoxiSleepingBag>(Deployable))
		{
			Bag->SetCooldownRemaining(Saved.CooldownRemaining);
		}
	}

	for (const FRoxiSavedContainer& Saved : Save.Containers)
	{
		if (ARoxiLootContainer* Container = World->SpawnActor<ARoxiLootContainer>(ARoxiLootContainer::StaticClass(), Saved.Transform, Params))
		{
			Container->SetOwnerLabel(Saved.Label);
			Container->GetInventory()->SetNumSlots(FMath::Max(1, Saved.NumSlots));
			LoadInventory(Container->GetInventory(), Saved.Items);
		}
	}

	for (const FRoxiSavedWorldItem& Saved : Save.WorldItems)
	{
		FRoxiItemStack Stack;
		if (ResolveStack(Saved.Stack, Stack))
		{
			ARoxiWorldItem::SpawnItem(World, Stack, Saved.Transform.GetLocation());
		}
	}

	// Персонажи — все спящие, пока владельцы не войдут (GDD §5.4).
	UClass* PawnClass = GameMode ? GameMode->DefaultPawnClass.Get() : nullptr;
	FActorSpawnParameters PawnParams;
	PawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	for (const FRoxiSavedCharacter& Saved : Save.Characters)
	{
		ARoxiCharacter* Character = PawnClass ? World->SpawnActor<ARoxiCharacter>(PawnClass, Saved.Transform, PawnParams) : nullptr;
		if (!Character)
		{
			UE_LOG(LogRoxi, Warning, TEXT("Save: could not restore character of %s"), *Saved.PlayerId);
			continue;
		}
		LoadInventory(Character->GetInventory(), Saved.Inventory);
		LoadInventory(Character->GetBelt(), Saved.Belt);
		Character->GetHealth()->SetCurrentHealth(Saved.Health);
		Character->EnterSleep(Saved.PlayerId, Saved.PlayerName);
	}

	if (GameMode)
	{
		GameMode->SetDeadPlayers(Save.DeadPlayers);
	}
}

// ---------------------------------------------------------------------------
// Сохранение

FRoxiSavedStack URoxiPersistenceSubsystem::MakeSavedStack(int32 Slot, const FRoxiItemStack& Stack)
{
	FRoxiSavedStack Saved;
	Saved.Slot = Slot;
	Saved.Item = Stack.Item->GetPrimaryAssetId();
	Saved.Quantity = Stack.Quantity;
	return Saved;
}

void URoxiPersistenceSubsystem::SaveInventory(const URoxiInventoryComponent* Inventory, TArray<FRoxiSavedStack>& Out)
{
	for (int32 i = 0; i < Inventory->GetNumSlots(); ++i)
	{
		const FRoxiItemStack Stack = Inventory->GetStack(i);
		if (!Stack.IsEmpty())
		{
			Out.Add(MakeSavedStack(i, Stack));
		}
	}
}

URoxiWorldSave* URoxiPersistenceSubsystem::GatherWorld() const
{
	UWorld* World = GetWorld();
	URoxiWorldSave* Save = NewObject<URoxiWorldSave>();
	Save->Version = URoxiWorldSave::CurrentVersion;
	Save->MapName = UWorld::RemovePIEPrefix(World->GetMapName());
	Save->SavedAt = FDateTime::Now();

	TMap<const URoxiBuildPieceDefinition*, int32> DefinitionIndices;
	TMap<FString, int32> OwnerIndices;
	for (TActorIterator<ARoxiBuildingPiece> It(World); It; ++It)
	{
		const URoxiBuildPieceDefinition* Definition = It->GetDefinition();
		if (It->IsActorBeingDestroyed() || !Definition)
		{
			continue;
		}
		int32* DefinitionIndex = DefinitionIndices.Find(Definition);
		if (!DefinitionIndex)
		{
			DefinitionIndex = &DefinitionIndices.Add(Definition, Save->PieceDefinitions.Add(Definition->GetPrimaryAssetId()));
		}
		int32* OwnerIndex = OwnerIndices.Find(It->GetOwnerId());
		if (!OwnerIndex)
		{
			OwnerIndex = &OwnerIndices.Add(It->GetOwnerId(), Save->Owners.Add(It->GetOwnerId()));
		}

		FRoxiSavedPiece& Saved = Save->Pieces.AddDefaulted_GetRef();
		Saved.DefinitionIndex = *DefinitionIndex;
		Saved.Location = FVector3f(It->GetActorLocation());
		Saved.Yaw = It->GetActorRotation().Yaw;
		Saved.Health = It->GetHealth()->GetHealth();
		Saved.OwnerIndex = *OwnerIndex;
	}

	for (TActorIterator<ARoxiDeployable> It(World); It; ++It)
	{
		if (It->IsActorBeingDestroyed())
		{
			continue;
		}
		FRoxiSavedDeployable& Saved = Save->Deployables.AddDefaulted_GetRef();
		Saved.Class = It->GetClass();
		Saved.Transform = It->GetActorTransform();
		Saved.Health = It->GetHealth()->GetHealth();
		Saved.OwnerId = It->GetOwnerId();
		if (const ARoxiSleepingBag* Bag = Cast<ARoxiSleepingBag>(*It))
		{
			Saved.CooldownRemaining = Bag->GetCooldownRemaining();
		}
	}

	for (TActorIterator<ARoxiLootContainer> It(World); It; ++It)
	{
		if (It->IsActorBeingDestroyed())
		{
			continue;
		}
		FRoxiSavedContainer& Saved = Save->Containers.AddDefaulted_GetRef();
		Saved.Transform = It->GetActorTransform();
		Saved.Label = It->GetOwnerLabel();
		Saved.NumSlots = It->GetInventory()->GetNumSlots();
		SaveInventory(It->GetInventory(), Saved.Items);
	}

	for (TActorIterator<ARoxiWorldItem> It(World); It; ++It)
	{
		if (!It->IsActorBeingDestroyed() && !It->GetStack().IsEmpty())
		{
			FRoxiSavedWorldItem& Saved = Save->WorldItems.AddDefaulted_GetRef();
			Saved.Transform = It->GetActorTransform();
			Saved.Stack = MakeSavedStack(0, It->GetStack());
		}
	}

	for (TActorIterator<ARoxiCharacter> It(World); It; ++It)
	{
		ARoxiCharacter* Character = *It;
		if (Character->IsActorBeingDestroyed() || Character->GetHealth()->IsDead())
		{
			continue;
		}
		FString PlayerId;
		FString PlayerName;
		if (Character->IsSleeping())
		{
			PlayerId = Character->GetSleeperId();
			PlayerName = Character->GetSleeperName();
		}
		else if (const ARoxiPlayerController* PC = Cast<ARoxiPlayerController>(Character->GetController()))
		{
			PlayerId = PC->GetPersistentPlayerId();
			PlayerName = PC->PlayerState ? PC->PlayerState->GetPlayerName() : FString();
		}
		if (PlayerId.IsEmpty())
		{
			continue; // боты стенда и т.п.
		}
		FRoxiSavedCharacter& Saved = Save->Characters.AddDefaulted_GetRef();
		Saved.PlayerId = PlayerId;
		Saved.PlayerName = PlayerName;
		Saved.Transform = Character->GetActorTransform();
		Saved.Health = Character->GetHealth()->GetHealth();
		SaveInventory(Character->GetInventory(), Saved.Inventory);
		SaveInventory(Character->GetBelt(), Saved.Belt);
	}

	// Игроки онлайн без персонажа (ждут возрождения) — тоже «мёртвые».
	if (const ARoxiGameMode* GameMode = World->GetAuthGameMode<ARoxiGameMode>())
	{
		TSet<FString> Dead = GameMode->GetDeadPlayers();
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const ARoxiPlayerController* PC = Cast<ARoxiPlayerController>(It->Get());
			if (PC && !PC->GetPawn() && !PC->GetPersistentPlayerId().IsEmpty())
			{
				Dead.Add(PC->GetPersistentPlayerId());
			}
		}
		Save->DeadPlayers = Dead.Array();
	}
	return Save;
}

bool URoxiPersistenceSubsystem::SaveNow(bool bBlocking)
{
	if (!IsActive() || !bLoaded || bLoading)
	{
		return false;
	}
	if (bSaveBlocked)
	{
		UE_LOG(LogRoxi, Warning, TEXT("Save: skipped — existing save could not be loaded and is protected (see log above)"));
		return false;
	}
	if (PendingWrite.IsValid() && !PendingWrite.IsReady())
	{
		if (!bBlocking)
		{
			UE_LOG(LogRoxi, Warning, TEXT("Save: previous write still in progress — skipped"));
			return false;
		}
		PendingWrite.Wait();
	}

	const double Start = FPlatformTime::Seconds();
	URoxiWorldSave* Save = GatherWorld();
	const double Gathered = FPlatformTime::Seconds();
	TArray<uint8> WorldBytes;
	TArray<uint8> ProfileBytes;
	// Профили версии 0 — файлы, записанные до явной записи версии; формат тот же (v1), поэтому просто повышаем.
	Profiles->Version = URoxiProfileSave::CurrentVersion;
	if (!UGameplayStatics::SaveGameToMemory(Save, WorldBytes) || !UGameplayStatics::SaveGameToMemory(Profiles, ProfileBytes))
	{
		UE_LOG(LogRoxi, Error, TEXT("Save: serialization failed"));
		return false;
	}
	LastGatherMs = (FPlatformTime::Seconds() - Start) * 1000.0;
	UE_LOG(LogRoxi, Verbose, TEXT("Save: gather %.1f ms, serialize %.1f ms"),
		(Gathered - Start) * 1000.0, (FPlatformTime::Seconds() - Gathered) * 1000.0);
	const double GatherOnlyMs = (Gathered - Start) * 1000.0;
	LastSaveBytes = WorldBytes.Num();
	LastSaveTime = FDateTime::Now();
	LastSaveSummary = FString::Printf(TEXT("%d pieces, %d deployables, %d containers, %d items, %d characters, %d dead, %d profiles"),
		Save->Pieces.Num(), Save->Deployables.Num(), Save->Containers.Num(), Save->WorldItems.Num(),
		Save->Characters.Num(), Save->DeadPlayers.Num(), Profiles->Profiles.Num());

	FRoxiFileSaveStorage* StoragePtr = Storage.Get();
	auto WriteBoth = [StoragePtr, WorldBytes = MoveTemp(WorldBytes), ProfileBytes = MoveTemp(ProfileBytes)]()
	{
		return StoragePtr->Write(WorldRecord, WorldBytes) && StoragePtr->Write(ProfilesRecord, ProfileBytes);
	};

	UE_LOG(LogRoxi, Log, TEXT("Save: world '%s' — %s; %d KB, game thread %.1f ms (gather %.1f + serialize %.1f), %s write"),
		*WorldName, *LastSaveSummary, LastSaveBytes / 1024, LastGatherMs, GatherOnlyMs, LastGatherMs - GatherOnlyMs,
		bBlocking ? TEXT("blocking") : TEXT("background"));

	if (bBlocking)
	{
		return WriteBoth();
	}
	PendingWrite = Async(EAsyncExecution::ThreadPool, MoveTemp(WriteBoth));
	return true;
}

// ---------------------------------------------------------------------------
// Профили

void URoxiPersistenceSubsystem::TouchProfile(const FString& PlayerId, const FString& PlayerName)
{
	if (!IsActive() || !Profiles || PlayerId.IsEmpty())
	{
		return;
	}
	FRoxiPlayerProfile* Profile = Profiles->Profiles.FindByPredicate([&PlayerId](const FRoxiPlayerProfile& P) { return P.PlayerId == PlayerId; });
	if (!Profile)
	{
		Profile = &Profiles->Profiles.AddDefaulted_GetRef();
		Profile->PlayerId = PlayerId;
		Profile->FirstSeen = FDateTime::Now();
		UE_LOG(LogRoxi, Log, TEXT("Profile: created for %s"), *PlayerId);
	}
	Profile->LastName = PlayerName;
	Profile->LastSeen = FDateTime::Now();
}

FString URoxiPersistenceSubsystem::Describe() const
{
	if (!IsActive())
	{
		return TEXT("Saving inactive (client, or Play-In-Editor with roxi.Save.InPIE 0)");
	}
	return FString::Printf(TEXT("World '%s' in %s; loaded=%d blocked=%d; last save: %s, %d KB, gather %.1f ms — %s"),
		*WorldName, Storage ? *Storage->GetDirectory() : TEXT("-"), bLoaded, bSaveBlocked,
		LastSaveBytes > 0 ? *LastSaveTime.ToString() : TEXT("never"), LastSaveBytes / 1024, LastGatherMs, *LastSaveSummary);
}

// ---------------------------------------------------------------------------
// Консольные команды (Х8)

namespace RoxiSaveCommands
{
	static FAutoConsoleCommandWithWorld SaveCmd(TEXT("roxi.Save"), TEXT("Save the world now (server)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (URoxiPersistenceSubsystem* Sub = World ? World->GetSubsystem<URoxiPersistenceSubsystem>() : nullptr)
			{
				Sub->SaveNow(false);
			}
		}));

	static FAutoConsoleCommandWithWorld InfoCmd(TEXT("roxi.Save.Info"), TEXT("Show save status (server)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (URoxiPersistenceSubsystem* Sub = World ? World->GetSubsystem<URoxiPersistenceSubsystem>() : nullptr)
			{
				UE_LOG(LogRoxi, Display, TEXT("Save info: %s"), *Sub->Describe());
			}
		}));
}
