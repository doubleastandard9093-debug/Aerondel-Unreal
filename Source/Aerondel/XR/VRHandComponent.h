#pragma once
#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "Data/AerondelTypes.h"
#include "VRHandComponent.generated.h"

class UStaticMeshComponent; class UStaticMesh; class UMotionControllerComponent;

/**
 * One per hand, attached to the grip MotionController. Owns:
 *  - grab/poke: overlap sphere -> VR interaction bridge (same Interact() the desktop click uses),
 *  - held tool: gripping in empty space draws the context tool (axe near trees, pickaxe near rocks, else weapon),
 *  - tool swing: hand velocity through an interactable -> OnToolHit(Tool, Power),
 *  - hand tracking: OpenXR pinch (thumb-index distance) acts as grip when controllers are put down.
 */
UCLASS(ClassGroup=(Aerondel), meta=(BlueprintSpawnableComponent))
class AERONDEL_API UVRHandComponent : public USphereComponent
{
	GENERATED_BODY()
public:
	UVRHandComponent();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VR") EControllerHand Hand = EControllerHand::Right;
	/** cm/s at which a swing counts as a hit, and the speed at which Power reaches 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VR|Swing") float MinSwingSpeed = 180.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VR|Swing") float FullPowerSpeed = 600.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VR|Swing") float HitCooldown = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VR|Hands") float PinchCm = 2.5f;

	UFUNCTION(BlueprintCallable, Category="VR") void GripPressed();
	UFUNCTION(BlueprintCallable, Category="VR") void GripReleased();
	UFUNCTION(BlueprintCallable, Category="VR") void TriggerPressed();
	UFUNCTION(BlueprintPure, Category="VR") bool IsHoldingTool() const { return bHolding; }
	UFUNCTION(BlueprintPure, Category="VR") FVector GetVelocity() const { return Velocity; }

	virtual void BeginPlay() override;
	virtual void TickComponent(float Dt, ELevelTick TickType, FActorComponentTickFunction* Fn) override;

private:
	AActor* BestOverlap() const;
	void DrawTool();
	void UpdateHandTracking();
	UFUNCTION() void OnHandOverlap(UPrimitiveComponent* Comp, AActor* Other, UPrimitiveComponent* OtherComp, int32 Body, bool bSweep, const FHitResult& Hit);

	UPROPERTY() TObjectPtr<UStaticMeshComponent> ToolMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> PlaceholderToolMesh;
	FTimerHandle HapticTimer;
	bool bHolding = false;
	EToolType HeldTool = EToolType::None; // None while holding = weapon
	FVector LastPos = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	float Cooldown = 0.f;
	bool bPinching = false;
};
