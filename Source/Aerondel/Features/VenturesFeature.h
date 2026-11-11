#pragma once
#include "CoreMinimal.h"
#include "Features/AerondelFeature.h"
#include "VenturesFeature.generated.h"

class UMobDef;
class AAerondelMob;
class UItemDef;
struct FGPanelModel;

/** One Overflare ultimate per weapon family (web adapt/overflare.js ULTS). */
struct FVentureUlt { const TCHAR* Key; const TCHAR* Name; int32 Hits; float Bonus; float Radius; };

/** A saved Kitbash Wardrobe outfit: slot -> look item, slot -> dye name. */
struct FVentureOutfit { bool bUsed = false; TMap<FString, FString> Looks; TMap<FString, FString> Dyes; };

/** Hidden Reliquary cache site (web metres). */
struct FVentureRelic { int32 Index = 0; FVector2D Pos = FVector2D::ZeroVector; int32 Tier = 1; };

/**
 * Ventures (web v23, js/adapt/*): twenty adaptation systems on one save block ("adapt", same shape as player.adapt on the web).
 * One subsystem so the systems can read each other (Overflare gain from sigils, stagger from the lattice, and so on).
 * Core lives in VenturesFeature.cpp; each group of systems has its own Ventures_*.cpp.
 * Combat bonus totals are capped like the web: damage +150%, accuracy +60%, damage reduction 60%, XP +100%.
 * UI: the "ventures" panel (J) with one tab per system (UI/AerondelPanels.cpp routes ventures* panels here).
 */
UCLASS()
class AERONDEL_API UVenturesFeature : public UAerondelFeature
{
	GENERATED_BODY()
public:
	static constexpr float CapDmg = 1.5f;
	static constexpr float CapAcc = 0.6f;
	static constexpr float CapDr = 0.6f;
	static constexpr float CapXp = 1.f;
	static constexpr int32 FlareBuffTicks = 10;
	static constexpr int32 RodPrice = 150;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual FString GetSaveKey() const override { return TEXT("adapt"); }
	virtual void SaveState(FJsonObject& Out) const override;
	virtual void LoadState(const FJsonObject& In) override;
	virtual void ResetState() override;
	virtual float XpBonus(FName SkillId) const override;
	virtual float DamageTakenMult(const UMobDef* Source, float HpFrac) const override;
	virtual float DamageDealtBonus(const UMobDef* Target) const override;
	virtual float AccuracyBonus(const UMobDef* Target) const override;
	virtual void OnPlayerDied() override;
	virtual void OnGameTick() override;

	static UVenturesFeature* Get(const UObject* WorldContext);
	/** AAerondelMob::TakePlayerDamage: every player hit passes through here first. Returns the damage to apply. */
	static int32 HookPlayerHit(AAerondelMob* Mob, int32 Damage, AActor* Attacker);
	/** AAerondelMob::HandleDied: the mob actor that died (OnMobKilled only carries the definition). */
	static void HookMobDied(AAerondelMob* Mob, AActor* Killer);
	/** AAerondelMob::HandleDied: respawn delay multiplier (Burst Tides surge areas respawn 3x faster). */
	static float HookRespawnScale(const AAerondelMob* Mob);

	/** Panel content for "ventures" / "ventures_<tab>" and the Ventures NPC services. False when the panel is not ours. */
	bool BuildPanel(const FString& Panel, FGPanelModel& M);
	static bool IsVenturePanel(const FString& Panel);
	/** Keyboard: overflare (U), tether (L), super (N), dash (C). J (hub) is handled by the character/HUD. */
	void OnKey(FName Key);

