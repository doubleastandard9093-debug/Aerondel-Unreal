#pragma once
#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "Features/AerondelFeature.h"
#include "VenturesFeature.generated.h"

class UMobDef;
class AAerondelMob;
class UItemDef;
class UNpcDef;
struct FGPanelModel;

/** One Overflare ultimate per weapon family (web adapt/overflare.js ULTS). */
struct FVentureUlt { const TCHAR* Key; const TCHAR* Name; int32 Hits; float Bonus; float Radius; };

/** A saved Lookplate (Kitbash Wardrobe): slot -> look item, slot -> dye name. Up to 8 plates, including weapons. */
struct FVentureOutfit { bool bUsed = false; TMap<FString, FString> Looks; TMap<FString, FString> Dyes; };

/** Townstead furniture in one owned house. */
struct FSteadFurn { bool bChest = false; bool bBed = false; TArray<FString> Hooks; };

/** Town Tie record for one townsfolk (rank 0-5). */
struct FTieRec { int32 Rank = 0; int64 GiftAt = 0; FString TalkAt; bool bHeart = false; bool bErrand = false; };

struct FGreenPlot { FString Crop; int64 PlantedAt = 0; FString Season; bool bCover = false; };
struct FGreenAnimal { FString Type; int64 FedAt = 0; bool bReady = false; };
struct FGreenDecor { FString Id; float X = 0.f; float Z = 0.f; float Rot = 0.f; };

struct FFacetTrail { TWeakObjectPtr<AAerondelMob> Mob; int64 DueMs = 0; };

/** Totals from filled Leyweave nodes (web A.leyFx). */
struct FLeyFx { float LeyGain = 0.f; float WhirlR = 0.f; float GrowSpeed = 0.f; float BuildSpeed = 0.f; float CryCd = 0.f; int32 Stun = 0; int32 CarveWin = 0; };

/** A Hordewake / Ashspire reward card. */
struct FRunPick { FString T, Id, From, Name, Desc; };

struct FKinRec
{
	FString Id, Species, Name;
	int32 Level = 1, Xp = 0, Loyalty = 20, Hp = 14;
	int64 FaintUntil = 0;
};

struct FFramePiece
{
	FString Id, Kind, Tier;
	int32 Gx = 0, Gz = 0, Rot = 0, Hp = 0;
};

struct FLeyFire
{
	FString Kind, Src;
	float Heat = 1.f, X = 0.f, Z = 0.f;
};

