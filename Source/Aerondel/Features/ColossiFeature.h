#pragma once
#include "CoreMinimal.h"
#include "Features/AerondelFeature.h"
#include "ColossiFeature.generated.h"

class AAerondelMob;

USTRUCT(BlueprintType)
struct FColossusInfo
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName Id;
	UPROPERTY(BlueprintReadOnly) FString Title;
	UPROPERTY(BlueprintReadOnly) int32 Kills = 0;
	UPROPERTY(BlueprintReadOnly) bool bAwake = false;
	UPROPERTY(BlueprintReadOnly) bool bHornBroken = false;
	UPROPERTY(BlueprintReadOnly) bool bLegsBroken = false;
	/** True while a charge is wound up (UI warning). */
	UPROPERTY(BlueprintReadOnly) bool bCharging = false;
};

/** Colossi: three huge creatures roaming the Contested Wilds (Stonehorn north, Frostmaw east, Ashwyrm south). One awake per area,
 *  woken within 110 m, parked beyond 160 m, 10 min respawn after a kill. Breakable parts: the horn (12% max HP, stops charges)
 *  and the legs (15%, slower attacks). Hits from behind (> 120 degrees off its facing) add +20% damage and wear the legs;
 *  front hits wear the horn; Stillwatch aimed at head / legs doubles the part damage. Charges every 14 ticks (2-tick warning,
 *  7 m, 22% max HP) unless you are behind it or dodge-rolling. Chest: 4000 coins, 25 arena tokens, 60 Watch. */
UCLASS()
class AERONDEL_API UColossiFeature : public UAerondelFeature
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual FString GetSaveKey() const override { return TEXT("colossi"); }
	virtual void SaveState(FJsonObject& Out) const override;
	virtual void LoadState(const FJsonObject& In) override;
	virtual void ResetState() override { Kills.Reset(); Parts = 0; }
	virtual void OnGameTick() override;
	virtual float DamageDealtBonus(const UMobDef* Target) const override;

	UFUNCTION(BlueprintPure, Category="Aerondel|Colossi") TArray<FColossusInfo> List() const;
	UFUNCTION(BlueprintPure, Category="Aerondel|Colossi") int32 GetPartsBroken() const { return Parts; }
	/** Behind test in web space (exposed for UI hints). */
	static bool IsBehind(FVector2D MobPos, FVector2D MobForward, FVector2D Attacker);
private:
	struct FLive { TWeakObjectPtr<AAerondelMob> Mob; int32 LastHp = 0; int32 Horn = 0, Legs = 0; int32 ChargeAt = 0; int32 Leg = 0; };
	UFUNCTION() void HandleMobKilled(UMobDef* Mob);
	bool Behind(const AAerondelMob* M) const;
	void Wake(int32 Idx);
	void Park(int32 Idx);
	void DamagePart(int32 Idx, bool bHorn, int32 D);
	TMap<FString, int32> Kills;
	int32 Parts = 0, Tick = 0;
	FLive Live[3];
	int64 NextAt[3] = { 0, 0, 0 };
};
