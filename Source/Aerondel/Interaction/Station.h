#pragma once
#include "CoreMinimal.h"
#include "Interaction/InteractableActor.h"
#include "Station.generated.h"

class URecipeDef;

/** Furnace / anvil / range. Picks a recipe (explicit or first craftable) and "makes all" on a timer. */
UCLASS()
class AERONDEL_API AStation : public AInteractableActor
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Station") EStationType StationType = EStationType::Furnace;

	virtual FText GetInteractLabel() const override;
	virtual void Interact(AActor* InInstigator) override;

	/** Craft one (Count=1) or all you can (Count=-1) of a recipe. */
	UFUNCTION(BlueprintCallable, Category="Station") void CraftRecipe(AActor* InInstigator, URecipeDef* Recipe, int32 Count = -1);
	UFUNCTION(BlueprintPure, Category="Station") TArray<URecipeDef*> GetRecipes() const;
	UFUNCTION(BlueprintPure, Category="Station") bool CanCraft(AActor* InInstigator, const URecipeDef* Recipe, bool bTell = false) const;
	/** How many full crafts the instigator can afford right now. */
	UFUNCTION(BlueprintPure, Category="Station") int32 CountAffordable(AActor* InInstigator, const URecipeDef* Recipe) const;

protected:
	virtual bool OnLoopTick(AActor* InInstigator) override;
private:
	UPROPERTY() TObjectPtr<URecipeDef> Active;
	int32 CraftLeft = -1; // -1 = make all
};