	// ---- shared helpers (web A.*) ----
	FString Style() const;                      // Melee | Ranged | Magic | Swordsmanship
	FName WeaponClass() const;                  // katana, scimitar, ... or none
	const UItemDef* Weapon() const;
	bool IsMagic() const;
	bool IsRangedNow() const;
	static bool IsBossy(const AAerondelMob* Mob);
	TArray<AAerondelMob*> MobsNear(FVector2D WebPos, float RadiusM, int32 Cap = 6) const;
	/** A bonus player hit (Overflare strikes, Tether pull, Breach blasts). Returns damage dealt. */
	int32 Strike(AAerondelMob* Mob, float Acc, float Dmg, const TCHAR* Kind);
	bool Owns(const FString& Item) const;
	int32 Coins() const { return CountItem(TEXT("Coins")); }
	int32 TickNow() const { return TickN; }
	/** Like the web A.cap: 8 foes at Low scalability, 15 otherwise. */
	int32 CapFoes(int32 N) const;
	/** Venture level from combat skills (web A.vLevel). */
	int32 VLevel() const;
	AAerondelMob* CurrentTarget() const;

	// ---- Overflare ----
	void FlareAdd(float N) const;
	bool Flare();
	const FVentureUlt& Ult() const;
	float FlareMeter() const { return Meter; }

	// ---- Kitbash Wardrobe ----
	/** Body dye colour for the rig tint (single mesh, so only the body slot shows). */
	static bool BodyDye(const UObject* WorldContext, FLinearColor& Out);

	// ---- Wayfarer's Ledger ----
	int32 LedgerCount() const { return Souls.Num() + Rares.Num() + Lore.Num() + Trinkets.Num(); }
	int32 LedgerTier() const { return LedgerCount() / 5; }
	bool LedgerAdd(int32 Kind, const FString& Entry, const FString& Label);

	// ---- Hidden Reliquaries ----
	void RelicDig();
	void RelicPress(int32 Rune);

	// ---- group 2: Crestbreak, Engrave Sigils, Riven Grafts, Twin Stance (Ventures_Combat.cpp) ----
	/** Adds stagger to a boss; at 100 a part breaks (stun 3 ticks, +40% damage / +70% with a katana for 8 ticks). */
	void Stagger(AAerondelMob* Mob, float N);
	bool IsBroken(const AAerondelMob* Mob) const;
	float StaggerOf(const AAerondelMob* Mob) const;
	int32 SigTier(const FString& Key) const;
	/** Active sigil value (0 when not engraved). */
	float Sig(const FString& Key) const;
	void Study(const FString& Key);
	void SigToggle(const FString& Key);
	float GraftSum(const FString& Stat) const;
	bool GraftOn(const FString& Item, const FString& Graft);
	void GraftOff(const FString& Item, int32 Index);
	void GraftFuse(const FString& Graft);
	bool NearBank() const;
	void SetStance(const FString& Slot, const FString& Style);
	bool StanceOn() const;
	/** UAerondelRules::Perk: Twin Stance multiplies a skill's perks (+20% main, +10% support). */
	static float PerkBoost(const UObject* WorldContext, FName SkillId);
	/** A bonus drop from Ventures loot (Sigil tome or a graft). */
	void BonusDrop(const AAerondelMob* Mob, const FString& Why);

	// ---- group 3: Burst Tides, Breach Runs, Tetherline + shared Ventures zones (Ventures_Zone.cpp) ----
	static constexpr float ZoneHalf = 14.f;
	static FString StyleSkill(const FString& Style);
	/** Pad centre (web metres): x = -1000, z = 135 vigil / 205 breach / 270 hold. */
	static FVector2D ZoneC(const FString& ZoneId);
	AAerondelMob* ZoneSpawn(const FString& ZoneId, const FString& Type, float Dx, float Dz, float L, float HpMul = 1.f, bool bBoss = false);
	TArray<AAerondelMob*> ZoneAlive() const;
	void ZonePark();
	bool OnPad(const FString& ZoneId) const;
	bool RunStart(const FString& ZoneId, const FString& Kind);
	void RunEnd(bool bWin);
	bool InRun() const { return Run.bOn; }
	bool InRunKind(const TCHAR* Kind) const { return Run.bOn && Run.Kind == Kind; }
	void BreachStart(const FString& Mode, bool bHard);
	void TideStart(int32 RegionIndex = -1);
	void TideWave();
	bool InTide() const;
	bool Tether();

