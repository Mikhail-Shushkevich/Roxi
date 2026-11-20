// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoxiGameMode.h"
#include "RoxiCharacter.h"
#include "UObject/ConstructorHelpers.h"

ARoxiGameMode::ARoxiGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
