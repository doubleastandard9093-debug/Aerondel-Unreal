#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AerondelCharacter.generated.h"

class USpringArmComponent; class UCameraComponent; class UPlayerCombatComponent; class UStaticMeshComponent; class UAerondelGamepadComponent;
class IAerondelInteractable;
struct FInputActionValue;

/**
 * Desktop / gamepad third-person pawn. WASD or left stick moves (cancels click-to-move),
 * mouse right-drag or right stick orbits, wheel zooms, left-click walks or interacts (via the controller).
 */
UCLASS()
class AERONDEL_API AAerondelCharacter : public ACharacter
{
	GENERATED_BODY()
public:
	AAerondelCharacter();
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera") TObjectPtr<USpringArmComponent> SpringArm;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera") TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat") TObjectPtr<UPlayerCombatComponent> Combat;
	/** Placeholder body until a skeletal mesh is imported. */
	/** Xbox / XInput controller layout (runtime mapping context above IMC_Desktop). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Input") TObjectPtr<UAerondelGamepadComponent> Gamepad;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Body") TObjectPtr<UStaticMeshComponent> PlaceholderBody;

	UPROPERTY(EditAnywhere, Category="Camera") float MinZoom = 250.f;
	UPROPERTY(EditAnywhere, Category="Camera") float MaxZoom = 1400.f;
	UPROPERTY(EditAnywhere, Category="Camera") float ZoomStep = 120.f;

	/** Walk to the target then Interact() (desktop click / gamepad interact). */
	void QueueInteraction(AActor* Target);
	void CancelInteraction();
	/** Positive = zoom out (cm of spring-arm length). */
	void AddZoom(float Cm) { TargetArm = FMath::Clamp(TargetArm + Cm, MinZoom, MaxZoom); }
	/** Nearest interactable within Radius in front of the pawn (gamepad "A"). */
	AActor* FindNearestInteractable(float Radius = 300.f) const;

	virtual void Tick(float Dt) override;
	virtual void BeginPlay() override;
	/** Plays a one-shot on the rigged body (attack, hit, gather); no-op with the placeholder capsule. */
	void PlayBodyAnim(FName State);
	virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
	virtual void PawnClientRestart() override;

private:
	bool bRigged = false;
	/** v22 armour tints: body-armour Tint blended onto the rig (polled, so no equipment delegate wiring). */
	void RefreshGearTint();
	FTimerHandle GearTintTimer;
	TWeakObjectPtr<const class UItemDef> TintedBody;
	TArray<FLinearColor> UntintedColors;
	void OnMove(const FInputActionValue& V);
	void OnLook(const FInputActionValue& V);
	void OnZoom(const FInputActionValue& V);
	void OnOrbitStart(const FInputActionValue& V) { bOrbiting = true; }
	void OnOrbitEnd(const FInputActionValue& V) { bOrbiting = false; }
	void OnInteract(const FInputActionValue& V);
	/** Ventures keys (U Overflare, J hub, L Tether, N Attunement super, C Glidewalk dash): runtime actions + context, like the gamepad layout. */
	void BuildVentureKeys();
	void OnVentureKey(FName Key);
	UPROPERTY(Transient) TObjectPtr<class UInputMappingContext> VentureContext;
	UPROPERTY(Transient) TArray<TObjectPtr<class UInputAction>> VentureActions;
	FLinearColor AppliedTint = FLinearColor::White;

	TWeakObjectPtr<AActor> Pending;
	bool bOrbiting = false;
	float TargetArm = 700.f;
};