	// ---- group 4: Sealed Geodes, Ascent Rolls, Lumen Rise, Anima Lattice (Ventures_Gear.cpp) ----
	bool GeodeOpen(const FString& Geode);
	void RelicForge(const FString& Geode);
	static bool AscentOk(const UItemDef* I);
	void AscentScan();
	void AscentReroll(const FString& Weapon);
	void AscentDismantle(const FString& Weapon);
	void AscentSet(const FString& Weapon, int32 Slot, const FString& Perk);
	bool LumenOn() const;
	void LumenGain(float N);
	/** Weekly (max 3) Pinnacle chest from a Vigil clear or a Tide boss. */
	bool Pinnacle(const FString& Source);
	bool LatticeOpen(const FString& StyleName) const;
	float LatticeSum(const FString& Type, const FString& Board) const;
	FString LatticeBoard() const;
	void LatticeFill(const FString& StyleName, int32 Node);
	void LatticeReset(const FString& StyleName);

	// ---- group 5: Vigil Gates, Elemental Attunements, Hearthhold (Ventures_Raid.cpp) ----
	bool VigilStart(const FString& Gate, bool bHard);
	bool SuperUse();
	void AttuneSet(const FString& Element);
	void FragToggle(const FString& Frag);
	bool HoldHome() const;
	void HoldGo();
	void HoldLeave();
	void HoldBuy();
	void HoldUp(const FString& Building);
	void HoldForge();
	void HoldGarden();
	void HoldSend(FName Companion, const FString& Trip);
	void HoldCollect(FName Companion);

	// ---- group 6: Season Chronicle, Glidewalk (Ventures_Season.cpp) ----
	void ChronGain(float N);
	void ChronStep(int32 I, int32 N = 1);
	/** Dash forward (C); facing a 3 m+ drop it becomes a long glide. */
	bool Dash();
	float GlideDrop() const;

	// ---- group 7: Huntwright (Ventures_Huntwright.cpp) — introduces Hunter ----
	bool HuntCarve(AAerondelMob* Mob = nullptr);
	bool HuntTrack(const FString& Boss);
	bool HuntCraft(const FString& Boss, const FString& Kind);
	FString HuntWornSet() const;

	// ---- group 8: Guardbreak (Ventures_Guardbreak.cpp) ----
	bool GuardRiposte(AAerondelMob* Mob = nullptr);
	int32 GuardPoiseOf(const AAerondelMob* Mob) const;
	int32 GuardPoiseMax(const AAerondelMob* Mob) const;
	bool GuardRiposteOpen(const AAerondelMob* Mob) const;
	int32 PlayerPoiseMax() const;
	/** Called from UPlayerStatsComponent after a landed incoming hit. */
	static void HookPlayerHurt(const UObject* WorldContext, int32 Amount);

protected:
	UFUNCTION() void HandleMobKilled(UMobDef* Mob);

	// extension points: each group adds its part (see the group .cpp files)
	float SumDmg(const UMobDef* Target) const;
	float SumAcc(const UMobDef* Target) const;
	float SumDr(const UMobDef* Source) const;
	float SumXp(FName SkillId) const;
	float FlareGainBonus() const;
	void DiedMore(AAerondelMob* Mob, bool bByPlayer);
	bool BuildTabMore(const FString& Tab, FGPanelModel& M);
	void OverviewMore(FGPanelModel& M);

