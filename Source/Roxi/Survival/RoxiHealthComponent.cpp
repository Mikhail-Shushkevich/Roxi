// Copyright Epic Games, Inc. All Rights Reserved.

#include "Survival/RoxiHealthComponent.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"

URoxiHealthComponent::URoxiHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void URoxiHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.Condition = COND_OwnerOnly;
	DOREPLIFETIME_WITH_PARAMS_FAST(URoxiHealthComponent, Health, Params);
}

void URoxiHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner()->HasAuthority())
	{
		Health = MaxHealth;
		MARK_PROPERTY_DIRTY_FROM_NAME(URoxiHealthComponent, Health, this);
	}
}

void URoxiHealthComponent::ApplyDamage(float Amount, AController* Instigator)
{
	if (!GetOwner()->HasAuthority() || bDead || Amount <= 0.f)
	{
		return;
	}

	Health = FMath::Max(0.f, Health - Amount);
	MARK_PROPERTY_DIRTY_FROM_NAME(URoxiHealthComponent, Health, this);

	if (Health <= 0.f)
	{
		bDead = true;
		OnDeath.Broadcast(this, Instigator);
	}
}
