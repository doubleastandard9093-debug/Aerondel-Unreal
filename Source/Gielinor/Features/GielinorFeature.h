#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "GielinorFeature.generated.h"

class AGielinorPlayerState;
class UMobDef;

/**
 * Base for the systems ported from Gielinor-Lite (phases 1-9, mechanics pass, Hearth & Arena, WoW/Outer Worlds/TERA round).
 * Each feature is a GameInstance subsystem that owns one save block (JSON, keyed by GetSaveKey(), same shape as the web save)
 * and can add an XP bonus. Logic and data only: UI is UMG on top of the BlueprintCallable API.
 * Abstract, so the subsystem collection never creates the base itself.
 */
UCLASS(Abstract)
class GIELINOR_API UGielinorFeature : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Save block name (matches the web save field, e.g. "companions"). */
	virtual FString GetSaveKey() const { return GetClass()->GetName(); }
	virtual void SaveState(FJsonObject& Out) const {}
	virtual void LoadState(const FJsonObject& In) {}
	/** Back to a fresh-character state (called before LoadState, and for new games). */
	virtual void ResetState() {}
	/** Extra XP fraction for a skill id (0.05 = +5%). */
	virtual float XpBonus(FName SkillId) const { return 0.f; }
	/** Multiplier on damage the player takes (1 = none). HpFrac is the player's hitpoints fraction before the hit. */
	virtual float DamageTakenMult(const UMobDef* Source, float HpFrac) const { return 1.f; }
	/** Additive bonus on player damage dealt (0.05 = +5%). */
	virtual float DamageDealtBonus(const UMobDef* Target) const { return 0.f; }
	/** Called when the player dies. */
	virtual void OnPlayerDied() {}
	/** Called every game tick (0.6 s) by UFeatureTicker. */
	virtual void OnGameTick() {}

	/** All live features of a game instance. */
	static TArray<UGielinorFeature*> AllFor(const UGameInstance* GI);
	static float TotalXpBonus(const UObject* WorldContext, FName SkillId);
	static float TotalDamageTakenMult(const UObject* WorldContext, const UMobDef* Source, float HpFrac);
	static float TotalDamageDealtBonus(const UObject* WorldContext, const UMobDef* Target);
	static void NotifyPlayerDied(const UObject* WorldContext);
	/** Unix time in ms (the web build uses Date.now()). */
	static int64 NowMs();
	/** Web display name -> content id ("Iron bar" -> iron_bar). */
	static FName Id(const FString& WebName);

protected:
	AGielinorPlayerState* GetPS() const;
	void Say(const FString& Msg) const;
	int32 SkillLevel(const FString& Skill) const;
	int32 CombatLevel() const;
	void GiveXp(const FString& Skill, float Xp) const;
	int32 CountItem(const FString& Item) const;
	bool TakeItem(const FString& Item, int32 N) const;
	/** Inventory first, overflow to the bank. */
	void GiveItem(const FString& Item, int32 N) const;
	/** Straight to the bank (errands, harvests). */
	void GiveToBank(const FString& Item, int32 N) const;
	bool TakeCoins(int32 N) const { return TakeItem(TEXT("Coins"), N); }
	void GiveCoins(int32 N) const { GiveItem(TEXT("Coins"), N); }
	template<class T> T* Feature() const { UGameInstance* GI = GetGameInstance(); return GI ? GI->GetSubsystem<T>() : nullptr; }

	// JSON helpers (missing keys keep defaults, like the web normalize* functions).
	static int64 JInt(const FJsonObject& O, const TCHAR* Key, int64 Def = 0);
	static double JNum(const FJsonObject& O, const TCHAR* Key, double Def = 0.0);
	static bool JBool(const FJsonObject& O, const TCHAR* Key);
	static FString JStr(const FJsonObject& O, const TCHAR* Key);
	static TArray<FString> JStrArr(const FJsonObject& O, const TCHAR* Key);
	static TSharedPtr<FJsonObject> JObj(const FJsonObject& O, const TCHAR* Key);
	static void PutStrArr(FJsonObject& O, const TCHAR* Key, const TArray<FString>& A);
	static void PutIntMap(FJsonObject& O, const TCHAR* Key, const TMap<FString, int32>& M);
	static TMap<FString, int32> JIntMap(const FJsonObject& O, const TCHAR* Key);
};
