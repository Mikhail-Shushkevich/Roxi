// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dev/RoxiLoadTestSubsystem.h"
#include "Dev/RoxiLoadTestBotController.h"
#include "Roxi.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/IConsoleManager.h"
#include "Building/RoxiBuildingPiece.h"
#include "Building/RoxiBuildPieceDefinition.h"
#include "Items/RoxiInventoryComponent.h"
#include "Items/RoxiItemDefinition.h"
#include "Items/RoxiLootContainer.h"
#include "Items/RoxiWorldItem.h"

bool URoxiLoadTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
#endif
}

TStatId URoxiLoadTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URoxiLoadTestSubsystem, STATGROUP_Tickables);
}

bool URoxiLoadTestSubsystem::HasAuthorityWorld() const
{
	const UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogRoxi, Warning, TEXT("LoadTest: commands must run on the server"));
		return false;
	}
	return true;
}

void URoxiLoadTestSubsystem::SpawnBots(int32 Count, float Radius)
{
	if (!HasAuthorityWorld() || Count <= 0)
	{
		return;
	}

	UWorld* World = GetWorld();
	const AGameModeBase* GameMode = World->GetAuthGameMode();
	UClass* PawnClass = GameMode ? GameMode->DefaultPawnClass.Get() : nullptr;
	if (!PawnClass)
	{
		UE_LOG(LogRoxi, Error, TEXT("LoadTest: no DefaultPawnClass"));
		return;
	}

	SpawnRadius = Radius;
	FVector Center = FVector::ZeroVector;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		Center = It->GetActorLocation();
		break;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	int32 Spawned = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector2D Offset = FMath::RandPointInCircle(Radius * 0.9f);
		const FVector Location = Center + FVector(Offset.X, Offset.Y, 50.f);
		const FRotator Rotation(0.f, FMath::FRandRange(0.f, 360.f), 0.f);

		APawn* Pawn = World->SpawnActor<APawn>(PawnClass, Location, Rotation, Params);
		if (!Pawn)
		{
			continue;
		}

		ARoxiLoadTestBotController* Controller = World->SpawnActor<ARoxiLoadTestBotController>(Params);
		Controller->InitWander(Center, Radius);
		Controller->Possess(Pawn);

		Bots.Add(Pawn);
		BotControllers.Add(Controller);
		++Spawned;
	}

	UE_LOG(LogRoxi, Log, TEXT("LoadTest: spawned %d bots (total %d), radius %.0f"), Spawned, Bots.Num(), Radius);
}

void URoxiLoadTestSubsystem::ClearBots()
{
	for (const TWeakObjectPtr<AController>& Controller : BotControllers)
	{
		if (Controller.IsValid())
		{
			Controller->Destroy();
		}
	}
	for (const TWeakObjectPtr<APawn>& Pawn : Bots)
	{
		if (Pawn.IsValid())
		{
			Pawn->Destroy();
		}
	}
	UE_LOG(LogRoxi, Log, TEXT("LoadTest: cleared %d bots"), Bots.Num());
	Bots.Reset();
	BotControllers.Reset();
}

void URoxiLoadTestSubsystem::SpawnItems(int32 Count, float Radius)
{
	if (!HasAuthorityWorld() || Count <= 0)
	{
		return;
	}
	URoxiItemDefinition* Item = URoxiItemDefinition::LoadByName(TEXT("DEV_TestItemA"));
	if (!Item)
	{
		UE_LOG(LogRoxi, Error, TEXT("LoadTest: DEV_TestItemA not found"));
		return;
	}

	UWorld* World = GetWorld();
	FVector Center = FVector::ZeroVector;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		Center = It->GetActorLocation();
		break;
	}

	int32 Containers = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector2D Offset = FMath::RandPointInCircle(Radius);
		const FVector Location = Center + FVector(Offset.X, Offset.Y, 0.f);
		if (i % 4 == 3)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (ARoxiLootContainer* Container = World->SpawnActor<ARoxiLootContainer>(Location, FRotator::ZeroRotator, Params))
			{
				Container->GetInventory()->AddItem(Item, 10);
				SpawnedItems.Add(Container);
				++Containers;
			}
		}
		else if (ARoxiWorldItem* WorldItem = ARoxiWorldItem::SpawnItem(World, FRoxiItemStack(Item, 1), Location))
		{
			SpawnedItems.Add(WorldItem);
		}
	}
	UE_LOG(LogRoxi, Log, TEXT("LoadTest: spawned %d world actors (%d loot containers), total %d"),
		Count, Containers, SpawnedItems.Num());
}