	// panel helpers
	void Tabs(FGPanelModel& M, const FString& Cur);
	void GoTab(const FString& Tab) const;
	void TabOverview(FGPanelModel& M);
	void TabFlare(FGPanelModel& M);
	void TabWardrobe(FGPanelModel& M);
	void TabLedger(FGPanelModel& M);
	void TabReliq(FGPanelModel& M);
	void TabCrest(FGPanelModel& M);
	void TabSigils(FGPanelModel& M);
	void TabGrafts(FGPanelModel& M);
	void TabStance(FGPanelModel& M);
	// group 2 internals
	void ResetCombat();
	void SaveCombat(FJsonObject& A) const;
	void LoadCombat(const FJsonObject& A);
	int32 HitCombat(AAerondelMob* Mob, int32 D);
	void DiedCombat(AAerondelMob* Mob, bool bByPlayer);
	float DmgCombat(const UMobDef* Target) const;
	float AccCombat() const;
	float DrCombat() const;
	float XpCombat(FName SkillId) const;
	float FlareCombat() const;
	// group 3 internals
	void ResetZone();
	void SaveZone(FJsonObject& A) const;
	void LoadZone(const FJsonObject& A);
	void TickZone();
	void DiedZone(AAerondelMob* Mob, bool bByPlayer);
	float RespawnZone(const AAerondelMob* Mob) const;
	float XpZone(FName SkillId) const;
	/** Incoming-damage multiplier from the active run (Frenzy). */
	float TakenZone() const;
	void BreachSetup();
	void BreachStep();
	void BreachFinish(bool bWin);
	void LoadTideRegions();
	void TideEnd();
	void TabTides(FGPanelModel& M);
	void TabBreach(FGPanelModel& M);
	void TabTether(FGPanelModel& M);
	// group 4 internals
	void ResetGear();
	void SaveGear(FJsonObject& A) const;
	void LoadGear(const FJsonObject& A);
	void TickGear();
	void DiedGear(AAerondelMob* Mob, bool bByPlayer, bool bTideBoss);
	void BreachClearGear(const FString& Mode);
	float DmgGear(const UMobDef* Target) const;
	float AccGear() const;
	float DrGear() const;
	float XpGear(FName SkillId) const;
	float FlareGear() const;
	float StagGear() const;
	float AscentCur(const FString& Stat) const;
	float RelicSum(const FString& Stat) const;
	float LumenBonus() const;
	void TabGeodes(FGPanelModel& M);
	void TabAscent(FGPanelModel& M);
	void TabLumen(FGPanelModel& M);
	void TabLattice(FGPanelModel& M);
	// group 5 internals
	void ResetRaid();
	void SaveRaid(FJsonObject& A) const;
	void LoadRaid(const FJsonObject& A);
	void TickRaid();
	void VigilStep();
	void VigilFinish(bool bWin);
	int32 HitRaid(AAerondelMob* Mob, int32 D);
	void DiedRaid(AAerondelMob* Mob, bool bByPlayer);
	float DmgRaid(const UMobDef* Target) const;
	float DmgRaidActor(const AAerondelMob* Mob) const;
	float AccRaid() const;
	float DrRaid() const;
	float XpRaid(FName SkillId) const;
	bool AttuneGearMatch() const;
	bool HasFrag(const TCHAR* Frag) const { return AttuneFrags.Contains(Frag); }
	void TabVigil(FGPanelModel& M);
	void TabAttune(FGPanelModel& M);
	void TabHold(FGPanelModel& M);
	// group 6 internals
	void ResetSeason();
	void SaveSeason(FJsonObject& A) const;
	void LoadSeason(const FJsonObject& A);
	void ChronCheck();
	bool ChronModOn(const TCHAR* Key) const;
	void DiedSeason(AAerondelMob* Mob, bool bByPlayer, bool bTideBoss);
	void ChronBreach();
	float DmgSeasonActor(const AAerondelMob* Mob) const;
	float AccSeason() const;
	float DrSeason() const;
	float XpSeason() const;
	void HitSeason(AAerondelMob* Mob, int32 D);
	void TabChron(FGPanelModel& M);
	// group 7 internals
	void ResetHunt();
	void SaveHunt(FJsonObject& A) const;
	void LoadHunt(const FJsonObject& A);
	void TickHunt();
	int32 HitHunt(AAerondelMob* Mob, int32 D);
	void DiedHunt(AAerondelMob* Mob, bool bByPlayer);
	void TabHunt(FGPanelModel& M);
	// group 8 internals
	void ResetGuard();
	void SaveGuard(FJsonObject& A) const;
	void LoadGuard(const FJsonObject& A);
	void TickGuard();
	int32 HitGuard(AAerondelMob* Mob, int32 D);
	void IncomingGuard(int32 Amount);
	void TabGuard(FGPanelModel& M);

