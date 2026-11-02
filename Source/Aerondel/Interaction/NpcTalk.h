#pragma once
#include "CoreMinimal.h"
#include "Interaction/InteractableActor.h"
#include "NpcTalk.generated.h"

class UNpcDef;

UCLASS()
class AERONDEL_API ANpcTalk : public AInteractableActor
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Npc") TObjectPtr<UNpcDef> Definition;
	/** JSON-only NPCs (no NpcDef asset): resolved from the GameDatabase at BeginPlay when Definition is empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Npc") FName NpcId;
	virtual void BeginPlay() override;
	virtual FText GetInteractLabel() const override;
	virtual void Interact(AActor* InInstigator) override;
	virtual void OnConstruction(const FTransform& Xf) override;
	/** Rigged body from rigs.json "npcs" (by NPC id); the static Mesh stays as the hidden click collider. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Npc") TObjectPtr<class USkeletalMeshComponent> Rig;
	ANpcTalk();
private:
	int32 LineIndex = 0;
};
