// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "RoxiSaveTypes.generated.h"

class ARoxiDeployable;

/** Стопка в слоте. Предмет — по PrimaryAssetId (не по пути к файлу — Х6). */
USTRUCT()
struct ROXI_API FRoxiSavedStack
{
	GENERATED_BODY()

	UPROPERTY() int32 Slot = 0;
	UPROPERTY() FPrimaryAssetId Item;
	UPROPERTY() int32 Quantity = 0;
};

/**
 * Элемент постройки — компактная двоичная запись (их десятки тысяч):
 * описание и владелец — индексы в таблицах URoxiWorldSave, положение — float, поворот — только yaw.
 */
USTRUCT()
struct ROXI_API FRoxiSavedPiece
{
	GENERATED_BODY()

	UPROPERTY() int32 DefinitionIndex = INDEX_NONE;
	UPROPERTY() FVector3f Location = FVector3f::ZeroVector;
	UPROPERTY() float Yaw = 0.f;
	UPROPERTY() float Health = 0.f;
	UPROPERTY() int32 OwnerIndex = INDEX_NONE;

	bool Serialize(FArchive& Ar)
	{
		Ar << DefinitionIndex << Location << Yaw << Health << OwnerIndex;
		return true;
	}
};

template<>
struct TStructOpsTypeTraits<FRoxiSavedPiece> : public TStructOpsTypeTraitsBase2<FRoxiSavedPiece>
{
	enum { WithSerializer = true };
};

USTRUCT()
struct ROXI_API FRoxiSavedDeployable
{
	GENERATED_BODY()

	UPROPERTY() TSoftClassPtr<ARoxiDeployable> Class;
	UPROPERTY() FTransform Transform;
	UPROPERTY() float Health = 0.f;
	UPROPERTY() FString OwnerId;
	/** Оставшаяся перезарядка точки возрождения, с (П6). */
	UPROPERTY() float CooldownRemaining = 0.f;
};

USTRUCT()
struct ROXI_API FRoxiSavedContainer
{
	GENERATED_BODY()

	UPROPERTY() FTransform Transform;
	UPROPERTY() FString Label;
	UPROPERTY() int32 NumSlots = 0;
	UPROPERTY() TArray<FRoxiSavedStack> Items;
};

USTRUCT()
struct ROXI_API FRoxiSavedWorldItem
{
	GENERATED_BODY()

	UPROPERTY() FTransform Transform;
	UPROPERTY() FRoxiSavedStack Stack;
};

/** Персонаж игрока (в сети или спящий). После загрузки все становятся спящими (GDD §5.4). */
USTRUCT()
struct ROXI_API FRoxiSavedCharacter
{
	GENERATED_BODY()

	UPROPERTY() FString PlayerId;
	UPROPERTY() FString PlayerName;
	UPROPERTY() FTransform Transform;
	UPROPERTY() float Health = 0.f;
	UPROPERTY() TArray<FRoxiSavedStack> Inventory;
	UPROPERTY() TArray<FRoxiSavedStack> Belt;
};

/** Мир (Х1): всё, что сбросит будущий вайп. */
UCLASS()
class ROXI_API URoxiWorldSave : public USaveGame
{
	GENERATED_BODY()

public:
	/** 2 — компактная запись построек. */
	static constexpr int32 CurrentVersion = 2;

	/** По умолчанию 0: USaveGame не пишет значения, равные значениям по умолчанию, поэтому
	 *  текущая версия выставляется явно перед записью и всегда попадает в файл. */
	UPROPERTY() int32 Version = 0;
	UPROPERTY() FString MapName;
	UPROPERTY() FDateTime SavedAt;

	/** Таблицы для компактной записи построек. */
	UPROPERTY() TArray<FPrimaryAssetId> PieceDefinitions;
	UPROPERTY() TArray<FString> Owners;
	UPROPERTY() TArray<FRoxiSavedPiece> Pieces;
	UPROPERTY() TArray<FRoxiSavedDeployable> Deployables;
	UPROPERTY() TArray<FRoxiSavedContainer> Containers;
	UPROPERTY() TArray<FRoxiSavedWorldItem> WorldItems;
	UPROPERTY() TArray<FRoxiSavedCharacter> Characters;

	/** Игроки, которые сейчас мертвы (умерли и вышли, или спящего убили): при входе — выбор точки возрождения. */
	UPROPERTY() TArray<FString> DeadPlayers;
};

/** Постоянный профиль игрока (GDD §13). Логики начисления нет (П4). */
USTRUCT()
struct ROXI_API FRoxiPlayerProfile
{
	GENERATED_BODY()

	UPROPERTY() FString PlayerId;
	UPROPERTY() FString LastName;
	UPROPERTY() int32 Level = 1;
	UPROPERTY() TArray<FName> Achievements;
	UPROPERTY() FDateTime FirstSeen;
	UPROPERTY() FDateTime LastSeen;
};

/** Профили (Х1): не сбрасываются вайпом. */
UCLASS()
class ROXI_API URoxiProfileSave : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentVersion = 1;

	/** По умолчанию 0: USaveGame не пишет значения, равные значениям по умолчанию, поэтому
	 *  текущая версия выставляется явно перед записью и всегда попадает в файл. */
	UPROPERTY() int32 Version = 0;
	UPROPERTY() TArray<FRoxiPlayerProfile> Profiles;
};
