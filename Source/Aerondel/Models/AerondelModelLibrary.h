#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AerondelModelLibrary.generated.h"

class USkeletalMeshComponent;
class USkeletalMesh;

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
	/** Pose diagnostic (reversible, changes no assets): 0 = normal animation, 1 = player rig held in its bind pose,
	 *  2 = every rig in bind pose. Console: aerondel.Rig.BindPose N, launch arg -AerondelBindPose (= 1), or Settings.
	 *  If the bent player stands straight in bind pose, the imported animation clips (not the mesh) are at fault. */
	static int32 GetBindPoseMode();
	static void SetBindPoseMode(int32 Mode, USkeletalMeshComponent* PlayerMesh);
	UFUNCTION(BlueprintCallable, Category="Aerondel|Models") static void ReloadRigTable();
	/** Scales the applied mesh to HeightCm (largest of height / 0.6 x length for quadrupeds), puts its feet at -HalfHeight
	 *  and turns it by the rigs.json "yaw" (default -90: imported glTF characters face +Y). */
	UFUNCTION(BlueprintCallable, Category="Aerondel|Models") static void FitRig(USkeletalMeshComponent* Comp, float HeightCm, float HalfHeight);
	/** Largest extent of the skeleton's bone positions: reference pose (Posed = nullptr) or that component's current pose. */
	static double BoneExtent(const USkeletalMesh* Mesh, const USkeletalMeshComponent* Posed);
	/** High/Ultra only: swaps each material slot for a dynamic instance of the matching realism master
	 *  (/Game/Gielinor/Materials/Real/MI_Real_<kind>: skin = subsurface profile, fur/cloth/hair = cloth shading with sheen,
	 *  leather/metal/horn/wood/bone/hide = default lit), keeping the original base colour/texture and adding a micro detail
	 *  normal. Slot kinds follow the web js/realism.js surfaceKind(). Low/Quest: no-op (original cheap materials).
	 *  Called by ApplyRig; missing assets (Build Realism Materials not run) are a no-op. */
	UFUNCTION(BlueprintCallable, Category="Aerondel|Models") static void ApplyRealism(USkeletalMeshComponent* Comp, const FString& RigKey);
	/** Web rigs.js T(rig, tint, k): blend every material's base colour toward Color by K (0..1). Cache keeps the untinted colours so it can be re-applied (pass nullptr for one-shot). */
	static void ApplyTint(USkeletalMeshComponent* Comp, const FLinearColor& Color, float K, TArray<FLinearColor>* Cache = nullptr);
	/** Web MONSTER_RIGS tint for a mob id (e.g. ice_giant); false when the rig is untinted. */
	static bool MobTint(FName MobId, FLinearColor& OutColor, float& OutK);
	/** True while a one-shot (attack/hit) is still playing, so callers don't cut it off with the base state. */
	UFUNCTION(BlueprintPure, Category="Aerondel|Models") static bool IsBusy(USkeletalMeshComponent* Comp);
};