void URoxiLoadTestSubsystem::SpawnPieces(int32 Count)
{
	const URoxiBuildPieceDefinition* Foundation = URoxiBuildPieceDefinition::LoadByName(TEXT("DA_Piece_Foundation"));
	const URoxiBuildPieceDefinition* Wall = URoxiBuildPieceDefinition::LoadByName(TEXT("DA_Piece_Wall"));
	if (!HasAuthorityWorld() || Count <= 0 || !Foundation || !Wall)
	{
		return;
	}

	UWorld* World = GetWorld();
	FVector Center = FVector::ZeroVector;
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		Center = It->GetActorLocation();
		break;
	}

	// Сетка «фундамент + стена» — половина элементов каждого типа. Без проверок размещения.
	const int32 Cells = FMath::Max(1, Count / 2);
	const int32 Side = FMath::CeilToInt(FMath::Sqrt(static_cast<float>(Cells)));
	TArray<FRoxiBuildSocket> Sockets;
	for (int32 i = 0; i < Cells; ++i)
	{
		FRoxiBuildSocket Socket;
		Socket.Location = Center + FVector((i % Side) * RoxiBuildGrid::CellSize, (i / Side) * RoxiBuildGrid::CellSize, 0.f);
		if (ARoxiBuildingPiece* Piece = ARoxiBuildingPiece::SpawnPiece(World, Foundation, Socket, TEXT("LoadTest")))
		{
			SpawnedItems.Add(Piece);
			Sockets.Reset();
			Piece->GetSockets(ERoxiPieceType::Wall, Sockets);
			if (ARoxiBuildingPiece* WallPiece = ARoxiBuildingPiece::SpawnPiece(World, Wall, Sockets[0], TEXT("LoadTest")))
			{
				SpawnedItems.Add(WallPiece);
			}
		}
	}
	UE_LOG(LogRoxi, Log, TEXT("LoadTest: spawned %d building pieces, total spawned %d"), Cells * 2, SpawnedItems.Num());
}

void URoxiLoadTestSubsystem::ClearItems()
{
	for (const TWeakObjectPtr<AActor>& Actor : SpawnedItems)
	{
		if (Actor.IsValid())
		{
			Actor->Destroy();
		}
	}
	SpawnedItems.Reset();
}

void URoxiLoadTestSubsystem::StartMeasure(float Seconds)
{
	if (!HasAuthorityWorld())
	{
		return;
	}
	MeasureSeconds = FMath::Max(Seconds, 1.f);
	SweepIndex = INDEX_NONE;
	BeginMeasuring();
}

void URoxiLoadTestSubsystem::StartSweep(float InitialDelay, float InMeasureSeconds, const TArray<int32>& Counts)
{
	if (!HasAuthorityWorld() || Counts.IsEmpty())
	{
		return;
	}
	MeasureSeconds = FMath::Max(InMeasureSeconds, 1.f);
	SweepCounts = Counts;
	SweepIndex = INDEX_NONE;
	Phase = EPhase::SweepDelay;
	PhaseEndTime = FPlatformTime::Seconds() + InitialDelay;
	UE_LOG(LogRoxi, Log, TEXT("LoadTest: sweep of %d steps starts in %.0fs"), Counts.Num(), InitialDelay);
}

void URoxiLoadTestSubsystem::AdvanceSweep()
{
	++SweepIndex;
	ClearBots();

	if (!SweepCounts.IsValidIndex(SweepIndex))
	{
		UE_LOG(LogRoxi, Log, TEXT("LoadTest: sweep finished"));
		Phase = EPhase::Idle;
		SweepIndex = INDEX_NONE;
		return;
	}

	SpawnBots(SweepCounts[SweepIndex], SpawnRadius);
	Phase = EPhase::Settling;
	PhaseEndTime = FPlatformTime::Seconds() + 5.0; // дать ботам приземлиться и разойтись
}

void URoxiLoadTestSubsystem::BeginMeasuring()
{
	FrameTimesMs.Reset();
	OutBytesPerSecSum = 0.0;
	InBytesPerSecSum = 0.0;
	BandwidthSamples = 0;

	// Без этого NetDriver не обновляет OutBytesPerSecond / InBytesPerSecond.
	if (IConsoleVariable* NetStats = IConsoleManager::Get().FindConsoleVariable(TEXT("net.EnableNetStats")))
	{
		bRestoreNetStats = !NetStats->GetBool();
		NetStats->Set(true, ECVF_SetByCode);
	}

	GEngine->Exec(GetWorld(), TEXT("CsvProfile Start"));

	Phase = EPhase::Measuring;
	PhaseEndTime = FPlatformTime::Seconds() + MeasureSeconds;
}

