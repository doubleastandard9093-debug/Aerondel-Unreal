#pragma once
#include "CoreMinimal.h"
#include "Interaction/InteractableActor.h"
#include "Door.generated.h"

/** Hinged door: the actor origin is the hinge, Mesh is the leaf (offset by half its width). */
UCLASS()
class GIELINOR_API ADoor : public AInteractableActor
{
	GENERATED_BODY()
public:
	ADoor();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Door") float OpenAngle = 95.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Door") float SwingSpeed = 240.f; // deg/s
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Door") bool bStartOpen = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Door") TObjectPtr<USceneComponent> Hinge;

	virtual FText GetInteractLabel() const override;
	virtual void Interact(AActor* InInstigator) override;
	virtual FVector GetInteractPoint() const override;
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;
	UFUNCTION(BlueprintPure, Category="Door") bool IsOpen() const { return bOpen; }
private:
	bool bOpen = false;
	float Yaw = 0.f;
	float Sign = 1.f;
};
