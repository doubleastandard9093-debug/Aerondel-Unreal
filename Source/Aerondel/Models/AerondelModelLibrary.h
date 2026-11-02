#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AerondelModelLibrary.generated.h"

class USkeletalMeshComponent;

/** Rigged Lite models in Unreal. Content/Data/rigs.json is written by Content/Python/import_lite_models.py:
 *    rigs:     key -> { mesh: SkeletalMesh path, anims: { idle|walk|run|attack|hit|death|gather: AnimSequence path } }
 *    monsters: mob id -> rig key        (web rigs.js MONSTER_RIGS)
 *    npcs:     npc role -> rig key      (web rigs.js NPC role mapping)
 *    player:   rig key
 *  Playback uses AnimationSingleNode (no AnimBP needed): SetAnimState swaps the looping sequence per state.
 *  On the Low/Quest tier rigs tick only when rendered, use URO and cast no dynamic shadow. Missing assets are a no-op. */
UCLASS()
class AERONDEL_API UAerondelModelLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Key is a rig key, or "mob:<id>", "npc:<role>", "player". Returns false when the rig is not imported. */
	UFUNCTION(BlueprintCallable, Category="Aerondel|Models") static bool ApplyRig(USkeletalMeshComponent* Comp, const FString& Key);
	/** idle, walk, run, attack, hit, death, gather. Falls back to idle; death plays once. */
	UFUNCTION(BlueprintCallable, Category="Aerondel|Models") static void SetAnimState(USkeletalMeshComponent* Comp, FName State);
	UFUNCTION(BlueprintPure, Category="Aerondel|Models") static FString ResolveKey(const FString& Key);
	UFUNCTION(BlueprintCallable, Category="Aerondel|Models") static void ReloadRigTable();
};