void URoxiLoadTestSubsystem::FinishMeasuring()
{
	GEngine->Exec(GetWorld(), TEXT("CsvProfile Stop"));

	if (bRestoreNetStats)
	{
		if (IConsoleVariable* NetStats = IConsoleManager::Get().FindConsoleVariable(TEXT("net.EnableNetStats")))
		{
			NetStats->Set(false, ECVF_SetByCode);
		}
		bRestoreNetStats = false;
	}

	const UWorld* World = GetWorld();
	const UNetDriver* NetDriver = World ? World->GetNetDriver() : nullptr;
	const int32 Connections = NetDriver ? NetDriver->ClientConnections.Num() : 0;

	TArray<float> Sorted = FrameTimesMs;
	Sorted.Sort();
	auto Percentile = [&Sorted](float P) -> float
	{
		return Sorted.IsEmpty() ? 0.f : Sorted[FMath::Clamp(FMath::FloorToInt(P * (Sorted.Num() - 1)), 0, Sorted.Num() - 1)];
	};
	double Sum = 0.0;
	for (float V : Sorted) { Sum += V; }
	const double Avg = Sorted.IsEmpty() ? 0.0 : Sum / Sorted.Num();

	const double OutKBs = BandwidthSamples ? OutBytesPerSecSum / BandwidthSamples / 1024.0 : 0.0;
	const double InKBs = BandwidthSamples ? InBytesPerSecSum / BandwidthSamples / 1024.0 : 0.0;

	UE_LOG(LogRoxi, Display,
		TEXT("LoadTest RESULT bots=%d connections=%d frames=%d frame_ms avg=%.2f p50=%.2f p95=%.2f max=%.2f out_KBps=%.1f in_KBps=%.1f out_KBps_per_conn=%.1f"),
		Bots.Num(), Connections, Sorted.Num(), Avg, Percentile(0.5f), Percentile(0.95f), Percentile(1.f),
		OutKBs, InKBs, Connections ? OutKBs / Connections : 0.0);

	Phase = EPhase::Idle;
	if (SweepIndex != INDEX_NONE)
	{
		AdvanceSweep();
	}
}

void URoxiLoadTestSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (Phase == EPhase::Idle)
	{
		return;
	}

	const double Now = FPlatformTime::Seconds();

	if (Phase == EPhase::Measuring)
	{
		FrameTimesMs.Add(DeltaTime * 1000.f);
		if (const UNetDriver* NetDriver = GetWorld()->GetNetDriver())
		{
			OutBytesPerSecSum += NetDriver->OutBytesPerSecond;
			InBytesPerSecSum += NetDriver->InBytesPerSecond;
			++BandwidthSamples;
		}
	}

	if (Now < PhaseEndTime)
	{
		return;
	}

	switch (Phase)
	{
	case EPhase::SweepDelay: AdvanceSweep(); break;
	case EPhase::Settling:   BeginMeasuring(); break;
	case EPhase::Measuring:  FinishMeasuring(); break;
	default: break;
	}
}

#if !UE_BUILD_SHIPPING
namespace RoxiLoadTest
{
	static URoxiLoadTestSubsystem* Get(UWorld* World)
	{
		return World ? World->GetSubsystem<URoxiLoadTestSubsystem>() : nullptr;
	}

	static FAutoConsoleCommandWithWorldAndArgs SpawnCmd(
		TEXT("roxi.LoadTest.Spawn"), TEXT("Spawn N wandering server bots: <Count> [Radius=1500]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (URoxiLoadTestSubsystem* Sub = Get(World))
			{
				Sub->SpawnBots(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 10, Args.Num() > 1 ? FCString::Atof(*Args[1]) : 1500.f);
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs ClearCmd(
		TEXT("roxi.LoadTest.Clear"), TEXT("Destroy all load-test bots"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (URoxiLoadTestSubsystem* Sub = Get(World)) { Sub->ClearBots(); }
		}));

	static FAutoConsoleCommandWithWorldAndArgs SpawnItemsCmd(
		TEXT("roxi.LoadTest.SpawnItems"), TEXT("Spawn N world items / loot containers: <Count> [Radius=1300]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (URoxiLoadTestSubsystem* Sub = Get(World))
			{
				Sub->SpawnItems(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 100, Args.Num() > 1 ? FCString::Atof(*Args[1]) : 1300.f);
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs SpawnPiecesCmd(
		TEXT("roxi.LoadTest.SpawnPieces"), TEXT("Spawn N building pieces (foundation+wall grid): <Count>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (URoxiLoadTestSubsystem* Sub = Get(World)) { Sub->SpawnPieces(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 1000); }
		}));

	static FAutoConsoleCommandWithWorldAndArgs ClearItemsCmd(
		TEXT("roxi.LoadTest.ClearItems"), TEXT("Destroy load-test world items and containers"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (URoxiLoadTestSubsystem* Sub = Get(World)) { Sub->ClearItems(); }
		}));

	static FAutoConsoleCommandWithWorldAndArgs MeasureCmd(
		TEXT("roxi.LoadTest.Measure"), TEXT("Measure server frame time and bandwidth: <Seconds=30>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (URoxiLoadTestSubsystem* Sub = Get(World)) { Sub->StartMeasure(Args.Num() > 0 ? FCString::Atof(*Args[0]) : 30.f); }
		}));

	static FAutoConsoleCommandWithWorldAndArgs SweepCmd(
		TEXT("roxi.LoadTest.Sweep"), TEXT("Series of measurements: <DelaySec> <MeasureSec> <N1> [N2 ...]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			URoxiLoadTestSubsystem* Sub = Get(World);
			if (!Sub || Args.Num() < 3)
			{
				UE_LOG(LogRoxi, Warning, TEXT("Usage: roxi.LoadTest.Sweep <DelaySec> <MeasureSec> <N1> [N2 ...]"));
				return;
			}
			TArray<int32> Counts;
			for (int32 i = 2; i < Args.Num(); ++i) { Counts.Add(FCString::Atoi(*Args[i])); }
			Sub->StartSweep(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]), Counts);
		}));
}
#endif
