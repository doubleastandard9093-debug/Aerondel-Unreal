#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GielinorCharacter.generated.h"

class USpringArmComponent; class UCameraComponent; class UPlayerCombatComponent; class UStaticMeshComponent;
class IGielinorInteractable;
struct FInputActionValue;

/**
 * Desktop / gamepad third-person pawn. WASD or left stick moves (cancels click-to-move),
 * mouse right-drag or right stick orbits, wheel zooms, left-click walks or interacts (via the controller).
 */
UCLASS()
class GIELINOR_API AGielinorCharacter : public ACharacter
{
	GENERATED_BODY()
public:
	AGielinorCharacter();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera") TObjectPtr<USpringArmComponent> SpringArm;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera") TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat") TObjectPtr<UPlayerCombatComponent> Combat;
	/** Placeholder body until a skeletal mesh is imported. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Body") TObjectPtr<UStaticMeshComponent> PlaceholderBody;

	UPROPERTY(EditAnywhere, Category="Camera") float MinZoom = 250.f;
	UPROPERTY(EditAnywhere, Category="Camera") float MaxZoom = 1400.f;
	UPROPERTY(EditAnywhere, Category="Camera") float ZoomStep = 120.f;

	/** Walk to the target then Interact() (desktop click / gamepad interact). */
	void QueueInteraction(AActor* Target);
	void CancelInteraction();
	/** Nearest interactable within Radius in front of the pawn (gamepad "A"). */
	AActor* FindNearestInteractable(float Radius = 300.f) const;

	virtual void Tick(float Dt) override;
	virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
	virtual void PawnClientRestart() override;

private:
	void OnMove(const FInputActionValue& V);
	void OnLook(const FInputActionValue& V);
	void OnZoom(const FInputActionValue& V);
	void OnOrbitStart(const FInputActionValue& V) { bOrbiting = true; }
	void OnOrbitEnd(const FInputActionValue& V) { bOrbiting = false; }
	void OnInteract(const FInputActionValue& V);

	TWeakObjectPtr<AActor> Pending;
	bool bOrbiting = false;
	float TargetArm = 700.f;
};
