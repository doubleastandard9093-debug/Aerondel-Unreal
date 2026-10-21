#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GielinorVRPawn.generated.h"

class UCameraComponent; class UMotionControllerComponent; class UCapsuleComponent; class UVRHandComponent;
class UWidgetInteractionComponent; class UWidgetComponent; class UPostProcessComponent;
class UInstancedStaticMeshComponent; class UStaticMeshComponent; class UPlayerCombatComponent;
struct FInputActionValue;

/**
 * VR pawn (OpenXR): HMD camera, grip + aim motion controllers (hand-tracking aware), two VRHand components,
 * teleport arc projected onto the NavMesh, smooth locomotion with collision, snap or smooth turn,
 * a comfort vignette while moving, a wrist UMG panel and a WidgetInteraction laser.
 * Root is a capsule at floor level so the pawn collides with walls (doors still need opening).
 */
UCLASS()
class GIELINOR_API AGielinorVRPawn : public APawn
{
	GENERATED_BODY()
public:
	AGielinorVRPawn();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VR") TObjectPtr<UCapsuleComponent> Body;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VR") TObjectPtr<USceneComponent> VROrigin;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VR") TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VR") TObjectPtr<UMotionControllerComponent> LeftGrip;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VR") TObjectPtr<UMotionControllerComponent> RightGrip;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VR") TObjectPtr<UMotionControllerComponent> LeftAim;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VR") TObjectPtr<UMotionControllerComponent> RightAim;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VR") TObjectPtr<UVRHandComponent> LeftHand;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VR") TObjectPtr<UVRHandComponent> RightHand;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VR|UI") TObjectPtr<UWidgetInteractionComponent> Laser;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VR|UI") TObjectPtr<UWidgetComponent> WristPanel;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VR|Comfort") TObjectPtr<UPostProcessComponent> Vignette;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VR|Teleport") TObjectPtr<UInstancedStaticMeshComponent> ArcDots;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="VR|Teleport") TObjectPtr<UStaticMeshComponent> TeleportMarker;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat") TObjectPtr<UPlayerCombatComponent> Combat;

	// Comfort options (expose in a settings menu later).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VR|Comfort") bool bSnapTurn = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VR|Comfort") float SnapAngle = 45.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VR|Comfort") float SmoothTurnDegPerSec = 120.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VR|Comfort") float MoveSpeed = 300.f; // cm/s
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VR|Comfort") bool bVignette = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VR|Comfort") float VignetteMax = 1.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VR|Teleport") float TeleportLaunchSpeed = 900.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VR|UI") TSubclassOf<class UUserWidget> WristWidgetClass;

	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;
	virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
	virtual void PawnClientRestart() override;

private:
	void OnMove(const FInputActionValue& V);
	void OnMoveDone(const FInputActionValue& V) { MoveInput = FVector2D::ZeroVector; }
	void OnTurn(const FInputActionValue& V);
	void OnTurnDone(const FInputActionValue& V) { bSnapArmed = true; TurnInput = 0.f; }
	void OnTeleportAim(const FInputActionValue& V) { bAimingTeleport = true; }
	void OnTeleportRelease(const FInputActionValue& V);
	void OnGripL(const FInputActionValue& V);
	void OnGripLEnd(const FInputActionValue& V);
	void OnGripR(const FInputActionValue& V);
	void OnGripREnd(const FInputActionValue& V);
	void OnTriggerL(const FInputActionValue& V);
	void OnTriggerR(const FInputActionValue& V);
	void OnTriggerREnd(const FInputActionValue& V);
	void OnMenu(const FInputActionValue& V);

	void RotateAroundHead(float Degrees);
	void UpdateTeleportArc();
	void ApplyVignette(float Dt, bool bMoving);

	FVector2D MoveInput = FVector2D::ZeroVector;
	float TurnInput = 0.f;
	bool bSnapArmed = true;
	bool bAimingTeleport = false;
	bool bTeleportValid = false;
	FVector TeleportTarget = FVector::ZeroVector;
	float VignetteCurrent = 0.f;
};
