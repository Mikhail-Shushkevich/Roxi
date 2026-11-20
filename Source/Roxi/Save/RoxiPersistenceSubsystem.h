// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Save/RoxiSaveStorage.h"
#include "Save/RoxiSaveTypes.h"
#include "RoxiPersistenceSubsystem.generated.h"

class URoxiInventoryComponent;

/**
 * Сохранение мира и профилей (Этап 4). Работает только на сервере.
 *
 * - Мир и профили — разные записи хранилища (Х1): будущий вайп удалит только мир.
 * - Сохранение: раз в roxi.Save.Interval секунд, при остановке сервера и по команде roxi.Save (Х3).
 *   Сбор данных — на игровом потоке, запись — в фоне (кроме остановки сервера).
 * - Загрузка: при старте, до появления игроков (Х4). Несовпадение версии/карты или нечитаемые файлы —
 *   мир не загружается и не перезаписывается до перезапуска.
 * - В редакторе (PIE) выключено по умолчанию (roxi.Save.InPIE): ID игроков там случайные.
 * Имя мира: параметр запуска -RoxiWorld=<имя> (по умолчанию default).
 */
UCLASS()
class ROXI_API URoxiPersistenceSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Готов ли мир: загружен, или сохранение неактивно. До этого GameMode не выдаёт игрокам персонажей. */
	bool IsReady() const { return bLoaded || !IsActive(); }

	/** Сохранить сейчас. bBlocking — записать файл в этом же потоке (остановка сервера). */
	bool SaveNow(bool bBlocking);

	/** Создать/обновить профиль игрока при входе. */
	void TouchProfile(const FString& PlayerId, const FString& PlayerName);

	FString Describe() const;

private:
	/** Загрузить мир (один раз). Вызывается на следующем тике после BeginPlay мира: акторы уже инициализированы. */
	void EnsureLoaded();
	bool IsActive() const;
	void HandleBeginTearDown(UWorld* InWorld);

	URoxiWorldSave* GatherWorld() const;
	void ApplyWorld(const URoxiWorldSave& Save);

	static void SaveInventory(const URoxiInventoryComponent* Inventory, TArray<FRoxiSavedStack>& Out);
	static void LoadInventory(URoxiInventoryComponent* Inventory, const TArray<FRoxiSavedStack>& Stacks);
	static FRoxiSavedStack MakeSavedStack(int32 Slot, const struct FRoxiItemStack& Stack);
	static bool ResolveStack(const FRoxiSavedStack& Saved, struct FRoxiItemStack& Out);

	TUniquePtr<FRoxiFileSaveStorage> Storage;

	UPROPERTY(Transient)
	TObjectPtr<URoxiProfileSave> Profiles;

	FString WorldName;
	bool bLoaded = false;
	bool bLoading = false;
	/** Мир не загрузился из существующих файлов — не перезаписывать их. */
	bool bSaveBlocked = false;

	FTimerHandle AutosaveTimer;
	FDelegateHandle TearDownHandle;
	TFuture<bool> PendingWrite;

	// Для roxi.Save.Info
	FDateTime LastSaveTime;
	int32 LastSaveBytes = 0;
	double LastGatherMs = 0.0;
	FString LastSaveSummary;
};
