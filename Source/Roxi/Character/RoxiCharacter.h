// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "RoxiCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class URoxiInventoryComponent;
class URoxiHealthComponent;
class URoxiCraftingComponent;
struct FInputActionValue;

UCLASS(config=Game)
class ROXI_API ARoxiCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> FollowCamera;
	
	/** MappingContext */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> LookAction;

	/** Interact Input Action: поднять предмет / открыть контейнер */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> InteractAction;

	/** Основной инвентарь (В1: 24 слота) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Inventory, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URoxiInventoryComponent> Inventory;

	/** Пояс — слоты быстрого доступа (В1: 6 слотов) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Inventory, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URoxiInventoryComponent> Belt;

	/** Здоровье (В7) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Survival, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URoxiHealthComponent> Health;

	/** Очередь крафта (К2) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Crafting, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URoxiCraftingComponent> Crafting;

public:
	ARoxiCharacter();
	

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Called for interact input */
	void Interact();
			

protected:

	virtual void NotifyControllerChanged() override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	URoxiInventoryComponent* GetInventory() const { return Inventory; }
	URoxiInventoryComponent* GetBelt() const { return Belt; }
	URoxiHealthComponent* GetHealth() const { return Health; }

	URoxiCraftingComponent* GetCrafting() const { return Crafting; }

	// --- Спящий персонаж (GDD §5.4): игрок вышел, персонаж остался в мире ---

	bool IsSleeping() const { return bSleeping; }
	/** Только на сервере: постоянный ID владельца спящего персонажа. */
	const FString& GetSleeperId() const { return SleeperId; }
	const FString& GetSleeperName() const { return SleeperName; }

	/** Сервер: уснуть (контроллер уже отсоединён). Очередь крафта отменяется с возвратом (П5). */
	void EnterSleep(const FString& PlayerId, const FString& PlayerName);
	/** Сервер: проснуться перед тем, как владелец снова управляет персонажем. */
	void ExitSleep();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	void HandleDeath(URoxiHealthComponent* DeadHealth, AController* Killer);

	UPROPERTY(Replicated)
	bool bSleeping = false;

	UPROPERTY(Replicated)
	FString SleeperName;

	FString SleeperId;
};

