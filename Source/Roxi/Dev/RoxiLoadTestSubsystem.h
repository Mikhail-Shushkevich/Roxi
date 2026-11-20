// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RoxiLoadTestSubsystem.generated.h"

class APawn;
class AController;

/**
 * DEV ONLY. Нагрузочный стенд сети (Этап 1). Не создаётся в Shipping и на клиентах.
 *
 * Консольные команды (выполняются на сервере):
 *   roxi.LoadTest.Spawn <Count> [Radius]          — заспавнить ботов
 *   roxi.LoadTest.Clear                           — удалить всех ботов
 *   roxi.LoadTest.Measure <Seconds>               — замер с текущим числом ботов
 *   roxi.LoadTest.Sweep <Delay> <Seconds> <N1> [N2 ...] — серия замеров для разных N
 *
 * Каждый замер пишет CSV-профиль (Saved/Profiling/CSV) и строку "LoadTest RESULT" в лог.
 */
UCLASS()
class ROXI_API URoxiLoadTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	void SpawnBots(int32 Count, float Radius);
	void ClearBots();
	void StartMeasure(float Seconds);
	void StartSweep(float InitialDelay, float MeasureSeconds, const TArray<int32>& Counts);

private:
	enum class EPhase : uint8 { Idle, SweepDelay, Settling, Measuring };

	bool HasAuthorityWorld() const;
	void BeginMeasuring();
	void FinishMeasuring();
	void AdvanceSweep();

	TArray<TWeakObjectPtr<APawn>> Bots;
	TArray<TWeakObjectPtr<AController>> BotControllers;

	EPhase Phase = EPhase::Idle;
	double PhaseEndTime = 0.0;
	float MeasureSeconds = 30.f;
	float SpawnRadius = 1500.f;

	TArray<int32> SweepCounts;
	int32 SweepIndex = INDEX_NONE;

	TArray<float> FrameTimesMs;
	double OutBytesPerSecSum = 0.0;
	double InBytesPerSecSum = 0.0;
	int32 BandwidthSamples = 0;
	bool bRestoreNetStats = false;
};
