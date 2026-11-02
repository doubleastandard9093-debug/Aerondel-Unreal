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

	/** UI path: craft a specific recipe. */
	UFUNCTION(BlueprintCallable, Category="Station") void CraftRecipe(AActor* InInstigator, URecipeDef* Recipe);
	UFUNCTION(BlueprintPure, Category="Station") TArray<URecipeDef*> GetRecipes() const;

protected:
	virtual bool OnLoopTick(AActor* InInstigator) override;
private:
	bool CanCraft(AActor* InInstigator, const URecipeDef* R, bool bTell) const;
	UPROPERTY() TObjectPtr<URecipeDef> Active;
};
