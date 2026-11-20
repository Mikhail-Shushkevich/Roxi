// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Items/RoxiItemStack.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "RoxiInventoryList.generated.h"

/** Один слот контейнера. SlotIndex хранится явно: порядок элементов на клиенте не гарантирован. */
USTRUCT()
struct ROXI_API FRoxiInventoryEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY()
	FRoxiItemStack Stack;
};

/**
 * Набор слотов с дельта-репликацией (по сети уходят только изменённые слоты).
 * На сервере Entries[i].SlotIndex == i.
 */
USTRUCT()
struct ROXI_API FRoxiInventoryList : public FFastArraySerializer
{
	GENERATED_BODY()

	/** Сервер: создать NumSlots пустых слотов (всё прежнее содержимое теряется). */
	void Init(int32 NumSlots);
	/** Сервер: очистить список полностью (0 слотов). */
	void Reset();
	/** Сервер: сделать содержимое копией Source, помечая изменёнными только отличающиеся слоты. */
	void SyncFrom(const FRoxiInventoryList& Source);

	int32 Num() const { return Entries.Num(); }
	bool IsValidSlot(int32 Slot) const { return Slot >= 0 && Slot < Entries.Num(); }
	FRoxiItemStack GetStack(int32 Slot) const;
	/** Сервер. */
	void SetStack(int32 Slot, const FRoxiItemStack& Stack);

	FString Describe() const;

	// FFastArraySerializer (клиент)
	void PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize) { OnReplicated.Broadcast(); }
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize) { OnReplicated.Broadcast(); }
	void PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize) { OnReplicated.Broadcast(); }

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FRoxiInventoryEntry, FRoxiInventoryList>(Entries, DeltaParms, *this);
	}

	/** Клиент: содержимое пришло по сети. */
	FSimpleMulticastDelegate OnReplicated;

private:
	const FRoxiInventoryEntry* FindEntry(int32 Slot) const;
	FRoxiInventoryEntry* FindEntry(int32 Slot);

	UPROPERTY()
	TArray<FRoxiInventoryEntry> Entries;
};

template<>
struct TStructOpsTypeTraits<FRoxiInventoryList> : public TStructOpsTypeTraitsBase2<FRoxiInventoryList>
{
	enum { WithNetDeltaSerializer = true };
};
