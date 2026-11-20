// Copyright Epic Games, Inc. All Rights Reserved.

#include "Save/RoxiSaveStorage.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Roxi.h"

FRoxiFileSaveStorage::FRoxiFileSaveStorage(const FString& InWorldName)
{
	Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), TEXT("Roxi"), InWorldName);
	IFileManager::Get().MakeDirectory(*Directory, true);
}

FString FRoxiFileSaveStorage::PathFor(const FString& Name, int32 BackupIndex) const
{
	return BackupIndex == 0
		? FPaths::Combine(Directory, Name + TEXT(".sav"))
		: FPaths::Combine(Directory, FString::Printf(TEXT("%s.bak%d"), *Name, BackupIndex));
}

bool FRoxiFileSaveStorage::Write(const FString& Name, const TArray<uint8>& Bytes)
{
	IFileManager& FM = IFileManager::Get();
	const FString TempPath = FPaths::Combine(Directory, Name + TEXT(".tmp"));
	if (!FFileHelper::SaveArrayToFile(Bytes, *TempPath))
	{
		UE_LOG(LogRoxi, Error, TEXT("Save: cannot write %s"), *TempPath);
		return false;
	}

	// Ротация: bak2 -> bak3, bak1 -> bak2, sav -> bak1.
	for (int32 i = NumBackups; i >= 1; --i)
	{
		const FString From = PathFor(Name, i - 1);
		if (FM.FileExists(*From))
		{
			FM.Move(*PathFor(Name, i), *From, /*Replace*/ true);
		}
	}
	if (!FM.Move(*PathFor(Name, 0), *TempPath, /*Replace*/ true))
	{
		UE_LOG(LogRoxi, Error, TEXT("Save: cannot move %s into place"), *TempPath);
		return false;
	}
	return true;
}

bool FRoxiFileSaveStorage::Read(const FString& Name, TFunctionRef<bool(const TArray<uint8>&)> Validate, TArray<uint8>& OutBytes, FString& OutSource)
{
	for (int32 i = 0; i <= NumBackups; ++i)
	{
		const FString Path = PathFor(Name, i);
		if (!IFileManager::Get().FileExists(*Path))
		{
			continue;
		}
		TArray<uint8> Bytes;
		if (FFileHelper::LoadFileToArray(Bytes, *Path) && Validate(Bytes))
		{
			OutBytes = MoveTemp(Bytes);
			OutSource = Path;
			return true;
		}
		UE_LOG(LogRoxi, Warning, TEXT("Save: %s is unreadable or invalid, trying older backup"), *Path);
	}
	return false;
}
