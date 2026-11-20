// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/RoxiInventoryComponent.h"
#include "Items/RoxiItemDefinition.h"
#include "Net/UnrealNetwork.h"

URoxiInventoryComponent::URoxiInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void URoxiInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(URoxiInventoryComponent, Slots, COND_OwnerOnly);
}

void URoxiInventoryComponent::OnRegister()
{
	Super::OnRegister();
	if (!Slots.OnReplicated.IsBoundToObject(this))
	{
		Slots.OnReplicated.AddUObject(this, &URoxiInventoryComponent::HandleReplicated);
	}
}

void URoxiInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && Slots.Num() == 0)
	{
		Slots.Init(NumSlots);
	}
}

bool URoxiInventoryComponent::HasAuthority() const
{
	const AActor* Owner = GetOwner();
	return Owner && Owner->HasAuthority();
}

bool URoxiInventoryComponent::IsEmpty() const
{
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		if (!Slots.GetStack(i).IsEmpty())
		{
			return false;
		}
	}
	return true;
}

bool URoxiInventoryComponent::SetNumSlots(int32 InNumSlots)
{
	if (!HasAuthority() || !IsEmpty())
	{
		return false;
	}
	NumSlots = FMath::Max(1, InNumSlots);
	Slots.Init(NumSlots);
	BroadcastChanged();
	return true;
}

int32 URoxiInventoryComponent::AddItem(URoxiItemDefinition* Item, int32 Quantity)
{
	if (!HasAuthority() || !Item || Quantity <= 0)
	{
		return Quantity;
	}

	const int32 MaxStack = FMath::Max(1, Item->MaxStackSize);
	int32 Left = Quantity;

	// 1. Дополнить неполные стопки того же предмета.
	for (int32 i = 0; i < Slots.Num() && Left > 0; ++i)
	{
		FRoxiItemStack Stack = Slots.GetStack(i);
		if (Stack.Item == Item && Stack.Quantity < MaxStack)
		{
			const int32 Add = FMath::Min(Left, MaxStack - Stack.Quantity);
			Stack.Quantity += Add;
			Slots.SetStack(i, Stack);
			Left -= Add;
		}
	}

	// 2. Пустые слоты.
	for (int32 i = 0; i < Slots.Num() && Left > 0; ++i)
	{
		if (Slots.GetStack(i).IsEmpty())
		{
			const int32 Add = FMath::Min(Left, MaxStack);
			Slots.SetStack(i, FRoxiItemStack(Item, Add));
			Left -= Add;
		}
	}

	if (Left != Quantity)
	{
		BroadcastChanged();
	}
	return Left;
}

FRoxiItemStack URoxiInventoryComponent::RemoveFromSlot(int32 Slot, int32 Quantity)
{
	if (!HasAuthority() || !Slots.IsValidSlot(Slot))
	{
		return FRoxiItemStack();
	}

	FRoxiItemStack Stack = Slots.GetStack(Slot);
	if (Stack.IsEmpty())
	{
		return FRoxiItemStack();
	}

	const int32 Take = (Quantity <= 0) ? Stack.Quantity : FMath::Min(Quantity, Stack.Quantity);
	const FRoxiItemStack Removed(Stack.Item, Take);
	Stack.Quantity -= Take;
	Slots.SetStack(Slot, Stack);
	BroadcastChanged();
	return Removed;
}

TArray<FRoxiItemStack> URoxiInventoryComponent::RemoveAll()
{
	TArray<FRoxiItemStack> Result;
	if (!HasAuthority())
	{
		return Result;
	}
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		const FRoxiItemStack Stack = Slots.GetStack(i);
		if (!Stack.IsEmpty())
		{
			Result.Add(Stack);
			Slots.SetStack(i, FRoxiItemStack());
		}
	}
	if (!Result.IsEmpty())
	{
		BroadcastChanged();
	}
	return Result;
}

bool URoxiInventoryComponent::MoveItem(URoxiInventoryComponent* From, int32 FromSlot, URoxiInventoryComponent* To, int32 ToSlot, int32 Quantity)
{
	if (!From || !To || !From->HasAuthority() || !To->HasAuthority()
		|| !From->IsValidSlot(FromSlot) || !To->IsValidSlot(ToSlot)
		|| (From == To && FromSlot == ToSlot))
	{
		return false;
	}

	FRoxiItemStack Src = From->GetStack(FromSlot);
	if (Src.IsEmpty())
	{
		return false;
	}

	const int32 Qty = (Quantity <= 0) ? Src.Quantity : FMath::Min(Quantity, Src.Quantity);
	FRoxiItemStack Dst = To->GetStack(ToSlot);
	FRoxiItemStack NewSrc;
	FRoxiItemStack NewDst;

	if (Dst.IsEmpty())
	{
		NewDst = FRoxiItemStack(Src.Item, Qty);
		NewSrc = FRoxiItemStack(Src.Item, Src.Quantity - Qty);
	}
	else if (Dst.Item == Src.Item)
	{
		const int32 Move = FMath::Min(Qty, Dst.GetMaxStackSize() - Dst.Quantity);
		if (Move <= 0)
		{
			return false;
		}
		NewDst = FRoxiItemStack(Dst.Item, Dst.Quantity + Move);
		NewSrc = FRoxiItemStack(Src.Item, Src.Quantity - Move);
	}
	else if (Qty == Src.Quantity)
	{
		NewDst = Src;
		NewSrc = Dst;
	}
	else
	{
		return false; // часть стопки в занятый другим предметом слот
	}

	From->Slots.SetStack(FromSlot, NewSrc);
	To->Slots.SetStack(ToSlot, NewDst);
	From->BroadcastChanged();
	if (To != From)
	{
		To->BroadcastChanged();
	}
	return true;
}

void URoxiInventoryComponent::BroadcastChanged()
{
	OnChanged.Broadcast(this);
}

void URoxiInventoryComponent::HandleReplicated()
{
	BroadcastChanged();
}
