#pragma once
#include "CoreMinimal.h"
#include "Features/AerondelFeature.h"
#include "QuirksFeature.generated.h"

class UMobDef;

/** Scars and Quirks: counters notice how you play (fire hits, fleeing, deaths, melee kills) and offer a quirk:
 *  a downside paired with a boon. Accept or decline; hold at most 3; Ysolde's remedy (Herbalism 30) removes one. */
UCLASS()
class AERONDEL_API UQuirksFeature : public UAerondelFeature
{
	GENERATED_BODY()
public:
	static constexpr int32 MaxHeld = 3;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual FString GetSaveKey() const override { return TEXT("quirks"); }
	virtual void SaveState(FJsonObject& Out) const override;
	virtual void LoadState(const FJsonObject& In) override;
	virtual void ResetState() override { Held.Reset(); Declined.Reset(); Counters.Reset(); Offered.Reset(); Removed = 0; }
	virtual float XpBonus(FName SkillId) const override;
	virtual float DamageTakenMult(const UMobDef* Source, float HpFrac) const override;
	virtual float DamageDealtBonus(const UMobDef* Target) const override;
	virtual void OnPlayerDied() override { Bump(TEXT("deaths")); }

	/** fire | flee | deaths | meleeKills */
	UFUNCTION(BlueprintCallable, Category="Aerondel|Quirks") void Bump(const FString& Counter, int32 N = 1);
	UFUNCTION(BlueprintPure, Category="Aerondel|Quirks") FString GetOffered() const { return Offered; }
	UFUNCTION(BlueprintCallable, Category="Aerondel|Quirks") bool Accept();
	UFUNCTION(BlueprintCallable, Category="Aerondel|Quirks") void Decline();
	/** Ysolde's remedy: Herbalism 30, 2000 coins, 5 grimy greenwort. */
	UFUNCTION(BlueprintCallable, Category="Aerondel|Quirks") FString Remedy(const FString& QuirkId);
	UFUNCTION(BlueprintPure, Category="Aerondel|Quirks") bool Has(const FString& QuirkId) const { return Held.Contains(QuirkId); }
	UFUNCTION(BlueprintPure, Category="Aerondel|Quirks") TArray<FString> GetHeld() const { return Held; }
	UFUNCTION(BlueprintPure, Category="Aerondel|Quirks") FString Describe(const FString& QuirkId) const;
	/** Skittish: fraction of run-energy drain refunded. */
	UFUNCTION(BlueprintPure, Category="Aerondel|Quirks") float RunRefund() const { return Has(TEXT("skittish")) ? 0.35f : 0.f; }
	/** Knuckle-head: ranged accuracy change. */
	UFUNCTION(BlueprintPure, Category="Aerondel|Quirks") float RangedAccuracy() const { return Has(TEXT("brawler")) ? -0.1f : 0.f; }
private:
	UFUNCTION() void HandleMobKilled(UMobDef* Mob);
	void CheckDue();
	TArray<FString> Held, Declined;
	mutable TMap<FString, int32> Counters;
	FString Offered;
	int32 Removed = 0;
};
