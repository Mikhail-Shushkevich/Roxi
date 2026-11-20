// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class APawn;
class UWorld;

/** Общие проверки размещения для размещаемых объектов и построек (К3, К6). Вызываются на сервере. */
namespace RoxiPlacement
{
	/** Тег объектов, мешающих размещению (размещаемые объекты, элементы построек). */
	extern ROXI_API const FName BlockerTag;
	/** Тег элементов построек (дополнительно к BlockerTag). */
	extern ROXI_API const FName BuildingPieceTag;

	/** Максимальная дистанция от игрока до точки размещения, см. */
	constexpr float MaxPlaceDistance = 700.f;

	/** Запрет по GDD §6.1: зоны POI и вода. Возвращает причину или пустую строку. */
	ROXI_API FString CheckRestrictedArea(const UWorld* World, const FVector& Location);

	/** Пересекает ли объём игроков, размещаемые объекты или элементы построек. */
	ROXI_API bool OverlapsBlockers(const UWorld* World, const FVector& Center, const FVector& HalfExtent, const FQuat& Rotation,
		bool bIgnoreBuildingPieces = false);

	/** Дистанция от игрока. */
	ROXI_API bool IsWithinReach(const APawn* Placer, const FVector& Location);
}
