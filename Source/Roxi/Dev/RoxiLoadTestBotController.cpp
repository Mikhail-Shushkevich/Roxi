// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dev/RoxiLoadTestBotController.h"
#include "GameFramework/Character.h"

ARoxiLoadTestBotController::ARoxiLoadTestBotController()
{
	PrimaryActorTick.bCanEverTick = true;
	bWantsPlayerState = false;
}

void ARoxiLoadTestBotController::InitWander(const FVector& InCenter, float InRadius)
{
	Center = InCenter;
	Radius = FMath::Max(InRadius, 100.f);
	TimeToNextDecision = 0.f;
}

void ARoxiLoadTestBotController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return;
	}

	const FVector Location = MyPawn->GetActorLocation();
	TimeToNextDecision -= DeltaSeconds;

	if (FVector::DistSquared2D(Location, Center) > FMath::Square(Radius))
	{
		// Вышел за круг — разворачиваемся к центру, чтобы не упасть с карты.
		MoveDir = (Center - Location).GetSafeNormal2D();
		TimeToNextDecision = FMath::FRandRange(1.f, 2.f);
	}
	else if (TimeToNextDecision <= 0.f)
	{
		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		MoveDir = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
		TimeToNextDecision = FMath::FRandRange(2.f, 4.f);

		if (FMath::FRand() < 0.15f)
		{
			if (ACharacter* BotCharacter = Cast<ACharacter>(MyPawn))
			{
				BotCharacter->Jump();
			}
		}
	}

	MyPawn->AddMovementInput(MoveDir, 1.f);
}
