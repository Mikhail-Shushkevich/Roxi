// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Building/RoxiDeployable.h"
#include "RoxiSleepingBag.generated.h"

/**
 * Спальник — точка возрождения владельца (GDD §5.3, К4).
 * Перезарядка 300 с (С5), не более 15 на игрока (С6), уничтожается уроном (С7а).
 */
UCLASS()
class ROXI_API ARoxiSleepingBag : public ARoxiDeployable
{
	GENERATED_BODY()

public:
	ARoxiSleepingBag();

	static constexpr int32 MaxPerOwner = 15;

	/** Уникальный ID точки на сервере (0 — «случайная точка», поэтому начинается с 1). */
	int32 GetPointId() const { return PointId; }
	const FText& GetDisplayName() const { return DisplayName; }

	/** Время мира сервера, когда точка снова доступна. */
	double GetAvailableAt() const { return AvailableAt; }
	bool IsAvailable() const;
	/** Сервер: точка использована — начать перезарядку. */
	void MarkUsed();

	/** Сервер: точка возрождения по ID. */
	static ARoxiSleepingBag* FindById(const UWorld* World, int32 Id);
	/** Сервер: все точки владельца. */
	static TArray<ARoxiSleepingBag*> FindByOwner(const UWorld* World, const FString& InOwnerId);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual bool CanBePlacedBy(const UWorld* World, const FString& InOwnerId, FString& OutReason) const override;

	/** Перезарядка после возрождения, секунды (С5). */
	UPROPERTY(EditDefaultsOnly, Category = "Respawn")
	float RespawnCooldown = 300.f;

	UPROPERTY(EditDefaultsOnly, Category = "Respawn")
	FText DisplayName;

private:
	void NotifyOwnerChanged() const;

	int32 PointId = 0;
	double AvailableAt = 0.0;
};

/** Кровать: та же точка возрождения с меньшей перезарядкой (120 с, С5). */
UCLASS()
class ROXI_API ARoxiBed : public ARoxiSleepingBag
{
	GENERATED_BODY()

public:
	ARoxiBed();
};
