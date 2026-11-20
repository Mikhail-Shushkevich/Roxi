// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/RoxiItemStack.h"
#include "Items/RoxiItemDefinition.h"

int32 FRoxiItemStack::GetMaxStackSize() const
{
	return Item ? FMath::Max(1, Item->MaxStackSize) : 1;
}

FString FRoxiItemStack::ToString() const
{
	return IsEmpty() ? FString(TEXT("-")) : FString::Printf(TEXT("%s x%d"), *Item->GetName(), Quantity);
}
