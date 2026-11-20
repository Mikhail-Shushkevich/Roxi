// Copyright Epic Games, Inc. All Rights Reserved.

// DEV ONLY. Вспомогательные консольные команды для автоматических тестов.

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Roxi.h"

#if !UE_BUILD_SHIPPING
namespace RoxiDevCommands
{
	/** Выполнить команду в текущем игровом мире: через локального игрока (Exec-функции контроллера) или как консольную. */
	static void ExecInGameWorld(const FString& Command)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (!World || !World->IsGameWorld())
			{
				continue;
			}
			UE_LOG(LogRoxi, Log, TEXT("roxi.Delay: executing '%s'"), *Command);
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				PC->ConsoleCommand(Command, true);
			}
			else
			{
				GEngine->Exec(World, *Command);
			}
			return;
		}
	}

	static FAutoConsoleCommand DelayCmd(
		TEXT("roxi.Delay"),
		TEXT("roxi.Delay <Seconds> <Command...> — run a console command later (for -ExecCmds test scripts)."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() < 2)
			{
				UE_LOG(LogRoxi, Warning, TEXT("Usage: roxi.Delay <Seconds> <Command...>"));
				return;
			}
			const float Delay = FCString::Atof(*Args[0]);
			const FString Command = FString::Join(TArrayView<const FString>(Args).RightChop(1), TEXT(" "));
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Command](float)
			{
				ExecInGameWorld(Command);
				return false; // один раз
			}), Delay);
		}));
}
#endif
