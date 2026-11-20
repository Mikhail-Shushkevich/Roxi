// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Хранилище сохранений (Х1, Х2). Работает с готовыми байтами; что внутри — решает вызывающий.
 * Сейчас — файлы на сервере. Другая реализация (БД, общий сервис профилей) подменяет только этот класс.
 */
class ROXI_API IRoxiSaveStorage
{
public:
	virtual ~IRoxiSaveStorage() = default;

	/** Записать запись Name. Должно быть безопасно вызывать из фонового потока. */
	virtual bool Write(const FString& Name, const TArray<uint8>& Bytes) = 0;

	/** Прочитать запись Name; при повреждении — попробовать резервные копии. OutSource — откуда прочитано. */
	virtual bool Read(const FString& Name, TFunctionRef<bool(const TArray<uint8>&)> Validate, TArray<uint8>& OutBytes, FString& OutSource) = 0;
};

/**
 * Файлы в Saved/SaveGames/Roxi/<WorldName>/ (Х5):
 * запись во временный файл и переименование (сбой не портит предыдущее сохранение),
 * 3 резервные копии с ротацией.
 */
class ROXI_API FRoxiFileSaveStorage final : public IRoxiSaveStorage
{
public:
	static constexpr int32 NumBackups = 3;

	explicit FRoxiFileSaveStorage(const FString& InWorldName);

	virtual bool Write(const FString& Name, const TArray<uint8>& Bytes) override;
	virtual bool Read(const FString& Name, TFunctionRef<bool(const TArray<uint8>&)> Validate, TArray<uint8>& OutBytes, FString& OutSource) override;

	const FString& GetDirectory() const { return Directory; }

private:
	FString PathFor(const FString& Name, int32 BackupIndex) const;

	FString Directory;
};
