// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/RoxiInventoryList.h"

void FRoxiInventoryList::Init(int32 NumSlots)
{
	Entries.SetNum(FMath::Max(0, NumSlots));
	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		Entries[i] = FRoxiInventoryEntry();
		Entries[i].SlotIndex = i;
	}
	MarkArrayDirty();
}

void FRoxiInventoryList::Reset()
{
	Entries.Reset();
	MarkArrayDirty();
}

void FRoxiInventoryList::SyncFrom(const FRoxiInventoryList& Source)
{
	if (Entries.Num() != Source.Num())
	{
		Init(Source.Num());
	}
	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		const FRoxiItemStack SourceStack = Source.GetStack(i);
		if (Entries[i].Stack != SourceStack)
		{
			Entries[i].Stack = SourceStack;
			MarkItemDirty(Entries[i]);
		}
	}
}

const FRoxiInventoryEntry* FRoxiInventoryList::FindEntry(int32 Slot) const
{
	if (Entries.IsValidIndex(Slot) && Entries[Slot].SlotIndex == Slot)
	{
		return &Entries[Slot];
	}
	return Entries.FindByPredicate([Slot](const FRoxiInventoryEntry& E) { return E.SlotIndex == Slot; });
}

FRoxiInventoryEntry* FRoxiInventoryList::FindEntry(int32 Slot)
{
	return const_cast<FRoxiInventoryEntry*>(static_cast<const FRoxiInventoryList*>(this)->FindEntry(Slot));
}

FRoxiItemStack FRoxiInventoryList::GetStack(int32 Slot) const
{
	const FRoxiInventoryEntry* Entry = FindEntry(Slot);
	return Entry ? Entry->Stack : FRoxiItemStack();
}

void FRoxiInventoryList::SetStack(int32 Slot, const FRoxiItemStack& Stack)
{
	if (FRoxiInventoryEntry* Entry = FindEntry(Slot))
	{
		Entry->Stack = Stack;
		if (Entry->Stack.IsEmpty())
		{
			Entry->Stack.Clear();
		}
		MarkItemDirty(*Entry);
	}
}

FString FRoxiInventoryList::Describe() const
{
	TArray<FString> Parts;
	for (const FRoxiInventoryEntry& Entry : Entries)
	{
		if (!Entry.Stack.IsEmpty())
		{
			Parts.Add(FString::Printf(TEXT("[%d] %s"), Entry.SlotIndex, *Entry.Stack.ToString()));
		}
	}
	return Parts.IsEmpty() ? FString::Printf(TEXT("(empty, %d slots)"), Entries.Num()) : FString::Join(Parts, TEXT(", "));
}
