// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RoxiItemStack.generated.h"

class URoxiItemDefinition;

/** Стопка предметов (Т2): тип + количество. Лёгкая структура, без UObject на экземпляр. */
USTRUCT(BlueprintType)
struct ROXI_API FRoxiItemStack
{
	GENERATED_BODY()

	FRoxiItemStack() = default;
	FRoxiItemStack(URoxiItemDefinition* InItem, int32 InQuantity) : Item(InItem), Quantity(InQuantity) {}

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	TObjectPtr<URoxiItemDefinition> Item = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Item")
	int32 Quantity = 0;

	bool IsEmpty() const { return Item == nullptr || Quantity <= 0; }
	void Clear() { Item = nullptr; Quantity = 0; }
	int32 GetMaxStackSize() const;

	bool operator==(const FRoxiItemStack& Other) const { return Item == Other.Item && Quantity == Other.Quantity; }
	bool operator!=(const FRoxiItemStack& Other) const { return !(*this == Other); }

	FString ToString() const;
};
