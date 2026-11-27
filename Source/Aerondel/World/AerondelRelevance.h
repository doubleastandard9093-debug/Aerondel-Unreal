#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "World/AerondelScalability.h"
#include "AerondelRelevance.generated.h"

/**
 * Distance relevance for world actors (Windows perf plan). World Partition already streams 128 m cells; inside the
 * loaded cells this keeps only nearby monsters / NPCs / resource nodes at full cost:
 *   Near: full tick + full animation.   Mid: slow actor tick, animation ticked at a low rate (URO).
 *   Far:  hidden, actor + animation tick off (collision kept, so nothing can be walked through when it wakes).
 * Engaged mobs (chasing / attacking / returning), the player's combat target and pending interaction are always Near.
 * Distances are per graphics tier: Low caps hardest, High/Ultra keep more on screen. Values are cm.
 */
namespace AerondelRelevance
{
	enum class EBand : uint8 { Near, Mid, Far };
	struct FBands { float Near, Mid; }; // beyond Mid = Far
	// Keep these in step with Tools/test_relevance.py (it parses this table).
	inline FBands ForTier(EAerondelTier T)
	{
		switch (T)
		{
		case EAerondelTier::Quest: return { 3000.f, 6000.f };
		case EAerondelTier::Low:   return { 4000.f, 9000.f };
		case EAerondelTier::High:  return { 7000.f, 18000.f };
		default:                   return { 9000.f, 26000.f }; // Ultra
		}
	}
	inline EBand Classify(float DistCm, const FBands& B, bool bPinned)
	{
		if (bPinned || DistCm <= B.Near) return EBand::Near;
		return DistCm <= B.Mid ? EBand::Mid : EBand::Far;
	}
}

UCLASS()
class AERONDEL_API UAerondelRelevanceSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UAerondelRelevanceSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return !IsTemplate(); }
private:
	void Apply(AActor* A, AerondelRelevance::EBand Band);
	float Accum = 0.f;
	TMap<TWeakObjectPtr<AActor>, uint8> Bands;      // last band applied per actor
	TMap<TWeakObjectPtr<AActor>, float> BaseTick;   // original actor tick interval
	int32 LastLog[3] = { -1, -1, -1 };
};
