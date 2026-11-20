// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RoxiHealthComponent.generated.h"

class AController;
class URoxiHealthComponent;

DECLARE_MULTICAST_DELEGATE_TwoParams(FRoxiDeathEvent, URoxiHealthComponent* /*Health*/, AController* /*Killer*/);

/**
 * Здоровье (В7: только здоровье, 100 единиц). Остальные показатели выживания — открытый вопрос GDD №1.
 * Сервер авторитетен; значение реплицируется только владельцу (для интерфейса).
 */
UCLASS(ClassGroup = (Roxi), meta = (BlueprintSpawnableComponent))
class ROXI_API URoxiHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URoxiHealthComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	bool IsDead() const { return bDead; }

	/** Максимум здоровья, применяемый в BeginPlay. Для конструкторов и отложенного спавна. */
	void SetInitialMaxHealth(float InMaxHealth) { MaxHealth = FMath::Max(1.f, InMaxHealth); Health = MaxHealth; }

	/** Сервер: установить текущее здоровье (загрузка сохранения). */
	void SetCurrentHealth(float NewHealth);

	/** Сервер: нанести урон. При достижении 0 — смерть (один раз). */
	void ApplyDamage(float Amount, AController* Instigator);

	/** Сервер: персонаж умер. */
	FRoxiDeathEvent OnDeath;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Health", meta = (ClampMin = 1))
	float MaxHealth = 100.f;

	UPROPERTY(Replicated)
	float Health = 100.f;

private:
	/** Любой урон по владельцу (AActor::TakeDamage -> OnTakeAnyDamage). */
	UFUNCTION()
	void HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const class UDamageType* DamageType,
		AController* InstigatedBy, AActor* DamageCauser);

	bool bDead = false;
};
