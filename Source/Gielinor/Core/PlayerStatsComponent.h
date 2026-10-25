#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/GielinorTypes.h"
#include "PlayerStatsComponent.generated.h"

class UItemDef; class UPrayerDef; class UAbilityDef; class AGielinorPlayerState;

/** A timed potion / food buff. Boosts don't stack across effects: the biggest one counts. */
USTRUCT(BlueprintType)
struct FActiveEffect
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, SaveGame) FName Id;
	UPROPERTY(BlueprintReadOnly, SaveGame) int32 TicksLeft = 0;
	UPROPERTY(BlueprintReadOnly, SaveGame) TMap<FName, int32> Boost;
	UPROPERTY(BlueprintReadOnly, SaveGame) TMap<FName, float> Fx;
};

/** Poison on the player or a mob: Damage every 10 ticks, dropping by 2 each hit. */
USTRUCT(BlueprintType)
struct FPoisonState
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 Damage = 0;
	UPROPERTY(BlueprintReadOnly) int32 Next = 0;
	bool IsActive() const { return Damage > 0; }
	/** Returns damage to deal this tick (0 most ticks). */
	int32 Tick(float Resist = 0.f);
	bool Apply(int32 Dmg);
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStatsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerDied);

/**
 * Per-player runtime state the web build keeps on `player`: prayer points and active prayers, potion effects,
 * poison, run energy, adrenaline and ability cooldowns, the toolbelt, ammo, attack style, agility lap, stun, regen.
 * Runs the 0.6 s game tick (regen, prayer drain, effect timers, poison). Lives on the PlayerState so it survives pawn swaps.
 */
