#pragma once
#include "CoreMinimal.h"
#include "Features/AerondelFeature.h"
#include "StandingFeature.generated.h"

USTRUCT(BlueprintType)
struct FFactionDef
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName Id;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) TArray<FString> Towns;
	UPROPERTY(BlueprintReadOnly) TArray<FString> Skills;
};

/** Standing: reputation with five factions. Tiers Unknown..Exalted give +1% XP per tier in the faction's skills;
 *  Quartermaster Odile (Whitecairn) pays a one-time reward per tier reached. Quest rep (+40 per finished town quest) is derived. */
UCLASS()
class AERONDEL_API UStandingFeature : public UAerondelFeature
{
	GENERATED_BODY()
public:
	static constexpr int32 QuestRep = 40;
	static constexpr int32 BountyRep = 25;
	virtual FString GetSaveKey() const override { return TEXT("rep"); }
	virtual void SaveState(FJsonObject& Out) const override;
	virtual void LoadState(const FJsonObject& In) override;
	virtual void ResetState() override { Earned.Reset(); ClaimedTier = 0; }
	virtual float XpBonus(FName SkillId) const override;

	static const TArray<FFactionDef>& Factions();
	static FString TierName(int32 Tier);
	static int32 TierOf(int32 Rep);
	/** Rep needed for the next tier, or -1 at Exalted. */
	static int32 NextTier(int32 Rep);

	UFUNCTION(BlueprintPure, Category="Aerondel|Standing") int32 RepOf(FName Faction) const;
	UFUNCTION(BlueprintPure, Category="Aerondel|Standing") int32 TierFor(FName Faction) const { return TierOf(RepOf(Faction)); }
	/** Highest tier reached with any faction (Skywing needs Exalted = 4). */
	UFUNCTION(BlueprintPure, Category="Aerondel|Standing") int32 BestTier() const;
	/** Returns true if the tier went up. */
	UFUNCTION(BlueprintCallable, Category="Aerondel|Standing") bool AddRep(FName Faction, int32 N);
	/** Quartermaster: claim every unclaimed tier reward (by best tier). Returns how many were paid. */
	UFUNCTION(BlueprintCallable, Category="Aerondel|Standing") int32 ClaimTierRewards();
	/** Faction for a town name ("Saltreach" -> mariners), or None. */
	UFUNCTION(BlueprintPure, Category="Aerondel|Standing") FName FactionForTown(const FString& Town) const;

private:
	int32 QuestRepFor(const FFactionDef& F) const;
	TMap<FName, int32> Earned;
	int32 ClaimedTier = 0;
};
