#pragma once
#include "CoreMinimal.h"
#include "Interaction/InteractableActor.h"
#include "ResourceNode.generated.h"

class UResourceNodeDef; class UItemDef;

/** Tree / rock / fishing spot. Timed gather with a tool check, XP, product, depletion and respawn. */
UCLASS()
class GIELINOR_API AResourceNode : public AInteractableActor
{
	GENERATED_BODY()
public:
	AResourceNode();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resource") TObjectPtr<UResourceNodeDef> Definition;
	/** Shown while depleted (stump / empty rock). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Resource") TObjectPtr<UStaticMeshComponent> DepletedMesh;

	virtual FText GetInteractLabel() const override;
	virtual bool CanInteract(AActor* Instigator) const override { return !bDepleted; }
	virtual void Interact(AActor* Instigator) override;
	virtual void OnToolHit(AActor* Instigator, EToolType Tool, float Power) override;
	virtual void OnConstruction(const FTransform& Xf) override;

	UFUNCTION(BlueprintPure, Category="Resource") bool IsDepleted() const { return bDepleted; }

protected:
	virtual bool OnLoopTick(AActor* Instigator) override;
private:
	UItemDef* FindTool(AActor* Instigator) const;
	bool Validate(AActor* Instigator, UItemDef*& OutTool) const;
	bool Roll(AActor* Instigator, float Bonus);
	void SetDepleted(bool b);
	void Respawn() { SetDepleted(false); }
	bool bDepleted = false;
	FTimerHandle RespawnTimer;
};