UCLASS(ClassGroup=(Gielinor), meta=(BlueprintSpawnableComponent))
class GIELINOR_API UPlayerStatsComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UPlayerStatsComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	UPROPERTY(BlueprintAssignable) FOnStatsChanged OnChanged;
	UPROPERTY(BlueprintAssignable) FOnPlayerDied OnDied;

	// ---- State (saved) ----
	UPROPERTY(BlueprintReadOnly, Category="Stats") float PrayerPoints = 10.f;
	UPROPERTY(BlueprintReadOnly, Category="Stats") TArray<FName> ActivePrayers;
	UPROPERTY(BlueprintReadOnly, Category="Stats") TArray<FActiveEffect> Effects;
	UPROPERTY(BlueprintReadOnly, Category="Stats") FPoisonState Poison;
	UPROPERTY(BlueprintReadOnly, Category="Stats") int32 PoisonImmuneTicks = 0;
	UPROPERTY(BlueprintReadOnly, Category="Stats") float RunEnergy = 100.f;
	UPROPERTY(BlueprintReadWrite, Category="Stats") bool bRunning = false;
	UPROPERTY(BlueprintReadOnly, Category="Stats") float Adrenaline = 0.f;
	/** Owned tools (the toolbelt). The best one per type that your level allows is used. */
	UPROPERTY(BlueprintReadOnly, Category="Stats") TArray<TObjectPtr<UItemDef>> Toolbelt;
	UPROPERTY(BlueprintReadOnly, Category="Stats") int32 AmmoCount = 0;
	/** Index into MeleeStyles or RangedStyles depending on the weapon. */
	UPROPERTY(BlueprintReadWrite, Category="Stats") int32 AttackStyle = 0;
	/** Spell id to autocast, or None. */
	UPROPERTY(BlueprintReadWrite, Category="Stats") FName AutocastSpell;
	UPROPERTY(BlueprintReadWrite, Category="Stats") bool bAutoRetaliate = true;
	UPROPERTY(BlueprintReadOnly, Category="Stats") FName AgilityCourse;
	UPROPERTY(BlueprintReadOnly, Category="Stats") int32 AgilityNext = 0;
	/** Active bounty id and kills so far. */
	UPROPERTY(BlueprintReadOnly, Category="Stats") FName BountyId;
	UPROPERTY(BlueprintReadOnly, Category="Stats") int32 BountyKills = 0;

	// ---- Transient ----
	UPROPERTY(BlueprintReadOnly, Category="Stats") int64 GameTick = 0;
	UPROPERTY(BlueprintReadOnly, Category="Stats") int64 StunUntil = 0;
	UPROPERTY(BlueprintReadOnly, Category="Stats") int64 FortifyUntil = 0;
	UPROPERTY(BlueprintReadOnly, Category="Stats") int64 RageUntil = 0;
	UPROPERTY(BlueprintReadOnly, Category="Stats") FName QueuedAbility;
	int64 QueuedAt = 0;
	TMap<FName, int64> CooldownReady;

	UFUNCTION(BlueprintPure, Category="Stats") bool IsStunned() const { return GameTick < StunUntil; }
	UFUNCTION(BlueprintPure, Category="Stats") bool IsFortified() const { return GameTick < FortifyUntil; }
	UFUNCTION(BlueprintPure, Category="Stats") bool IsRaging() const { return GameTick < RageUntil; }
	UFUNCTION(BlueprintPure, Category="Stats") bool IsPrayerActive(FName PrayerId) const { return ActivePrayers.Contains(PrayerId); }
	UFUNCTION(BlueprintPure, Category="Stats") int32 BoostOf(FName SkillId) const;
	UFUNCTION(BlueprintPure, Category="Stats") float EffectFx(FName Key) const;

	// ---- Actions ----
	/** Turns a prayer on/off (group exclusivity + ranged/melee clash). Returns an error or empty. */
	UFUNCTION(BlueprintCallable, Category="Stats") FText TogglePrayer(FName PrayerId);
	UFUNCTION(BlueprintCallable, Category="Stats") int32 RechargePrayer();
	/** Eat food or drink a potion from a backpack slot. Returns true if something was consumed. */
	UFUNCTION(BlueprintCallable, Category="Stats") bool Consume(int32 InventorySlot);
	UFUNCTION(BlueprintCallable, Category="Stats") bool BuryBones(int32 InventorySlot);
	/** Clean a grimy herb (Herblore level check). */
	UFUNCTION(BlueprintCallable, Category="Stats") bool CleanHerb(int32 InventorySlot);
	/** Moves a tool into the toolbelt (from a backpack slot). */
	UFUNCTION(BlueprintCallable, Category="Stats") bool AddTool(UItemDef* Tool);
	UFUNCTION(BlueprintCallable, Category="Stats") bool ApplyPoison(int32 Damage);
	/** Abilities: validates, spends/gains adrenaline, applies self buffs or queues a strike for the next attack. */
	UFUNCTION(BlueprintCallable, Category="Stats") FText UseAbility(FName AbilityId, bool bHasTarget);
	UFUNCTION(BlueprintPure, Category="Stats") float CooldownSecondsLeft(FName AbilityId) const;
	/** Takes the queued special (if any) and starts its cooldown. */
	FName ConsumeQueuedAbility();
	void GainAdrenaline(float Amount) { Adrenaline = FMath::Clamp(Adrenaline + Amount, 0.f, 100.f); }
	/** Fire one arrow / bolt. Thrown weapons are free. Returns false (with message) when out of ammo. */
	bool UseAmmo(FText& OutError);
	/** Applies damage to the player's health (death -> keep items, respawn in town). */
	void ApplyPlayerDamage(int32 Amount, AActor* Source);
	/** Called when HP hits 0: keep everything, full heal, clear prayers/buffs, teleport to the respawn point. */
	void HandleDeath();
	void RefreshMaxHp(bool bRefill = false);
	/** Starts a bounty from the database. */
	UFUNCTION(BlueprintCallable, Category="Stats") bool StartBounty(FName Id);
	void OnMobKilledForBounty(const class UMobDef* Mob);

	/** Bank / save helpers. */
	void ResetRuntime();

private:
	void GameTickFn();
	AGielinorPlayerState* PS() const;
	void Msg(const FText& T) const;
	FTimerHandle TickTimer;
	float RegenAcc = 0.f;
};