struct FKinLive
{
	bool bOn = false;
	FString Id;
	int32 Hp = 0;
	int64 NextMs = 0;
	float Acc = 0.f;
	TWeakObjectPtr<AActor> Pet;
};

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
	static constexpr int32 LookPlateCount = 8;

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
	/** Keyboard: overflare (U), tether (L), super (N), dash (C), ashkin (H), wordcry (;), kinbeast (K), climb (,). J (hub) is handled by the character/HUD. */
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
	bool WardrobeSavePlate(int32 Index);
	bool WardrobeApplyPlate(int32 Index);

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
	/** Flat incoming reduction (Ashkin warder) applied before the hit lands. */
	static int32 FilterIncoming(const UObject* WorldContext, int32 Amount);

	// ---- group 9: Ashkin (Ventures_Ashkin.cpp) — shared follower cap with later Kinbeasts ----
	bool AshSummon(const FString& Item = FString());
	bool AshRecall(const FString& Why = FString(), bool bSilent = false);
	bool AshCraft(const FString& Id);
	void AshEquip(const FString& Item);
	bool AshOut() const;
	bool KinOut() const;
	void RecallKin(const FString& Why, bool bSilent = false);

	// ---- group 20: Kinbeasts (Ventures_Kinbeasts.cpp) — shared follower cap with Ashkin ----
	bool KinThrow();
	bool KinSend(const FString& Id = FString());
	bool KinFeed(const FString& Id = FString());
	bool KinSetActive(const FString& Id);
	bool KinBuy(const FString& Name);

	// ---- group 21: Freeclimb (Ventures_Freeclimb.cpp) ----
	bool ClimbBegin();
	bool Climbing() const;
	void ClimbEnd(const FString& Why, bool bSafe);
	bool ClimbLaunch();
	void TickClimb(float Dt);

	// ---- group 22: Leychemistry (Ventures_Leychem.cpp) ----
	bool LeyIgnite(float X, float Z, const FString& Kind = TEXT("grass"), const FString& Src = FString());
	bool LeyExtinguish(float X, float Z);
	bool LeyOnFire(float X, float Z) const;
	bool LeyProtected(float X, float Z) const;
	bool LeyCanBurn(float X, float Z, const FString& Kind = TEXT("grass")) const;
	void LeyPushWind(float X, float Z, float Fx, float Fz);
	void LeyStrike(float X, float Z);
	void LeyOnCry();
	FString LeyWeather() const;
	bool LeyIsRain() const;
	bool LeyIsStorm() const;
	bool NearTownHouse(FVector2D WebPos, float RadiusM) const;

	// ---- group 23: Freeframe (Ventures_Freeframe.cpp) — not a Townstead, not Greenstead ----
	bool FrameOn(float X, float Z) const;
	bool FrameFlammable(float X, float Z) const;
	bool FrameMetal(FVector2D& Out) const;
	bool FrameClaimed() const;
	bool FrameHome() const;
	bool FrameBuy();
	bool FramePlace(const FString& Kind);
	bool FramePlaceAt(const FString& Kind, int32 Gx, int32 Gz);
	bool FrameRemove(const FString& Id, const FString& Why = FString());
	void FrameSettle();
	bool FrameRepair(const FString& Id);
	bool FrameDamage(float X, float Z, int32 N);
	void FrameGo();

	// ---- group 24: Holdraids (Ventures_Holdraids.cpp) — never a town house or the farm ----
	bool RaidStart();
	bool RaidEnd(bool bWin);
	bool RaidDelay();
	bool RaidSkip();
	bool RaidActive() const;

	// ---- group 10: Vocations (Ventures_Vocations.cpp) ----
	bool VocSwap(const FString& Id);
	bool VocClear();
	bool VocUnlock(const FString& Id);
	bool VocCan(const FString& Id) const;
	bool VocUse(const FString& SkillId);
	bool VocInCombat() const;
	bool VocHoldActive() const;
	FString CurrentVoc() const { return VocId; }
	/** Ley Focus: the next spell spends no runes. */
	static bool ConsumeVocFocus(const UObject* WorldContext);

	// ---- group 11: Wordcries (Ventures_Wordcries.cpp) ----
	bool CryLearn(const FString& Id);
	bool CryUse(const FString& Id = FString());
	bool CryEquip(const FString& Id);
	int32 CryWordsOf(const FString& Id) const;

	// ---- group 12: Townsteads (Ventures_Townsteads.cpp) — trains Construction ----
	bool SteadBuy(const FString& Id);
	bool SteadPlace(const FString& Id, const FString& Kind);
	bool SteadRest(const FString& Id);
	bool SteadStore(const FString& Id, const FString& Item, int32 N = 1);
	bool SteadTake(const FString& Id, const FString& Item, int32 N = 1);
	int32 SteadHookSlots() const;
	int32 SteadBuildMs() const;

	// ---- group 13: Town Ties (Ventures_TownTies.cpp) — not Companion Bond ----
	bool TieTalk(const FString& Id);
	bool TieGift(const FString& Id, const FString& Item);
	bool TieDiscount(const FString& Id) const;
	/** Modest 5% shop discount when a Friendly+ townsfolk last traded with you. */
	static float TieBuyDiscount(const UObject* WorldContext);

	// ---- group 14: Greenstead (Ventures_Greenstead.cpp) — trains Farming ----
	bool GreenBuy();
	bool GreenGo();
	void GreenLeave();
	bool GreenBuySeed(const FString& Seed, int32 N = 1);
	bool GreenPlant(int32 I, const FString& CropId);
	bool GreenHarvest(int32 I);
	bool GreenCover(int32 I);
	bool GreenBuyAnimal(const FString& Type);
	bool GreenFeed(int32 I);
	bool GreenCollect(int32 I);
	bool GreenPreserve(const FString& Item);
	bool GreenPlace(const FString& Id);
	bool GreenPick(int32 I);
	bool GreenHome() const;

	// ---- group 15: Facet Stones (Ventures_Facets.cpp) — not grafts, not a damage percent ----
	bool FacetSocket(const FString& Where, const FString& Stone);
	bool FacetUnsocket(const FString& Where, int32 I);
	bool FacetInstall();
	bool FacetOn(const FString& Id) const;
	static bool FacetOn(const UObject* WorldContext, const FString& Id);
	float FacetWhirlR() const;
	/** Metres added to Reaping Whirl (Facets + Leyweave). */
	static float WhirlBonusM(const UObject* WorldContext);
	/** Extra stun ticks for a special (Ironroot + Leyweave). */
	static int32 StunBonusTicks(const UObject* WorldContext, FName Special);
	static void NoteCombatSpecial(const UObject* WorldContext, FName Special, FName WeaponClass);

	// ---- group 16: Leyweave (Ventures_Leyweave.cpp) — Ley, not Anima ----
	bool LeyOpen() const;
	bool LeyPickStart(const FString& Id);
	bool LeyFill(const FString& Id);
	bool LeyRefund();
	bool LeyHas(const FString& Id) const;
	FLeyFx LeyFx() const;

	// ---- group 17: Hordewake (Ventures_Hordewake.cpp) ----
	bool HordeStart();
	bool HordePick(int32 I);
	void TickHordeFire();

	// ---- group 18: Deepmarks (Ventures_Deepmarks.cpp) ----
	bool MarkStart(int32 Tier);
	/** Hush: prayer recovers more slowly inside a Deepmark. */
	float PrayerRegenMul() const;
	static float PrayerRegenMul(const UObject* WorldContext);

	// ---- group 19: Ashspire (Ventures_Ashspire.cpp) — not the Chronicle ----
	bool SpireStart();
	bool SpirePick(int32 I);
	bool SpireBuy(const FString& Id);

	// ---- group 25: Trailhand (Ventures_Trailhand.cpp) — not Twenty-One Tides or Crown & Anchor ----
	bool TrailDeal();
	bool TrailPlay(int32 HandIndex, int32 Row);
	void TrailClose();
	static bool TrailLegal(const TArray<FIntPoint>& RowSuitsRanks, int32 Rank, int32 Suit);
	bool TrailLive() const { return bTrailLive; }

	// ---- group 26: Skillwords (Ventures_Skillwords.cpp) — Silver Tongue stays Persuade, Lie, Intimidate ----
	bool SkillTry(const FString& Id);
	bool SkillHasOpen(FName NpcId) const;
	void SkillOpenFrom(AActor* Npc);
	void SkillContinue();

	// ---- group 27: Crossed Banners (Ventures_Banners.cpp) — Larkhaven, the bank and the Saga stay open ----
	bool BannerWary(FName Faction) const;
	bool BannerMend(FName Faction);
	void BannerOnWrit(FName Faction);
	bool BannerHideStock(FName ShopId, const AActor* Shop, int32 Price) const;
	FName BannerOpposite(FName Faction) const;

	// ---- group 28: Kitwright (Ventures_Kitwright.cpp) — kits are tools, not grafts or a damage percent ----
	bool ScoreHas(const FString& Id) const;
	TArray<FString> KitList(const FString& Item) const;
	bool KitHas(const FString& KitId) const;
	bool KitOnItem(const FString& Item, const FString& KitId) const;
	bool KitHush() const { return KitHas(TEXT("hush")); }
	bool KitLamp() const { return KitHas(TEXT("lamp")); }
	bool KitCraft(const FString& KitId);
	bool KitAttach(const FString& Item, const FString& KitId);
	bool KitRemove(const FString& Item, int32 Index);
	float KitRangeBonus(bool bRanged) const;
	int32 KitSpeedBonus(bool bRanged) const;
	float KitFocusMul() const { return KitHas(TEXT("sight")) ? 1.2f : 1.f; }
	int32 KitPoiseBonus() const;
	FString KitExamine(const FString& Item, const FString& Text) const;



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
	// group 9 internals
	void ResetAsh();
	void SaveAsh(FJsonObject& A) const;
	void LoadAsh(const FJsonObject& A);
	void TickAsh();
	int32 WardAsh(int32 Amount) const;
	void TabAsh(FGPanelModel& M);

	// group 10 internals
	void ResetVoc();
	void SaveVoc(FJsonObject& A) const;
	void LoadVoc(const FJsonObject& A);
	void TickVoc();
	int32 HitVoc(AAerondelMob* Mob, int32 D);
	void DiedVoc(AAerondelMob* Mob, bool bByPlayer);
	int32 VocIncoming(int32 Amount);
	void VocMarkFight();
	void VocHop(float Back, float Right);
	bool ConsumeVocFocus();
	void TabVoc(FGPanelModel& M);

	// group 11 internals
	void ResetCry();
	void SaveCry(FJsonObject& A) const;
	void LoadCry(const FJsonObject& A);
	int32 CryIncoming(int32 Amount) const;
	bool CryNear(const FString& Id, float& Dist) const;
	void TabCry(FGPanelModel& M);

	// group 12 internals
	void ResetStead();
	void SaveStead(FJsonObject& A) const;
	void LoadStead(const FJsonObject& A);
	void TabStead(FGPanelModel& M);
	FSteadFurn& SteadFurnOf(const FString& Id);
	TMap<FString, int32>& SteadChestOf(const FString& Id);
	const FString* SteadHere() const;
	float SteadLeyBuildBonus() const;

	void ResetTie();
	void SaveTie(FJsonObject& A) const;
	void LoadTie(const FJsonObject& A);
	void TabTie(FGPanelModel& M);
	FTieRec& TieOf(const FString& Id);
	const UNpcDef* TieNpc(const FString& Id) const;
	bool TieNear(const FString& Id, float& Dist) const;
	int32 TieBump(const FString& Id, int32 N);
	int32 GreenGrewOf(const FString& Item) const;
	void GreenGrewAdd(const FString& Item, int32 N);
	void ResetGreen();
	void SaveGreen(FJsonObject& A) const;
	void LoadGreen(const FJsonObject& A);
	void TabGreen(FGPanelModel& M);
	void TickGreen();
	void GreenEnsurePlots();
	int32 GreenPlotN() const;
	int32 GreenAnimalCap() const;
	int32 GreenDecorCap() const;
	int32 GreenGrowMs(const FString& CropId) const;
	int32 GreenFarmXp(int32 Base, bool bInSeason) const;
	float GreenLeyGrowBonus() const;
	int32 GreenCoverCap() const;
	bool GreenCanSeason(const FString& CropId, bool bCovered) const;
	bool GreenRipe(int32 I) const;

	void ResetFacet();
	void SaveFacet(FJsonObject& A) const;
	void LoadFacet(const FJsonObject& A);
	void TabFacet(FGPanelModel& M);
	void TickFacet();
	int32 HitFacet(AAerondelMob* Mob, int32 D);
	void DiedFacet(AAerondelMob* Mob, bool bByPlayer);
	TArray<FString> FacetSocketed() const;
	FString FacetWeaponKey() const;
	float FacetDullChance() const;
	float LeyWhirlBonus() const;
	int32 LeyStunBonus() const;
	bool LeyKey(const FString& Id) const;
	void ResetLey();
	void SaveLey(FJsonObject& A) const;
	void LoadLey(const FJsonObject& A);
	void TabLey(FGPanelModel& M);
	void DiedLey(AAerondelMob* Mob, bool bByPlayer);
	FString LeyDefaultStart() const;

	void ResetHorde();
	void SaveHorde(FJsonObject& A) const;
	void LoadHorde(const FJsonObject& A);
	void TabHorde(FGPanelModel& M);
	void TickHorde();
	void HordeFinish(bool bWin);
	void DiedHorde(AAerondelMob* Mob, bool bByPlayer);
	void HordeSpawn(int32 N);
	void HordeDeck();
	void HordeFire(const FString& Id);
	void HordeStrike(AAerondelMob* M, float Dmg);
	AAerondelMob* HordeNearest(float R) const;
	AAerondelMob* HordeFacing() const;
	int32 HordeCap() const;

	void ResetMark();
	void SaveMark(FJsonObject& A) const;
	void LoadMark(const FJsonObject& A);
	void TabMark(FGPanelModel& M);
	void TickMark();
	void MarkFinish(bool bWin);
	void DiedMark(AAerondelMob* Mob, bool bByPlayer);
	void MarkSpawnRoom();
	void MarkFromBreach(bool bHard, const FString& Grade);
	int32 MarkLevelFor(int32 T) const;

	void ResetSpire();
	void SaveSpire(FJsonObject& A) const;
	void LoadSpire(const FJsonObject& A);
	void TabSpire(FGPanelModel& M);
	void TickSpire();
	void SpireFinish(bool bWin);
	void DiedSpire(AAerondelMob* Mob, bool bByPlayer);
	int32 HitSpire(AAerondelMob* Mob, int32 D);
	void SpireSpawnFloor();
	void SpireEnterFloor();
	void SpireDeck();
	void SpireHeal(float Frac);
	bool SpireHas(const FString& Id) const;
	float AccSpire() const;

	void ResetKin();
	void SaveKin(FJsonObject& A) const;
	void LoadKin(const FJsonObject& A);
	void TabKin(FGPanelModel& M);
	void TickKin();
	int32 KinSoak(int32 Amount);
	FKinRec* KinRecOf(const FString& Id);
	float KinKennelMul() const;
	FString KinBestVessel(int32 NeedTier) const;
	float KinChance(const AAerondelMob* Mob, const FString& Vessel) const;
	void KinSpawnPet(FKinRec& R);

	void ResetClimb();
	void SaveClimb(FJsonObject& A) const;
	void LoadClimb(const FJsonObject& A);
	void TabClimb(FGPanelModel& M);
	bool ClimbProbe(FHitResult& Out) const;

	void ResetChem();
	void SaveChem(FJsonObject& A) const;
	void LoadChem(const FJsonObject& A);
	void TabChem(FGPanelModel& M);
	void TickChem();
	bool LeyWearingMetal() const;

	void ResetFrame();
	void SaveFrame(FJsonObject& A) const;
	void LoadFrame(const FJsonObject& A);
	void TabFrame(FGPanelModel& M);
	void TickFrame();
	void FrameClearMeshes();
	void FrameShow(const FFramePiece& P);
	void FrameRebuild();
	const FFramePiece* FrameAt(int32 Gx, int32 Gz) const;
	FFramePiece* FrameAtMut(int32 Gx, int32 Gz);
	int32 FrameCap() const;
	int32 FrameRange() const;
	TSet<FIntPoint> FrameSupported() const;

	void ResetRaids();
	void SaveRaids(FJsonObject& A) const;
	void LoadRaids(const FJsonObject& A);
	void TabRaids(FGPanelModel& M);
	void TickRaids();
	void RaidClearMobs();
	int32 RaidLive() const;

	void ResetTrail();
	void SaveTrail(FJsonObject& A) const;
	void LoadTrail(const FJsonObject& A);
	void TabTrail(FGPanelModel& M);
	void TrailSettle(int32 Mult, const FString& Msg);
	FString TrailCardLabel(int32 Rank, int32 Suit) const;

	void ResetChecks();
	void SaveChecks(FJsonObject& A) const;
	void LoadChecks(const FJsonObject& A);
	void TabChecks(FGPanelModel& M);
	bool SkillQuestReady(const FString& QuestId) const;

	void ResetBanners();
	void SaveBanners(FJsonObject& A) const;
	void LoadBanners(const FJsonObject& A);
	void TabBanners(FGPanelModel& M);
	FString BannerTownAt(float X, float Z) const;

	void ResetKits();
	void SaveKits(FJsonObject& A) const;
	void LoadKits(const FJsonObject& A);
	void TabKits(FGPanelModel& M);
	FString KitCanTake(const FString& Item, const FString& KitId) const;




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
	FVentureOutfit Outfits[LookPlateCount];

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
		// Hordewake / Deepmarks / Ashspire (live while bOn)
		int32 Level = 1;
		int32 Slots = 2;
		int32 Xp = 0;
		int32 Haste = 0;
		int32 Mag = 0;
		int32 Room = 1;
		int32 Floor = 1;
		int32 AshEarned = 0;
		int64 GoalMs = 0;
		bool bPaused = false;
		bool bBound = false;
		TArray<FString> Weapons;
		TArray<FString> Mods;
		TArray<FString> Boons;
		TArray<FRunPick> Picks;
		TMap<FString, int64> ShotAt;
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

	// Ashkin (web adapt/ashkin.js, save key "ashkin")
	TArray<FString> AshOwned;
	FString AshEquipped;
	int64 AshLastUsedMs = 0;
	struct FAshLive { FString Item, Style, Name; int64 UntilMs = 0; int32 NextTick = 0; bool bOn = false; TWeakObjectPtr<AActor> Spirit; };
	FAshLive AshLive;
	/** Filled by Kinbeasts later; Ashkin only reads it for the shared follower cap. */
	bool bKinOut = false;
	TArray<FKinRec> KinRoster;
	TArray<FString> KinSeen;
	FString KinActive;
	int32 KinNextId = 1;
	int64 KinSwapAt = 0;
	FKinLive KinLive;

	// Vocations (web adapt/vocations.js, save key "voc")
	FString VocId;
	TArray<FString> VocUnlocked;
	TMap<FString, TArray<FString>> VocBars;
	int32 VocFightLeft = 0;
	int32 VocBraceUntil = 0;
	int32 VocHoldUntil = 0;
	int32 VocSootheUntil = 0;
	int32 VocReadUntil = 0;
	bool bVocRetrieve = false;
	bool bVocFocus = false;
	bool bVocRiposteArm = false;
	bool bVocCantProc = false;
	TMap<FString, int32> VocCd;
	TWeakObjectPtr<AAerondelMob> VocReadMob;

	// Wordcries (web adapt/wordcries.js, save key "cries")
	TMap<FString, int32> CryWords;
	FString CryEquipped;
	int32 CryUses = 0;
	int64 CryLastMs = 0;
	int32 CryValorUntil = 0;

	// Townsteads (web adapt/townsteads.js, save key "steads")
	TArray<FString> SteadOwned;
	TMap<FString, TMap<FString, int32>> SteadChests;
	TMap<FString, FSteadFurn> SteadFurn;
	int64 SteadBuildBusyMs = 0;

	// Town Ties (web adapt/townties.js, save key "ties")
	TMap<FString, FTieRec> TieRecs;
	FString TieShopId;

	// Greenstead (web adapt/greenstead.js, save key "green")
	bool bGreenDeed = false;
	TArray<FGreenPlot> GreenPlots;
	TArray<FGreenAnimal> GreenAnimals;
	TArray<FGreenDecor> GreenDecor;
	TMap<FString, int32> GreenGrown;

	// Facet Stones (web adapt/facets.js, save key "facets")
	TMap<FString, TArray<FString>> FacetOnWeap;
	TArray<FString> FacetFrame;
	bool bFacetFrame = false;
	FString FacetSel;
	FName FacetLastSpecial = NAME_None;
	FName FacetLastClass = NAME_None;
	TArray<FFacetTrail> FacetTrails;

	// Leyweave (web adapt/leyweave.js, save key "ley")
	TArray<FString> LeyNodes;
	int32 LeyAmt = 0;
	int32 LeySpent = 0;
	FString LeyStart;
	bool bLeyRec = false;

	// Hordewake (web adapt/hordewake.js, save key "horde")
	int64 HordeBestTime = 0;
	int32 HordeBestLevel = 0;
	int32 HordeRuns = 0;
	TArray<FString> HordeUnlocks;

	// Deepmarks (web adapt/deepmarks.js, save key "marks")
	int32 MarkTierUnlocked = 1;
	TMap<FString, int32> MarkClears;
	TArray<FString> MarkStash;
	FString MarkWeekPinn;

	// Ashspire (web adapt/ashspire.js, save key "spire")
	int32 SpireAsh = 0;
	TArray<FString> SpireUnlocks;
	int32 SpireBestFloor = 0;
	int32 SpireRuns = 0;

	// Freeclimb (web adapt/freeclimb.js, save key "climb")
	bool bClimbUsed = false;
	bool bClimbing = false;
	FVector ClimbNormal = FVector::ZeroVector;
	float ClimbGrav = 1.f;

	// Leychemistry (web adapt/leychem.js, save key "chem")
	bool bChemSeen = false;
	TMap<FIntPoint, FLeyFire> LeyFires;
	float ChemAcc = 0.f;

	// Freeframe (web adapt/freeframe.js, save key "frame")
	bool bFrameDeed = false;
	TArray<FFramePiece> FrameList;
	int32 FrameNextId = 1;
	TArray<TObjectPtr<AActor>> FrameMeshes;

	// Holdraids (web adapt/holdraids.js, save key "raids")
	int64 RaidNext = 0;
	int32 RaidRuns = 0, RaidSkipped = 0, RaidDelayed = 0;
	bool bRaidOn = false;
	bool bRaidWarned = false;
	int32 RaidHits = 0;
	int64 RaidLastChip = 0, RaidStartMs = 0;
	TArray<TWeakObjectPtr<AAerondelMob>> RaidMobs;

	// Trailhand (web adapt/trailhand.js, save key "trail")
	int32 TrailWins = 0, TrailBest = 0, TrailTokens = 0, TrailBet = 50, TrailSel = -1;
	bool bTrailLive = false;
	FString TrailDone;
	TArray<FIntPoint> TrailHand; // X = rank 0-8, Y = suit 0-3
	TArray<TArray<FIntPoint>> TrailRows;

	// Skillwords (web adapt/skillwords.js, save key "checks")
	TArray<FString> CheckUsed;
	TWeakObjectPtr<AActor> SkillNpc;

	// Crossed Banners (web adapt/banners.js, save key "banners")
	TArray<FString> BannerCrosses;
	TSet<FName> BannerWarySet;

	// Kitwright (web adapt/kitwright.js, save key "kits")
	TMap<FString, FString> KitOn; // item display name -> comma-separated kit ids (max 2)
	TArray<FString> KitOwned;
	FString KitSel;
	// Seasonboard unlocks (filled in Phase 22; empty until then)
	TSet<FString> ScoreUnlocks;




};








