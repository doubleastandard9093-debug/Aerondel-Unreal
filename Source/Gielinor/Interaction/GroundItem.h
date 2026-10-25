#pragma once
#include "CoreMinimal.h"
#include "Interaction/InteractableActor.h"
#include "GroundItem.generated.h"

class UItemDef;

/** Dropped loot. Click to take (desktop) or grab it (VR). Despawns after a while. */
UCLASS()
class GIELINOR_API AGroundItem : public AInteractableActor
{
	GENERATED_BODY()
public:
	AGroundItem();
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") TObjectPtr<UItemDef> Item;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") int32 Count = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") float DespawnSeconds = 120.f;

	virtual FText GetInteractLabel() const override;
	virtual void Interact(AActor* InInstigator) override;
	virtual void BeginPlay() override;

	static AGroundItem* Spawn(UWorld* InWorld, UItemDef* InItem, int32 InCount, const FVector& Where);
};
