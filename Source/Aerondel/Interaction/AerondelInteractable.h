#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Data/AerondelTypes.h"
#include "AerondelInteractable.generated.h"

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UAerondelInteractable : public UInterface { GENERATED_BODY() };

/**
 * One interaction contract for every input mode:
 *  - Desktop: click -> walk into range -> Interact().
 *  - VR: grab / poke / trigger -> Interact(); a tracked tool swing -> OnToolHit().
 * Implementations never know which rig triggered them; the instigator is just the pawn.
 */
class AERONDEL_API IAerondelInteractable
{
	GENERATED_BODY()
public:
	virtual FText GetInteractLabel() const = 0;            // "Chop down Tree", "Talk-to Granny Hilda"
	virtual float GetInteractRange() const { return 200.f; } // cm
	virtual bool CanInteract(AActor* InInstigator) const { return true; }
	virtual void Interact(AActor* InInstigator) = 0;          // start (may be a repeating action)
	virtual void StopInteract(AActor* InInstigator) {}        // moved away / cancelled
	/** VR tool swing landed with Power 0..1 (from controller velocity). Defaults to a normal interact. */
	virtual void OnToolHit(AActor* InInstigator, EToolType Tool, float Power) { Interact(InInstigator); }
	/** World-space point the desktop pawn walks to / VR highlights. */
	virtual FVector GetInteractPoint() const = 0;
};
