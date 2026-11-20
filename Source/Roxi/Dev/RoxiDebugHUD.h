// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RoxiDebugHUD.generated.h"

/**
 * DEV ONLY (Т9). Временный текстовый интерфейс: инвентарь, пояс, открытый контейнер,
 * подсказка взаимодействия. Финальный UI — отдельная задача. В Shipping ничего не рисует.
 */
UCLASS()
class ROXI_API ARoxiDebugHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawLine(const FString& Text, float& Y, const FLinearColor& Color = FLinearColor::White);
};
