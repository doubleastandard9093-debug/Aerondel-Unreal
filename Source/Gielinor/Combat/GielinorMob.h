#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interaction/GielinorInteractable.h"
#include "GielinorMob.generated.h"

class UMobDef; class UHealthComponent; class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EMobState : uint8 { Idle, Wander, Chase, Attack, Return, Dead };

/**
 * Data-driven mob with a tiny code state machine (no Behavior Tree asset needed):
 * Idle -> Wander -> (aggro or attacked) Chase -> Attack -> (target lost / leash) Return -> Idle; Dead -> respawn.
 * Movement uses the default AIController + NavMesh.
 */
UCLASS()
class GIELINOR_API AGielinorMob : public ACharacter, public IGielinorInteractable
{
	GENERATED_BODY()
public:
	AGielinorMob();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mob") TObjectPtr<UMobDef> Definition;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mob") TObjectPtr<UHealthComponent> Health;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mob") TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mob") float WanderRadius = 500.f;

	// IGielinorInteractable: "Attack Goblin (level 2)".
	virtual FText GetInteractLabel() const override;
	virtual float GetInteractRange() const override { return 180.f; }
	virtual bool CanInteract(AActor* InInstigator) const override { return State != EMobState::Dead; }
	virtual void Interact(AActor* InInstigator) override;
	virtual void OnToolHit(AActor* InInstigator, EToolType Tool, float Power) override;
	virtual FVector GetInteractPoint() const override { return GetActorLocation(); }

	/** Player melee hit (desktop auto-attack or VR swing with Power 0..1). Handles xp and aggro. */
	UFUNCTION(BlueprintCallable, Category="Mob") void ReceivePlayerHit(AActor* Attacker, float Power = 1.f);
	UFUNCTION(BlueprintPure, Category="Mob") EMobState GetState() const { return State; }

	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;
	virtual void OnConstruction(const FTransform& Xf) override;

private:
	UFUNCTION() void HandleDied(AActor* Killer);
	void SetState(EMobState S);
	void Respawn();
	void TryAttack();
	AActor* FindPlayerInAggro() const;
	class AAIController* AI() const;

	EMobState State = EMobState::Idle;
	TWeakObjectPtr<AActor> Target;
	FVector Home;
	float StateTime = 0.f;
	float AttackCooldown = 0.f;
	FTimerHandle RespawnTimer;
};
