#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/GielinorInteractable.h"
#include "InteractableActor.generated.h"

class UStaticMeshComponent;
class AGielinorPlayerState;

/** Shared base: a mesh root, a display name, range checks and a "keep doing it" timer used by gather/craft loops. */
UCLASS(Abstract)
class GIELINOR_API AInteractableActor : public AActor, public IGielinorInteractable
{
	GENERATED_BODY()
public:
	AInteractableActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interact") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interact") TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interact") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Interact") float Range = 200.f;

	virtual FText GetInteractLabel() const override { return DisplayName; }
	virtual float GetInteractRange() const override { return Range; }
	virtual FVector GetInteractPoint() const override { return GetActorLocation(); }
	virtual void Interact(AActor* Instigator) override {}
	virtual void StopInteract(AActor* Instigator) override;

	static bool InRange(const AActor* Instigator, const IGielinorInteractable* Target, float Slack = 1.5f);

protected:
	/** Starts a repeating action; OnLoopTick returns false to stop. Auto-stops if the instigator walks off. */
	void StartLoop(AActor* Instigator, float Seconds);
	virtual bool OnLoopTick(AActor* Instigator) { return false; }
	void Msg(const FText& T) const;
	static AGielinorPlayerState* PS(AActor* Instigator);

	FTimerHandle LoopTimer;
	TWeakObjectPtr<AActor> Current;
private:
	void LoopTick();
};
