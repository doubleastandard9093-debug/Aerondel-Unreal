#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/AerondelInteractable.h"
#include "InteractableActor.generated.h"

class UStaticMeshComponent;
class AAerondelPlayerState;

/** Shared base: a mesh root, a display name, range checks and a "keep doing it" timer used by gather/craft loops. */
UCLASS(Abstract)
class AERONDEL_API AInteractableActor : public AActor, public IAerondelInteractable
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
	virtual void Interact(AActor* InInstigator) override {}
	virtual void StopInteract(AActor* InInstigator) override;

	static bool InRange(const AActor* InInstigator, const IAerondelInteractable* Target, float Slack = 1.5f);

protected:
	/** Starts a repeating action; OnLoopTick returns false to stop. Auto-stops if the instigator walks off. */
	void StartLoop(AActor* InInstigator, float Seconds);
	virtual bool OnLoopTick(AActor* InInstigator) { return false; }
	void Msg(const FText& T) const;
	static AAerondelPlayerState* PS(AActor* InInstigator);

	FTimerHandle LoopTimer;
	TWeakObjectPtr<AActor> Current;
private:
	void LoopTick();
};
