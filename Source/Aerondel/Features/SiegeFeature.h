#pragma once
#include "CoreMinimal.h"
#include "Features/AerondelFeature.h"
#include "SiegeFeature.generated.h"

class AAerondelMob;

/** A pending raid mechanic for the UI to draw (red fire ring on the player, blue stack ring at the rally point). */
USTRUCT(BlueprintType)
struct FSiegeMech
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName Kind;      // fire | stack
	UPROPERTY(BlueprintReadOnly) FVector Location = FVector::ZeroVector; // world cm
	UPROPERTY(BlueprintReadOnly) float RadiusCm = 0.f;
	UPROPERTY(BlueprintReadOnly) int32 TicksLeft = 0;
};

/** Warband Siege: a 4-boss raid on the Ashen Warband's fort with you, your active companion and three hired sellswords.
 *  Offline only. Caps: 8 friendly, 10 hostile units. Mechanics every 8 ticks with a 3-tick warning: fire under you (move),
 *  a crushing blow split across the party in the blue ring (stack), and Ashen raider adds. Downed bosses stay down for the week. */
UCLASS()
class AERONDEL_API USiegeFeature : public UAerondelFeature
{
	GENERATED_BODY()
public:
	static constexpr int32 FriendCap = 8, HostileCap = 10, BossCoins = 1500;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual FString GetSaveKey() const override { return TEXT("siege"); }
	virtual void SaveState(FJsonObject& Out) const override;
	virtual void LoadState(const FJsonObject& In) override;
	virtual void ResetState() override { Week = 0; Downed.Reset(); Clears = 0; }
	virtual void OnGameTick() override;
	virtual void OnPlayerDied() override { if (bRunning) End(TEXT("You fall in the siege. Downed bosses stay down for the week.")); }

	UFUNCTION(BlueprintCallable, Category="Aerondel|Siege") bool Enter();
	UFUNCTION(BlueprintCallable, Category="Aerondel|Siege") void Leave() { if (bRunning) End(TEXT("You fall back from the siege.")); }
	UFUNCTION(BlueprintPure, Category="Aerondel|Siege") bool IsRunning() const { return bRunning; }
	UFUNCTION(BlueprintPure, Category="Aerondel|Siege") int32 DownedThisWeek() const;
	UFUNCTION(BlueprintPure, Category="Aerondel|Siege") int32 GetClears() const { return Clears; }
	UFUNCTION(BlueprintPure, Category="Aerondel|Siege") TArray<FString> PartyNames() const { return Party; }
	UFUNCTION(BlueprintPure, Category="Aerondel|Siege") bool GetPendingMech(FSiegeMech& Out) const;
	/** Warcaptain Hesk's lines. */
	UFUNCTION(BlueprintPure, Category="Aerondel|Siege") FString Greeting() const;
	UFUNCTION(BlueprintPure, Category="Aerondel|Siege") static FString HowItWorks();
private:
	UFUNCTION() void HandleMobKilled(UMobDef* Mob);
	void Lock() const;
	void SpawnBoss();
	void StartMech(FName Kind);
	void ResolveMech();
	void End(const FString& Msg);
	int32 Alive() const;

	mutable int64 Week = 0;
	mutable TArray<FString> Downed;
	int32 Clears = 0;
	// run (not saved: leaving ends the attempt)
	bool bRunning = false;
	int32 BossIdx = 0, MechAt = 0, MechN = 0, Tick = 0;
	bool bAddsDone = false, bPending = false;
	FName PendingKind; FVector2D PendingAt = FVector2D::ZeroVector; int32 PendingTick = 0;
	TArray<FString> Party;
	TWeakObjectPtr<AAerondelMob> Boss;
	TArray<TWeakObjectPtr<AAerondelMob>> Hostiles;
	FVector2D Return = FVector2D::ZeroVector;
};