	// group 1 internals
	void ResetQuick();
	void SaveQuick(FJsonObject& A) const;
	void LoadQuick(const FJsonObject& A);
	void TickQuick();
	void KilledQuick(const FString& Name, AAerondelMob* Mob);
	void SweepFlareStrikes();
	const TArray<FVentureRelic>& Relics() const;
	bool RelicNear(FVentureRelic& Out, float& Dist) const;
	void RelicFinish(bool bOk);
	void WardrobeApply(const FVentureOutfit& O);
	static FString TodayUtc();

	int32 TickN = 0;
	/** Unknown save keys from a newer build are kept (web normalizeAdapt). */
	TMap<FString, TSharedPtr<FJsonValue>> Unknown;

	// Overflare
	mutable float Meter = 0.f;
	mutable bool bWasFull = false;
	int32 FlareUses = 0;
	int32 FlareBuffUntil = 0;
	struct FPendingStrike { TArray<TWeakObjectPtr<AAerondelMob>> Foes; int32 Left = 0; float Bonus = 0.f; };
	TArray<FPendingStrike> PendingStrikes;
	const TCHAR* StrikeKind = nullptr;

	// Wardrobe
	TMap<FString, FString> Dyes;
	FVentureOutfit Outfits[3];

	// Ledger
	TArray<FString> Souls, Rares, Lore, Trinkets;

	// Reliquaries
	FString RelicDay;
	TArray<int32> RelicDug;
	int32 RelicTotal = 0;
	struct FRelicPuzzle { bool bOn = false; FVentureRelic Site; TArray<int32> Seq; int32 At = 0; int64 EndMs = 0; };
	FRelicPuzzle Puz;

	// Crestbreak (per live boss) and its save
	struct FCrestState { float Stag = 0.f; int32 Parts = 0; int32 BrokeUntil = 0; };
	TMap<TWeakObjectPtr<AAerondelMob>, FCrestState> Crest;
	int32 CrestBreaks = 0;
	bool bBouncing = false;
	// Sigils
	TMap<FString, int32> SigRead;
	TArray<FString> SigOn;
	// Grafts: worn item name -> graft names
	TMap<FString, TArray<FString>> GraftsOn;
	FString GraftSel;
	// Twin Stance
	FString StanceMain, StanceSub;

