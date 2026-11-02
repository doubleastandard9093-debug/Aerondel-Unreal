#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Data/AerondelTypes.h"
#include "VRInteractionBridge.generated.h"

/**
 * Translates VR verbs into the shared IAerondelInteractable calls, so grab, poke, trigger
 * and desktop click all run the exact same gather/craft/talk code paths.
 */
UCLASS()
class AERONDEL_API UVRInteractionBridge : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Grab or poke: GroundItem -> take; NPC -> talk; Door -> open; node/station -> start loop; mob -> target. */
	UFUNCTION(BlueprintCallable, Category="Aerondel|XR") static bool GrabOrPoke(AActor* Pawn, AActor* Target);
	/** Tool/weapon swing with Power 0..1. Mobs respect the player's combat cooldown. */
	UFUNCTION(BlueprintCallable, Category="Aerondel|XR") static bool SwingHit(AActor* Pawn, AActor* Target, EToolType Tool, float Power);
	/** Which tool the player would draw near Target (axe near trees, etc.). None = weapon. */
	UFUNCTION(BlueprintPure, Category="Aerondel|XR") static EToolType ContextTool(const AActor* Target);
};
