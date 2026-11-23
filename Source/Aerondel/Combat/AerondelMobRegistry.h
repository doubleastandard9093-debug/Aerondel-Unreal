#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AerondelMobRegistry.generated.h"

class AAerondelMob;

/** A snapshot of live mobs. Inline storage, so the per-tick queries below do not touch the heap for typical counts. */
typedef TArray<AAerondelMob*, TInlineAllocator<64>> FAerondelMobList;

/**
 * Live-mob registry for one game world. AAerondelMob registers in BeginPlay and leaves in EndPlay, so the periodic
 * queries (Relevance, MobsNear, ShadeFoesNear, OathNearest, BossMech) read a short list instead of building a fresh
 * TActorIterator array (object-hash lookup + world filter + heap allocation) every 0.5-0.6 s.
 */
UCLASS()
class AERONDEL_API UAerondelMobRegistry : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	void RegisterMob(AAerondelMob* Mob);
	void UnregisterMob(AAerondelMob* Mob);
	/** Copies the live mobs into Out (registration order). A copy, so callers may kill or spawn mobs while walking it. */
	void SnapshotMobs(FAerondelMobList& Out) const;
	int32 NumMobs() const { return Mobs.Num(); }
	static UAerondelMobRegistry* GetFor(const UWorld* World);
private:
	TArray<TWeakObjectPtr<AAerondelMob>> Mobs;
};

namespace AerondelMobs
{
	/** Live mobs of World: the registry once play has begun, otherwise (editor tools, pre-BeginPlay) a TActorIterator walk. */
	AERONDEL_API void Live(UWorld* World, FAerondelMobList& Out);
}