	// One Ventures run at a time (breach / vigil / hold share it).
	struct FVentureRun
	{
		bool bOn = false;
		FString Zone, Kind, Mode, Mod;
		bool bHard = false;
		int32 L = 0;
		int32 Set = 0;
		int32 Wave = 0;
		int32 Kills = 0;
		int32 Goal = 0;
		int32 Away = 0;
		int32 Phase = 0;
		float Ward = 100.f;
		int64 StartMs = 0;
		TArray<FVector2D> NodePos;
		TArray<float> NodeP;
		FVector2D Return = FVector2D(0.0, 10.0);
		TArray<TWeakObjectPtr<AAerondelMob>> Foes;
		TWeakObjectPtr<AAerondelMob> Boss;
	};
	FVentureRun Run;
	struct FTideRegion { FString Name; FVector2D C = FVector2D::ZeroVector; float R = 50.f; FString Camp; FVector2D CampOff = FVector2D::ZeroVector; bool bCamp = false; };
	TArray<FTideRegion> TideRegions;
	bool bTideLoaded = false;
	int32 TideRegion = -1;
	int64 TideUntil = 0;
	int64 TideNext = 0;
	int32 TideStacks = 0;
	bool bTideWaved = false;
	TArray<TWeakObjectPtr<AAerondelMob>> TideWaveMobs;
	int32 TidesSeen = 0;
	int32 TidesBest = 0;
	int32 BreachRuns = 0;
	int32 BreachAnima = 0;
	TMap<FString, FString> BreachBest;
	int64 TetherReady = 0;
	// Geodes: geode name -> parts got; relics forged
	TMap<FString, TArray<FString>> GeodeParts;
	TArray<FString> RelicsForged;
	// Ascent: weapon name -> 2 perk keys; dismantle counts; learned patterns
	TMap<FString, TArray<FString>> AscentRolls;
	TMap<FString, int32> AscentDis;
	TArray<FString> AscentPat;
	FString AscentSel;
	// Lumen
	float LumenScore = 1000.f;
	int32 LumenWeek = 0;
	int32 LumenPinn = 0;
	// Lattice: style -> filled node indices
	TMap<FString, TArray<int32>> LatticeBoards;
	int32 LatticeSpent = 0;
	FString LatticeView;
	// Vigil: "gate:tier" -> week looted; clears per gate; live blast
	TMap<FString, int32> VigilLock;
	TMap<FString, int32> VigilClears;
	int32 VigilNext = 0;
	bool bBlast = false;
	FVector2D BlastAt = FVector2D::ZeroVector;
	int32 BlastTick = 0;
	// Attune
	FString AttuneEl;
	TArray<FString> AttuneFrags;
	int32 AttuneSupers = 0;
	int64 SuperReady = 0;
	bool bArcing = false;
	// Hearthhold
	struct FHoldTrip { FString Kind; int64 Back = 0; bool bTold = false; };
	bool bHoldOwned = false;
	TMap<FString, int32> HoldLv;
	TMap<FName, FHoldTrip> HoldTrips;
	FString HoldForgeDay;
	double HoldGardenAt = 0.0;
	int32 HoldDone = 0;
	FVector2D HoldReturn = FVector2D(0.0, 10.0);
	// Season Chronicle
	FString ChronKeyCur;
	TArray<int32> ChronSteps = { 0, 0, 0, 0 };
	int32 ChronLv = 0;
	float ChronXp = 0.f;
	TArray<FString> ChronOn;
	bool bChronDone = false;
	// Glidewalk
	int64 DashReady = 0;

	// Huntwright (web adapt/huntwright.js, save key "hunt")
	TMap<FString, int32> HuntCarved;
	TArray<FString> HuntUnlocked;
	int32 HuntPartsBroken = 0;
	int32 HuntTracks = 0;
	struct FHuntPart { FString Id, Item, Label; int32 Hp = 0; int32 Max = 0; };
	struct FHuntLive { TArray<FHuntPart> Parts; TArray<FString> Broke; int64 CarveUntilMs = 0; bool bCarved = false; };
	TMap<TWeakObjectPtr<AAerondelMob>, FHuntLive> HuntLive;
	struct FHuntCarveJob { TWeakObjectPtr<AAerondelMob> Mob; int64 UntilMs = 0; FVector2D Start = FVector2D::ZeroVector; bool bOn = false; };
	mutable FHuntCarveJob HuntCarveJob;
	struct FHuntTrackJob { FString Boss; TArray<FVector2D> Marks; int32 At = 0; bool bOn = false; };
	FHuntTrackJob HuntTrackJob;

	// Guardbreak (web adapt/guardbreak.js, save key "guard")
	int32 GuardBreaks = 0;
	int32 GuardRipostes = 0;
	struct FGuardPoise { float Poise = 0.f; int32 CdUntil = 0; int32 RiposteUntil = 0; bool bUsed = false; };
	TMap<TWeakObjectPtr<AAerondelMob>, FGuardPoise> GuardPoise;
	mutable float PlayerPoise = -1.f;
	int32 PlayerPoiseCd = 0;
};
