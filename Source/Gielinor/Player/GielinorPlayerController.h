#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GielinorPlayerController.generated.h"

struct FInputActionValue;

/** Mode-agnostic controller: survives pawn swaps. Handles desktop cursor clicks and player death/respawn. */
UCLASS()
class GIELINOR_API AGielinorPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	AGielinorPlayerController();
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;

	/** Hovered interactable's label for the desktop tooltip ("Chop down Tree"). */
	UFUNCTION(BlueprintPure, Category="Gielinor") FText GetHoverText() const { return HoverText; }

	virtual void PlayerTick(float Dt) override;
private:
	void OnClick(const FInputActionValue& V);
	UFUNCTION() void OnPlayerDied(AActor* Killer);
	FText HoverText;
	FVector RespawnPoint = FVector::ZeroVector;
};
