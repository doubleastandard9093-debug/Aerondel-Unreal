#include "Features/VenturesFeature.h"
#include "Features/StableCodexFeature.h"
#include "UI/AerondelPanels.h"
#include "Core/AerondelEvents.h"
#include "Core/AerondelPlayerState.h"
#include "Core/PlayerStatsComponent.h"
#include "Combat/AerondelMob.h"
#include "Combat/PlayerCombatComponent.h"
#include "Rules/AerondelRules.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/EquipmentComponent.h"
#include "Bank/BankComponent.h"
#include "Skills/SkillsComponent.h"
#include "World/AerondelScalability.h"
#include "Data/GameDatabase.h"
#include "Data/SkillDef.h"
#include "Data/ItemDef.h"
#include "Data/MobDef.h"
#include "Dom/JsonValue.h"
#include "GameFramework/Pawn.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

// ============================================================================================
// Ventures core (web adapt/core.js, state.js, zone.js helpers) and group 1:
// Overflare, Kitbash Wardrobe, Wayfarer's Ledger, Hidden Reliquaries.
// ============================================================================================

namespace VenturesData
{
	static const FVentureUlt Ults[] = {
		{ TEXT("katana"), TEXT("Thousand Cuts Barrage"), 6, 0.45f, 0.f },
		{ TEXT("scimitar"), TEXT("Sandstorm Reel"), 4, 0.6f, 4.f },
		{ TEXT("glaive"), TEXT("Crescent Tempest"), 3, 0.8f, 5.f },
		{ TEXT("warfan"), TEXT("Gale Sovereign"), 3, 0.7f, 6.f },
		{ TEXT("chakram"), TEXT("Chakram Storm"), 4, 0.6f, 6.f },
		{ TEXT("Ranged"), TEXT("Starfall Deluge"), 4, 0.7f, 6.f },
		{ TEXT("Magic"), TEXT("Astral Cataclysm"), 3, 0.9f, 6.f },
		{ TEXT("Melee"), TEXT("Titan's Overflare"), 3, 0.9f, 3.f } };

	struct FDye { const TCHAR* Name; uint32 Hex; };
	static const FDye Dyes[] = {
		{ TEXT("Emberglow"), 0xc0401a }, { TEXT("Rimefrost"), 0xa8d8f0 }, { TEXT("Mossbank"), 0x4a7a3a }, { TEXT("Duskviolet"), 0x5a3a8a },
		{ TEXT("Saltwhite"), 0xe8e4d8 }, { TEXT("Ironsoot"), 0x2a2a2e }, { TEXT("Goldleaf"), 0xd4a830 }, { TEXT("Tidewater"), 0x2a6a8a },
		{ TEXT("Bloodroot"), 0x7a1a24 }, { TEXT("Ashgrey"), 0x7a7a78 }, { TEXT("Sunflax"), 0xe0c870 }, { TEXT("Nightjet"), 0x14141c } };
	static const TCHAR* LookSlots[] = { TEXT("head"), TEXT("body"), TEXT("legs"), TEXT("shield"), TEXT("cape") };

	// Towns in world.json order (web metres).
	struct FTown { const TCHAR* Name; float X; float Z; };
	static const FTown Towns[] = {
		{ TEXT("Larkhaven"), 0, 0 }, { TEXT("Emberfold"), 50, -24 }, { TEXT("Saltreach"), -80, 60 }, { TEXT("Highcrest"), 120, -40 },
		{ TEXT("Whitecairn"), -60, -80 }, { TEXT("Thistlewell"), 40, 90 }, { TEXT("Rimeholt"), -250, -355 }, { TEXT("Cragmoor Lodge"), 235, -350 },
		{ TEXT("Cinderpost"), 355, -140 }, { TEXT("Mirage Well"), 360, 120 }, { TEXT("Vinehollow"), 210, 360 }, { TEXT("Bogwatch"), -360, 185 },
		{ TEXT("Ashgate Post"), 540, 0 }, { TEXT("Briarwatch"), 380, 400 }, { TEXT("Thunderhold"), -390, 400 }, { TEXT("Lanternmoor"), -545, -10 },
		{ TEXT("Mirrorsand Rest"), -410, -440 }, { TEXT("Slagwatch"), 0, -545 }, { TEXT("Last Light Bastion"), 420, -430 },
		{ TEXT("Snarlwick"), 190, 40 }, { TEXT("Gorrumhold"), -200, -30 }, { TEXT("Sylvaranth"), 30, 230 } };
	static constexpr int32 NumTowns = (int32)UE_ARRAY_COUNT(Towns);

	static const TCHAR* Souls[] = { TEXT("Cinder Warden"), TEXT("Bramble Colossus"), TEXT("Glacier Behemoth"), TEXT("Peak Warlord"), TEXT("Bog Revenant"),
		TEXT("Dune Tyrant"), TEXT("Molten Archon"), TEXT("Voidborn Knight"), TEXT("Sunderlord Vexmar"), TEXT("Captain Saltgrave"), TEXT("Stonehorn"), TEXT("Frostmaw"), TEXT("Ashwyrm") };
	static const TCHAR* Rares[] = { TEXT("Emberfang katana"), TEXT("Briarhook scimitar"), TEXT("Glacierfall glaive"), TEXT("Thunderwing war fan"), TEXT("Mirewraith chakram"),
		TEXT("Mirage scimitar"), TEXT("Magmaheart glaive"), TEXT("Nulledge katana"), TEXT("Vexmar, the Last Eclipse") };
	static const TCHAR* LoreText[] = {
		TEXT("The first Larkhaven bell was cast from a fallen star. It still rings a little sharp."),
		TEXT("Emberfold's forges were lit with a coal that has never once gone out."),
		TEXT("Saltreach sailors leave one fish on the dock each dawn \"for the one below\"."),
		TEXT("Highcrest was a watchtower long before it was a town; its stairs still count to 99."),
		TEXT("Whitecairn's cairns are graves of travellers who knew the road too well."),
		TEXT("The tundra wolves follow the old frost lines, not the scent of prey."),
		TEXT("Skyreach lodges keep a candle lit for climbers who never came down."),
		TEXT("The Mirage Well shows each traveller a different city at the bottom."),
		TEXT("Fen-folk say the Drowned Chapel bell rings when the water is about to rise."),
		TEXT("Vexmar was a knight once. The eclipse took his name before it took his blade."),
		TEXT("Goblin totems are not gods. They are ledgers of grudges, carved in order."),
		TEXT("Elven wards hum at a pitch only grief can hear.") };
	static constexpr int32 NumLore = (int32)UE_ARRAY_COUNT(LoreText);
	struct FVillage { const TCHAR* Id; const TCHAR* Name; float X; float Z; float R; };
	static const FVillage Villages[] = { { TEXT("snarlwick"), TEXT("Snarlwick"), 190, 40, 26 }, { TEXT("gorrumhold"), TEXT("Gorrumhold"), -200, -30, 30 }, { TEXT("sylvaranth"), TEXT("Sylvaranth"), 30, 230, 28 } };
	static constexpr int32 LedgerAll = (int32)(UE_ARRAY_COUNT(Souls) + UE_ARRAY_COUNT(Rares) + UE_ARRAY_COUNT(Villages)) + NumLore;

	static const TCHAR* Runes[] = { TEXT("Triangle"), TEXT("Diamond"), TEXT("Circle"), TEXT("Cross") };
	static const FVector2D RelicOff[] = { FVector2D(-16, -12), FVector2D(18, -10), FVector2D(-14, 16), FVector2D(15, 17) };

	static FVector2D LoreStone(int32 i) { return FVector2D(Towns[i].X + 7.f, Towns[i].Z + 5.f); }
	static FString Pct(float F) { return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(F * 100.f)); }
}
using namespace VenturesData;

static void VAct(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }

// ---------------------------------------------------------------- lifecycle / save

void UVenturesFeature::Initialize(FSubsystemCollectionBase& Collection)
{
	UAerondelEvents* Ev = Collection.InitializeDependency<UAerondelEvents>();
	Super::Initialize(Collection);
	if (Ev) Ev->OnMobKilled.AddDynamic(this, &UVenturesFeature::HandleMobKilled);
}

UVenturesFeature* UVenturesFeature::Get(const UObject* WorldContext)
{
	const UWorld* W = WorldContext ? WorldContext->GetWorld() : nullptr;
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UVenturesFeature>() : nullptr;
}

void UVenturesFeature::ResetState()
{
	Unknown.Reset();
	ResetQuick();
	ResetCombat();
	ResetZone();
	ResetGear();
	ResetRaid();
	ResetSeason();
}

void UVenturesFeature::SaveState(FJsonObject& Out) const
{
	for (const auto& U : Unknown) Out.SetField(U.Key, U.Value); // keys from a newer build survive a round trip
	SaveQuick(Out);
	SaveCombat(Out);
	SaveZone(Out);
	SaveGear(Out);
	SaveRaid(Out);
	SaveSeason(Out);
}

void UVenturesFeature::LoadState(const FJsonObject& In)
{
	ResetState();
	for (const auto& V : In.Values) Unknown.Add(V.Key, V.Value);
	LoadQuick(In);
	LoadCombat(In);
	LoadZone(In);
	LoadGear(In);
	LoadRaid(In);
	LoadSeason(In);
}

void UVenturesFeature::OnGameTick()
{
	++TickN;
	TickQuick();
	TickZone();
	TickGear();
	TickRaid();
}

void UVenturesFeature::OnPlayerDied()
{
	Puz = FRelicPuzzle();
	if (Run.bOn) RunEnd(false);
}

void UVenturesFeature::HandleMobKilled(UMobDef* Mob)
{
	if (!Mob) return;
	KilledQuick(Mob->DisplayName.ToString(), nullptr);
}

// ---------------------------------------------------------------- combat totals

float UVenturesFeature::DamageDealtBonus(const UMobDef* Target) const { return FMath::Clamp(SumDmg(Target), 0.f, CapDmg); }
float UVenturesFeature::AccuracyBonus(const UMobDef* Target) const { return FMath::Clamp(SumAcc(Target), 0.f, CapAcc); }
float UVenturesFeature::DamageTakenMult(const UMobDef* Source, float HpFrac) const
{
	FlareAdd(1.2f); // only called for a hit that lands (Amount > 0)
	return (1.f - FMath::Clamp(SumDr(Source), 0.f, CapDr)) * TakenZone();
}
float UVenturesFeature::XpBonus(FName SkillId) const { return FMath::Clamp(SumXp(SkillId), 0.f, CapXp); }

// Each group adds its terms here (web A.mods.dmg / acc / dr / xp).
float UVenturesFeature::SumDmg(const UMobDef* Target) const
{
	float T = (TickN < FlareBuffUntil ? 0.3f : 0.f) + LedgerTier() * 0.01f;
	T += DmgCombat(Target) + DmgGear(Target) + DmgRaid(Target);
	return T;
}
float UVenturesFeature::SumAcc(const UMobDef* Target) const
{
	float T = AccCombat() + AccGear() + AccRaid() + AccSeason();
	return T;
}
float UVenturesFeature::SumDr(const UMobDef* Source) const
{
	float T = LedgerTier() * 0.01f + DrCombat() + DrGear() + DrRaid() + DrSeason();
	return T;
}
float UVenturesFeature::SumXp(FName SkillId) const
{
	float T = LedgerTier() * 0.02f + XpCombat(SkillId) + XpZone(SkillId) + XpGear(SkillId) + XpRaid(SkillId) + XpSeason();
	return T;
}
float UVenturesFeature::FlareGainBonus() const
{
	float T = FlareCombat() + FlareGear();
	return T;
}

// ---------------------------------------------------------------- hooks

int32 UVenturesFeature::HookPlayerHit(AAerondelMob* Mob, int32 Damage, AActor* Attacker)
{
	UVenturesFeature* V = Get(Mob);
	if (!V || !Mob || !Attacker || !AAerondelPlayerState::FromActor(Attacker)) return Damage;
	const int32 D = V->HitCombat(Mob, FMath::Max(0, Damage));
	const bool bFlareStrike = V->StrikeKind && FCString::Strcmp(V->StrikeKind, TEXT("flare")) == 0;
	if (D > 0 && !bFlareStrike) V->FlareAdd(1.5f + FMath::Min(4.f, D / (float)FMath::Max(1, Mob->GetMaxHp()) * 25.f));
	return D;
}

void UVenturesFeature::HookMobDied(AAerondelMob* Mob, AActor* Killer)
{
	UVenturesFeature* V = Get(Mob);
	if (!V || !Mob || !Mob->Definition) return;
	V->DiedMore(Mob, AAerondelPlayerState::FromActor(Killer) != nullptr);
}

void UVenturesFeature::DiedMore(AAerondelMob* Mob, bool bByPlayer)
{
	DiedCombat(Mob, bByPlayer);
	DiedZone(Mob, bByPlayer);
}

bool UVenturesFeature::BuildTabMore(const FString& Tab, FGPanelModel& M)
{
	if (Tab == TEXT("crest")) { TabCrest(M); return true; }
	if (Tab == TEXT("sigils")) { TabSigils(M); return true; }
	if (Tab == TEXT("grafts")) { TabGrafts(M); return true; }
	if (Tab == TEXT("stance")) { TabStance(M); return true; }
	if (Tab == TEXT("tides")) { TabTides(M); return true; }
	if (Tab == TEXT("breach")) { TabBreach(M); return true; }
	if (Tab == TEXT("tether")) { TabTether(M); return true; }
	if (Tab == TEXT("geodes")) { TabGeodes(M); return true; }
	if (Tab == TEXT("ascent")) { TabAscent(M); return true; }
	if (Tab == TEXT("lumen")) { TabLumen(M); return true; }
	if (Tab == TEXT("lattice")) { TabLattice(M); return true; }
	if (Tab == TEXT("vigil")) { TabVigil(M); return true; }
	if (Tab == TEXT("attune")) { TabAttune(M); return true; }
	if (Tab == TEXT("hold")) { TabHold(M); return true; }
	if (Tab == TEXT("chron")) { TabChron(M); return true; }
	return false;
}

void UVenturesFeature::OverviewMore(FGPanelModel& M)
{
	VAct(M, TEXT("Crestbreak"), [this] { GoTab(TEXT("crest")); });
	VAct(M, TEXT("Engrave Sigils"), [this] { GoTab(TEXT("sigils")); });
	VAct(M, TEXT("Riven Grafts"), [this] { GoTab(TEXT("grafts")); });
	VAct(M, TEXT("Twin Stance"), [this] { GoTab(TEXT("stance")); });
	VAct(M, TideRegion >= 0 ? TEXT("Burst Tides (surging!)") : TEXT("Burst Tides"), [this] { GoTab(TEXT("tides")); });
	VAct(M, TEXT("Breach Runs"), [this] { GoTab(TEXT("breach")); });
	VAct(M, TEXT("Tetherline (L)"), [this] { GoTab(TEXT("tether")); });
	VAct(M, TEXT("Sealed Geodes"), [this] { GoTab(TEXT("geodes")); });
	VAct(M, TEXT("Ascent Rolls"), [this] { GoTab(TEXT("ascent")); });
	VAct(M, TEXT("Lumen Rise"), [this] { GoTab(TEXT("lumen")); });
	VAct(M, TEXT("Anima Lattice"), [this] { GoTab(TEXT("lattice")); });
	VAct(M, TEXT("Vigil Gates"), [this] { GoTab(TEXT("vigil")); });
	VAct(M, TEXT("Elemental Attunements (N)"), [this] { GoTab(TEXT("attune")); });
	VAct(M, TEXT("Hearthhold"), [this] { GoTab(TEXT("hold")); });
	VAct(M, TEXT("Season Chronicle"), [this] { GoTab(TEXT("chron")); });
	VAct(M, TEXT("Dash / Glide (C)"), [this] { Dash(); }, true, true);
}

float UVenturesFeature::HookRespawnScale(const AAerondelMob* Mob)
{
	const UVenturesFeature* V = Get(Mob);
	return V ? V->RespawnZone(Mob) : 1.f;
}

void UVenturesFeature::OnKey(FName Key)
{
	if (Key == FName(TEXT("overflare"))) { Flare(); return; }
	if (Key == FName(TEXT("tether"))) { Tether(); return; }
	if (Key == FName(TEXT("super"))) { SuperUse(); return; }
	if (Key == FName(TEXT("dash"))) { Dash(); return; }
}

// ---------------------------------------------------------------- helpers

const UItemDef* UVenturesFeature::Weapon() const
{
	const AAerondelPlayerState* PS = GetPS();
	return PS && PS->Equipment ? PS->Equipment->GetEquipped(EEquipSlot::Weapon) : nullptr;
}

FName UVenturesFeature::WeaponClass() const { const UItemDef* W = Weapon(); return W ? W->WeaponClass : NAME_None; }

bool UVenturesFeature::IsMagic() const
{
	const AAerondelPlayerState* PS = GetPS();
	if (PS && PS->Stats && !PS->Stats->AutocastSpell.IsNone()) return true;
	const UItemDef* W = Weapon();
	const FString N = W ? W->DisplayName.ToString() : FString();
	return N.Contains(TEXT("staff")) || N.Contains(TEXT("wand"));
}

bool UVenturesFeature::IsRangedNow() const { const AAerondelPlayerState* PS = GetPS(); return PS && UAerondelRules::IsRanged(PS); }

FString UVenturesFeature::Style() const
{
	if (IsMagic()) return TEXT("Magic");
	if (IsRangedNow()) return TEXT("Ranged");
	const UItemDef* W = Weapon();
	const FName C = WeaponClass();
	if (W && (W->IsBlade() || C == FName(TEXT("warfan")) || C == FName(TEXT("chakram")))) return TEXT("Swordsmanship");
	return TEXT("Melee");
}

bool UVenturesFeature::IsBossy(const AAerondelMob* Mob)
{
	const UMobDef* D = Mob ? Mob->Definition.Get() : nullptr;
	if (!D) return false;
	if (D->bBoss) return true;
	const FString N = D->DisplayName.ToString();
	static const TCHAR* Words[] = { TEXT("Warden"), TEXT("Colossus"), TEXT("Behemoth"), TEXT("Warlord"), TEXT("Revenant"), TEXT("Tyrant"), TEXT("Archon"), TEXT("Knight"), TEXT("Sunderlord") };
	if (D->CombatLevel >= 140) for (const TCHAR* Wd : Words) if (N.Contains(Wd)) return true;
	return false;
}

TArray<AAerondelMob*> UVenturesFeature::MobsNear(FVector2D WebPos, float RadiusM, int32 Cap) const
{
	TArray<AAerondelMob*> Out;
	UGameInstance* GI = GetGameInstance(); UWorld* W = GI ? GI->GetWorld() : nullptr;
	if (!W) return Out;
	const FVector C = WebToWorld(WebPos.X, WebPos.Y);
	for (TActorIterator<AAerondelMob> It(W); It; ++It)
	{
		AAerondelMob* M = *It;
		if (!M || M->IsDead() || M->IsHidden()) continue;
		if (FVector::Dist2D(M->GetActorLocation(), C) < RadiusM * 100.f) { Out.Add(M); if (Out.Num() >= Cap) break; }
	}
	return Out;
}

AAerondelMob* UVenturesFeature::CurrentTarget() const
{
	const AAerondelPlayerState* PS = GetPS(); const APawn* P = PS ? PS->GetPawn() : nullptr;
	const UPlayerCombatComponent* C = P ? P->FindComponentByClass<UPlayerCombatComponent>() : nullptr;
	AAerondelMob* M = C ? C->GetTarget() : nullptr;
	return M && !M->IsDead() ? M : nullptr;
}

int32 UVenturesFeature::Strike(AAerondelMob* Mob, float Acc, float Dmg, const TCHAR* Kind)
{
	AAerondelPlayerState* PS = GetPS(); APawn* P = PS ? PS->GetPawn() : nullptr;
	if (!PS || !P || !Mob || Mob->IsDead()) return 0;
	FSwingMods M; M.Accuracy = Acc; M.Damage = Dmg;
	const int32 D = UAerondelRules::RollPlayerHit(PS, Mob->Definition, Mob->GetHp(), Mob->GetMaxHp(), M);
	if (const UGameDatabase* Db = UGameDatabase::Get())
		for (const auto& X : UAerondelRules::CombatXp(PS, D))
			if (X.Value > 0.f && PS->Skills) PS->Skills->AddXp(Db->FindSkill(X.Key), X.Value * UAerondelRules::XpMultiplier(PS, X.Key));
	const TCHAR* Prev = StrikeKind; StrikeKind = Kind;
	Mob->TakePlayerDamage(D, P);
	StrikeKind = Prev;
	return D;
}

bool UVenturesFeature::Owns(const FString& Item) const
{
	const AAerondelPlayerState* PS = GetPS(); const UGameDatabase* Db = UGameDatabase::Get();
	const UItemDef* I = Db ? Db->FindItem(Id(Item)) : nullptr;
	if (!PS || !I) return false;
	if (PS->Inventory && PS->Inventory->CountOf(I) > 0) return true;
	if (PS->Bank && PS->Bank->CountOf(I) > 0) return true;
	if (PS->Equipment) for (const auto& E : PS->Equipment->GetAll()) if (E.Value.Get() == I) return true;
	return false;
}

int32 UVenturesFeature::CapFoes(int32 N) const
{
	const EAerondelTier T = UAerondelScalability::GetAppliedTier();
	return FMath::Min(N, (T == EAerondelTier::Low || T == EAerondelTier::Quest) ? 8 : 15);
}

int32 UVenturesFeature::VLevel() const
{
	const int32 Best = FMath::Max3(SkillLevel(TEXT("Ranged")), SkillLevel(TEXT("Magic")), SkillLevel(TEXT("Swordsmanship")));
	return FMath::Max(10, FMath::RoundToInt((SkillLevel(TEXT("Attack")) + SkillLevel(TEXT("Strength")) + SkillLevel(TEXT("Defence")) + SkillLevel(TEXT("Hitpoints")) + Best) / 5.f));
}

FString UVenturesFeature::TodayUtc() { return FDateTime::UtcNow().ToString(TEXT("%Y-%m-%d")); }

// ---------------------------------------------------------------- group 1 state

void UVenturesFeature::ResetQuick()
{
	Meter = 0.f; bWasFull = false; FlareUses = 0; FlareBuffUntil = 0; PendingStrikes.Reset();
	Dyes.Reset(); for (FVentureOutfit& O : Outfits) O = FVentureOutfit();
	Souls.Reset(); Rares.Reset(); Lore.Reset(); Trinkets.Reset();
	RelicDay.Reset(); RelicDug.Reset(); RelicTotal = 0; Puz = FRelicPuzzle();
}

static TSharedPtr<FJsonObject> StrMapObj(const TMap<FString, FString>& M)
{
	TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
	for (const auto& E : M) O->SetStringField(E.Key, E.Value);
	return O;
}

static void ReadStrMap(const TSharedPtr<FJsonObject>& O, TMap<FString, FString>& Out)
{
	Out.Reset();
	if (O.IsValid()) for (const auto& E : O->Values) if (E.Value.IsValid() && E.Value->Type == EJson::String) Out.Add(E.Key, E.Value->AsString());
}

void UVenturesFeature::SaveQuick(FJsonObject& A) const
{
	{ TSharedPtr<FJsonObject> F = MakeShared<FJsonObject>(); F->SetNumberField(TEXT("meter"), Meter); F->SetNumberField(TEXT("uses"), FlareUses); A.SetObjectField(TEXT("flare"), F); }
	{
		TSharedPtr<FJsonObject> W = MakeShared<FJsonObject>(); W->SetObjectField(TEXT("dyes"), StrMapObj(Dyes));
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const FVentureOutfit& O : Outfits)
		{
			if (!O.bUsed) { Arr.Add(MakeShared<FJsonValueNull>()); continue; }
			TSharedPtr<FJsonObject> J = MakeShared<FJsonObject>(); J->SetObjectField(TEXT("looks"), StrMapObj(O.Looks)); J->SetObjectField(TEXT("dyes"), StrMapObj(O.Dyes));
			Arr.Add(MakeShared<FJsonValueObject>(J));
		}
		W->SetArrayField(TEXT("outfits"), Arr); A.SetObjectField(TEXT("wardrobe"), W);
	}
	{ TSharedPtr<FJsonObject> L = MakeShared<FJsonObject>(); PutStrArr(*L, TEXT("souls"), Souls); PutStrArr(*L, TEXT("rares"), Rares); PutStrArr(*L, TEXT("lore"), Lore); PutStrArr(*L, TEXT("trinkets"), Trinkets); A.SetObjectField(TEXT("ledger"), L); }
	{
		TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>(); R->SetStringField(TEXT("day"), RelicDay);
		TArray<TSharedPtr<FJsonValue>> D; for (int32 I : RelicDug) D.Add(MakeShared<FJsonValueNumber>(I));
		R->SetArrayField(TEXT("dug"), D); R->SetNumberField(TEXT("total"), RelicTotal); A.SetObjectField(TEXT("reliq"), R);
	}
}

void UVenturesFeature::LoadQuick(const FJsonObject& A)
{
	if (TSharedPtr<FJsonObject> F = JObj(A, TEXT("flare"))) { Meter = FMath::Clamp((float)JNum(*F, TEXT("meter")), 0.f, 100.f); FlareUses = FMath::Max(0, (int32)JInt(*F, TEXT("uses"))); bWasFull = Meter >= 100.f; }
	if (TSharedPtr<FJsonObject> W = JObj(A, TEXT("wardrobe")))
	{
		TMap<FString, FString> D; ReadStrMap(JObj(*W, TEXT("dyes")), D);
		for (const auto& E : D) for (const FDye& Dy : VenturesData::Dyes) if (E.Value == Dy.Name) Dyes.Add(E.Key, E.Value);
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (W->TryGetArrayField(TEXT("outfits"), Arr) && Arr)
			for (int32 i = 0; i < FMath::Min(3, Arr->Num()); ++i)
			{
				const TSharedPtr<FJsonValue>& V = (*Arr)[i];
				if (!V.IsValid() || V->Type != EJson::Object) continue;
				const TSharedPtr<FJsonObject> O = V->AsObject();
				Outfits[i].bUsed = true; ReadStrMap(JObj(*O, TEXT("looks")), Outfits[i].Looks); ReadStrMap(JObj(*O, TEXT("dyes")), Outfits[i].Dyes);
			}
	}
	if (TSharedPtr<FJsonObject> L = JObj(A, TEXT("ledger"))) { Souls = JStrArr(*L, TEXT("souls")); Rares = JStrArr(*L, TEXT("rares")); Lore = JStrArr(*L, TEXT("lore")); Trinkets = JStrArr(*L, TEXT("trinkets")); }
	if (TSharedPtr<FJsonObject> R = JObj(A, TEXT("reliq")))
	{
		RelicDay = JStr(*R, TEXT("day")); RelicTotal = FMath::Max(0, (int32)JInt(*R, TEXT("total")));
		const TArray<TSharedPtr<FJsonValue>>* D = nullptr;
		if (R->TryGetArrayField(TEXT("dug"), D) && D) for (const TSharedPtr<FJsonValue>& V : *D) if (V.IsValid() && V->Type == EJson::Number) RelicDug.AddUnique((int32)V->AsNumber());
	}
}

void UVenturesFeature::TickQuick()
{
	// Ledger: rare drops owned, village trinkets and lore stones by proximity (every 10 ticks, like the web A.every(10)).
	if (TickN % 10 == 0)
	{
		for (const TCHAR* R : VenturesData::Rares) if (!Rares.Contains(R) && Owns(R)) LedgerAdd(1, R, R);
		FVector2D P;
		if (PlayerWebPos(P))
		{
			for (const FVillage& V : Villages) if (!Trinkets.Contains(V.Id) && FVector2D::Distance(P, FVector2D(V.X, V.Z)) < V.R) LedgerAdd(3, V.Id, FString(V.Name) + TEXT(" trinket"));
		}
	}
	if (TickN % 2 == 0)
	{
		FVector2D P;
		if (PlayerWebPos(P))
			for (int32 i = 0; i < NumLore; ++i)
			{
				const FString Lid = FString::Printf(TEXT("lore%d"), i);
				if (!Lore.Contains(Lid) && FVector2D::Distance(P, LoreStone(i)) < 3.f)
				{
					Say(FString::Printf(TEXT("A lore stone reads: \"%s\""), LoreText[i]));
					LedgerAdd(2, Lid, TEXT("a lore stone"));
				}
			}
	}
	// Wardrobe: armour you own unlocks as a look (web every 25 ticks).
	if (TickN % 25 == 0)
		if (UStableCodexFeature* Cx = Feature<UStableCodexFeature>())
			if (const AAerondelPlayerState* PS = GetPS())
				for (const UInventoryComponent* Bag : { (const UInventoryComponent*)PS->Inventory.Get(), (const UInventoryComponent*)PS->Bank.Get() })
					if (Bag) for (const FItemStack& S : Bag->GetSlots())
					{
						if (S.IsEmpty()) continue;
						const EEquipSlot E = S.Item->EquipSlot;
						if (E == EEquipSlot::Head || E == EEquipSlot::Body || E == EEquipSlot::Legs || E == EEquipSlot::Shield || E == EEquipSlot::Cape) Cx->MarkSeen(S.Item->DisplayName.ToString());
					}
	// Reliquary seal timer.
	if (Puz.bOn && NowMs() > Puz.EndMs) RelicFinish(false);
}

void UVenturesFeature::KilledQuick(const FString& Name, AAerondelMob* Mob)
{
	for (const TCHAR* S : VenturesData::Souls) if (Name.StartsWith(S)) { if (!Souls.Contains(S)) LedgerAdd(0, S, FString(S) + TEXT("'s soul")); break; }
}

// ---------------------------------------------------------------- Overflare

void UVenturesFeature::FlareAdd(float N) const
{
	Meter = FMath::Min(100.f, Meter + N * (1.f + FlareGainBonus()));
	if (Meter >= 100.f && !bWasFull) { bWasFull = true; Say(TEXT("Overflare is ready! Press U to unleash it.")); }
}

const FVentureUlt& UVenturesFeature::Ult() const
{
	const FName C = WeaponClass();
	if (!C.IsNone()) for (const FVentureUlt& U : Ults) if (C == FName(U.Key)) return U;
	const FString S = Style();
	for (const FVentureUlt& U : Ults) if (S == U.Key) return U;
	return Ults[UE_ARRAY_COUNT(Ults) - 1];
}

bool UVenturesFeature::Flare()
{
	if (Meter < 100.f) { Say(FString::Printf(TEXT("Overflare is at %d%%. Keep fighting to fill it."), FMath::FloorToInt(Meter))); return false; }
	const FVentureUlt& U = Ult();
	FVector2D P; PlayerWebPos(P);
	TArray<AAerondelMob*> Foes = U.Radius > 0.f ? MobsNear(P, U.Radius, 6) : TArray<AAerondelMob*>();
	if (AAerondelMob* T = CurrentTarget()) if (!Foes.Contains(T)) Foes.Insert(T, 0);
	if (Foes.Num() == 0) { Say(TEXT("No foe close enough for your Overflare.")); return false; }
	Meter = 0.f; ++FlareUses; bWasFull = false; FlareBuffUntil = TickN + FlareBuffTicks;
	Say(FString::Printf(TEXT("%s!"), U.Name));
	const int32 N = U.Radius > 0.f ? FMath::Min(6, Foes.Num()) : 1;
	FPendingStrike S; S.Left = U.Hits; S.Bonus = U.Bonus;
	for (int32 i = 0; i < N; ++i) S.Foes.Add(Foes[i]);
	PendingStrikes.Add(S);
	SweepFlareStrikes();
	return true;
}

void UVenturesFeature::SweepFlareStrikes()
{
	// One strike per foe every 140 ms (web setTimeout chain).
	for (int32 i = PendingStrikes.Num() - 1; i >= 0; --i)
	{
		FPendingStrike& S = PendingStrikes[i];
		for (const TWeakObjectPtr<AAerondelMob>& M : S.Foes) if (M.IsValid()) Strike(M.Get(), 0.4f, S.Bonus, TEXT("flare"));
		if (--S.Left <= 0) PendingStrikes.RemoveAt(i);
	}
	if (PendingStrikes.Num() > 0)
		if (UGameInstance* GI = GetGameInstance())
		{
			FTimerHandle H;
			GI->GetTimerManager().SetTimer(H, FTimerDelegate::CreateUObject(this, &UVenturesFeature::SweepFlareStrikes), 0.14f, false);
		}
}

// ---------------------------------------------------------------- Kitbash Wardrobe

bool UVenturesFeature::BodyDye(const UObject* WorldContext, FLinearColor& Out)
{
	const UVenturesFeature* V = Get(WorldContext);
	const FString* D = V ? V->Dyes.Find(TEXT("body")) : nullptr;
	if (!D) return false;
	for (const FDye& Dy : VenturesData::Dyes) if (*D == Dy.Name) { Out = FLinearColor(FColor((Dy.Hex >> 16) & 0xff, (Dy.Hex >> 8) & 0xff, Dy.Hex & 0xff)); return true; }
	return false;
}

void UVenturesFeature::WardrobeApply(const FVentureOutfit& O)
{
	if (UStableCodexFeature* Cx = Feature<UStableCodexFeature>())
		for (const TCHAR* S : LookSlots) { const FString* L = O.Looks.Find(S); Cx->SetLook(S, L ? *L : FString()); }
	Dyes = O.Dyes;
}

// ---------------------------------------------------------------- Wayfarer's Ledger

bool UVenturesFeature::LedgerAdd(int32 Kind, const FString& Entry, const FString& Label)
{
	TArray<FString>* L = Kind == 0 ? &Souls : Kind == 1 ? &Rares : Kind == 2 ? &Lore : &Trinkets;
	if (L->Contains(Entry)) return false;
	const int32 T0 = LedgerTier();
	L->Add(Entry);
	Say(FString::Printf(TEXT("Wayfarer's Ledger: %s recorded (%d/%d)."), *Label, LedgerCount(), LedgerAll));
	if (LedgerTier() > T0) Say(FString::Printf(TEXT("Ledger milestone %d: +1%% damage, 1%% less damage taken and +2%% XP."), LedgerTier()));
	return true;
}

// ---------------------------------------------------------------- Hidden Reliquaries

const TArray<FVentureRelic>& UVenturesFeature::Relics() const
{
	static TArray<FVentureRelic> R;
	if (R.Num() == 0)
		for (int32 i = 0; i < 16; ++i)
		{
			const FTown& T = Towns[i % NumTowns];
			const FVector2D O = RelicOff[((i / NumTowns) % 4 + (i % 2)) % 4];
			FVentureRelic S; S.Index = i; S.Pos = FVector2D(T.X + O.X, T.Z + O.Y); S.Tier = 1 + (i % 3);
			R.Add(S);
		}
	return R;
}

bool UVenturesFeature::RelicNear(FVentureRelic& Out, float& Dist) const
{
	FVector2D P; if (!PlayerWebPos(P)) return false;
	const bool bToday = RelicDay == TodayUtc();
	float Best = 1e9f; bool bAny = false;
	for (const FVentureRelic& S : Relics())
	{
		if (bToday && RelicDug.Contains(S.Index)) continue;
		const float D = FVector2D::Distance(P, S.Pos);
		if (D < Best) { Best = D; Out = S; bAny = true; }
	}
	Dist = Best;
	return bAny;
}

void UVenturesFeature::RelicDig()
{
	if (RelicDay != TodayUtc()) { RelicDay = TodayUtc(); RelicDug.Reset(); }
	if (!Owns(TEXT("Dowsing rod"))) { Say(TEXT("You need a dowsing rod (Crafting 10: 1 Logs + 1 Iron bar, or buy one from Wayfinder Orla in Larkhaven).")); return; }
	FVentureRelic S; float D = 0.f;
	if (!RelicNear(S, D)) { Say(TEXT("Every reliquary has been dug up today. They reseal at midnight UTC.")); return; }
	if (D > 3.f)
	{
		FVector2D P; PlayerWebPos(P);
		// Web: atan2(dx, dz) with +z = south on the web map.
		const float Dir = FMath::Atan2(S.Pos.X - P.X, S.Pos.Y - P.Y);
		static const TCHAR* Comp[] = { TEXT("south"), TEXT("south-east"), TEXT("east"), TEXT("north-east"), TEXT("north"), TEXT("north-west"), TEXT("west"), TEXT("south-west") };
		const int32 Ix = ((FMath::RoundToInt(Dir / (PI / 4.f)) % 8) + 8) % 8;
		Say(FString::Printf(TEXT("The rod pulls %s. %s"), Comp[Ix], D < 15.f ? TEXT("Very close!") : D < 40.f ? TEXT("Warm.") : *FString::Printf(TEXT("Far (about %d paces)."), FMath::RoundToInt(D))));
		return;
	}
	const int32 N = 3 + S.Tier;
	const int32 Secs = 6 + S.Tier * 2 + SkillLevel(TEXT("Thieving")) / 10;
	Puz = FRelicPuzzle(); Puz.bOn = true; Puz.Site = S; Puz.EndMs = NowMs() + Secs * 1000ll;
	for (int32 i = 0; i < N; ++i) Puz.Seq.Add(FMath::RandRange(0, 3));
	GoTab(TEXT("reliq"));
}

void UVenturesFeature::RelicPress(int32 Rune)
{
	if (!Puz.bOn) return;
	if (NowMs() > Puz.EndMs || !Puz.Seq.IsValidIndex(Puz.At) || Rune != Puz.Seq[Puz.At]) { RelicFinish(false); return; }
	if (++Puz.At >= Puz.Seq.Num()) RelicFinish(true);
}

void UVenturesFeature::RelicFinish(bool bOk)
{
	const FVentureRelic S = Puz.Site;
	Puz = FRelicPuzzle();
	if (!bOk) { Say(TEXT("The seal flares and locks. Try again.")); return; }
	RelicDug.AddUnique(S.Index); ++RelicTotal;
	const int32 G = 150 * S.Tier * (1 + SkillLevel(TEXT("Thieving")) / 25);
	GiveCoins(G); GiveXp(TEXT("Thieving"), 60.f * S.Tier); FlareAdd(20.f);
	Say(FString::Printf(TEXT("You open a tier %d reliquary: %d coins."), S.Tier, G));
}

// ---------------------------------------------------------------- panels

bool UVenturesFeature::IsVenturePanel(const FString& P)
{
	return P.StartsWith(TEXT("ventures")) || P == TEXT("reliquary") || P == TEXT("grafts") || P == TEXT("breach") || P == TEXT("hold");
}

void UVenturesFeature::GoTab(const FString& Tab) const
{
	const FName N(*(Tab.IsEmpty() ? FString(TEXT("ventures")) : TEXT("ventures_") + Tab));
	if (UGameInstance* GI = GetGameInstance()) if (UAerondelEvents* Ev = GI->GetSubsystem<UAerondelEvents>()) Ev->OnServiceRequested.Broadcast(N, nullptr);
}


void UVenturesFeature::Tabs(FGPanelModel& M, const FString& Cur)
{
	if (!Cur.IsEmpty()) VAct(M, TEXT("< Ventures"), [this] { GoTab(FString()); });
	VAct(M, TEXT("Close"), [] {}, true, true);
}

bool UVenturesFeature::BuildPanel(const FString& P, FGPanelModel& M)
{
	if (!IsVenturePanel(P)) return false;
	FString Tab = P == TEXT("reliquary") ? FString(TEXT("reliq")) : P.StartsWith(TEXT("ventures_")) ? P.Mid(9) : P.StartsWith(TEXT("ventures")) ? FString() : P;
	M.bLive = true;
	if (P == TEXT("reliquary"))
	{
		VAct(M, FString::Printf(TEXT("Buy a dowsing rod (%d coins)"), RodPrice), [this]
		{
			if (!TakeCoins(RodPrice)) { Say(TEXT("Wayfinder Orla: Come back with 150 coins.")); return; }
			GiveItem(TEXT("Dowsing rod"), 1); Say(TEXT("Wayfinder Orla: Dowse in the Reliquaries tab and follow the pull."));
		});
	}
	if (Tab == TEXT("flare")) TabFlare(M);
	else if (Tab == TEXT("wardrobe")) TabWardrobe(M);
	else if (Tab == TEXT("ledger")) TabLedger(M);
	else if (Tab == TEXT("reliq")) TabReliq(M);
	else if (!BuildTabMore(Tab, M)) { Tab.Reset(); TabOverview(M); }
	Tabs(M, Tab);
	return true;
}

void UVenturesFeature::TabOverview(FGPanelModel& M)
{
	M.Title = TEXT("Ventures");
	M.Lines.Add(TEXT("New systems inspired by other games, reworked for Aerondel. Press J to open and close this window."));
	M.Lines.Add(FString::Printf(TEXT("Your Ventures bonuses: +%s damage, +%s accuracy, %s less damage taken, +%s XP."),
		*Pct(DamageDealtBonus(nullptr)), *Pct(AccuracyBonus(nullptr)), *Pct(FMath::Clamp(SumDr(nullptr), 0.f, CapDr)), *Pct(XpBonus(TEXT("attack")))));
	struct FT { const TCHAR* Id; const TCHAR* Name; };
	static const FT T[] = { { TEXT("flare"), TEXT("Overflare (U)") }, { TEXT("wardrobe"), TEXT("Wardrobe") }, { TEXT("ledger"), TEXT("Wayfarer's Ledger") }, { TEXT("reliq"), TEXT("Hidden Reliquaries") } };
	for (const FT& X : T) { const FString Id = X.Id; VAct(M, X.Name, [this, Id] { GoTab(Id); }); }
	OverviewMore(M);
}

void UVenturesFeature::TabFlare(FGPanelModel& M)
{
	const FVentureUlt& U = Ult();
	M.Title = FString::Printf(TEXT("Overflare %d%%"), FMath::FloorToInt(Meter));
	M.Lines.Add(TEXT("Fills as you deal and take damage. At 100% press U to unleash your weapon's ultimate, then +30% damage for 6 seconds."));
	M.Lines.Add(FString::Printf(TEXT("With this weapon: %s - %d strikes at +%d%% damage%s."), U.Name, U.Hits, FMath::RoundToInt(U.Bonus * 100.f),
		U.Radius > 0.f ? *FString::Printf(TEXT(", hitting up to 6 foes within %d m"), FMath::RoundToInt(U.Radius)) : TEXT(" on your target")));
	M.Lines.Add(TEXT("Katana Thousand Cuts Barrage, scimitar Sandstorm Reel, glaive Crescent Tempest, war fan Gale Sovereign, chakram Chakram Storm, bows Starfall Deluge, magic Astral Cataclysm, other melee Titan's Overflare."));
	M.Lines.Add(FString::Printf(TEXT("Used %d times."), FlareUses));
	VAct(M, TEXT("Unleash (U)"), [this] { Flare(); }, Meter >= 100.f);
}

void UVenturesFeature::TabWardrobe(FGPanelModel& M)
{
	M.Title = TEXT("Kitbash Wardrobe");
	M.Lines.Add(TEXT("Show any armour you have owned or worn over your real gear, and dye each slot. Stats always come from your real gear."));
	M.Lines.Add(TEXT("The body dye tints your whole figure; other slot dyes are kept for outfits."));
	UStableCodexFeature* Cx = Feature<UStableCodexFeature>();
	for (const TCHAR* Slot : LookSlots)
	{
		const FString S = Slot;
		const FString Cur = Cx ? Cx->GetLook(S) : FString();
		const FString* D = Dyes.Find(S);
		M.Lines.Add(FString::Printf(TEXT("%s: %s%s"), *S, Cur.IsEmpty() ? TEXT("your real gear") : *Cur, D ? *(TEXT(" - ") + *D) : TEXT("")));
		VAct(M, FString::Printf(TEXT("%s: next look"), *S), [this, S]
		{
			UStableCodexFeature* C = Feature<UStableCodexFeature>(); const UGameDatabase* Db = UGameDatabase::Get();
			if (!C || !Db) return;
			TArray<FString> List;
			for (const FString& I : C->GetSeen()) if (const UItemDef* Def = Db->FindItem(Id(I)))
			{
				const EEquipSlot E = Def->EquipSlot;
				const bool bOk = (S == TEXT("head") && E == EEquipSlot::Head) || (S == TEXT("body") && E == EEquipSlot::Body) || (S == TEXT("legs") && E == EEquipSlot::Legs) || (S == TEXT("shield") && E == EEquipSlot::Shield) || (S == TEXT("cape") && E == EEquipSlot::Cape);
				if (bOk) List.Add(I);
			}
			if (List.Num() == 0) { Say(TEXT("No looks unlocked for that slot yet.")); return; }
			const int32 Ix = List.IndexOfByKey(C->GetLook(S));
			C->SetLook(S, List[(Ix + 1) % List.Num()]);
		});
		VAct(M, FString::Printf(TEXT("%s: next dye"), *S), [this, S]
		{
			const FString* Now = Dyes.Find(S); int32 Ix = -1;
			for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(VenturesData::Dyes); ++i) if (Now && *Now == VenturesData::Dyes[i].Name) Ix = i;
			if (Ix + 1 >= (int32)UE_ARRAY_COUNT(VenturesData::Dyes)) Dyes.Remove(S); else Dyes.Add(S, VenturesData::Dyes[Ix + 1].Name);
		});
		VAct(M, FString::Printf(TEXT("%s: real gear, no dye"), *S), [this, S] { if (UStableCodexFeature* C = Feature<UStableCodexFeature>()) C->SetLook(S, FString()); Dyes.Remove(S); });
	}
	for (int32 i = 0; i < 3; ++i)
	{
		M.Lines.Add(FString::Printf(TEXT("Outfit %d: %s"), i + 1, Outfits[i].bUsed ? *FString::Printf(TEXT("%d looks, %d dyes"), Outfits[i].Looks.Num(), Outfits[i].Dyes.Num()) : TEXT("empty")));
		VAct(M, FString::Printf(TEXT("Save outfit %d"), i + 1), [this, i]
		{
			FVentureOutfit O; O.bUsed = true; O.Dyes = Dyes;
			if (UStableCodexFeature* C = Feature<UStableCodexFeature>()) for (const TCHAR* S : LookSlots) { const FString L = C->GetLook(S); if (!L.IsEmpty()) O.Looks.Add(S, L); }
			Outfits[i] = O; Say(FString::Printf(TEXT("Outfit %d saved."), i + 1));
		});
		VAct(M, FString::Printf(TEXT("Wear outfit %d"), i + 1), [this, i] { WardrobeApply(Outfits[i]); Say(FString::Printf(TEXT("You change into outfit %d."), i + 1)); }, Outfits[i].bUsed);
	}
}

void UVenturesFeature::TabLedger(FGPanelModel& M)
{
	const int32 N = LedgerCount(), T = LedgerTier();
	M.Title = FString::Printf(TEXT("Wayfarer's Ledger %d/%d"), N, LedgerAll);
	M.Lines.Add(FString::Printf(TEXT("Milestone %d: +%d%% damage, %d%% less damage taken, +%d%% XP. Next at %d entries."), T, T, T, T * 2, (T + 1) * 5));
	auto List = [&M](const TCHAR* Title, const TArray<FString>& Got, int32 All)
	{ M.Lines.Add(FString::Printf(TEXT("%s %d/%d: %s"), Title, Got.Num(), All, Got.Num() ? *FString::Join(Got, TEXT(", ")) : TEXT("none yet"))); };
	List(TEXT("Boss souls"), Souls, (int32)UE_ARRAY_COUNT(VenturesData::Souls));
	List(TEXT("Rare drops"), Rares, (int32)UE_ARRAY_COUNT(VenturesData::Rares));
	List(TEXT("Lore stones"), Lore, NumLore);
	List(TEXT("Village trinkets"), Trinkets, (int32)UE_ARRAY_COUNT(Villages));
	FString Missing;
	for (int32 i = 0; i < NumLore; ++i) if (!Lore.Contains(FString::Printf(TEXT("lore%d"), i))) Missing += FString::Printf(TEXT("%s%s"), Missing.IsEmpty() ? TEXT("") : TEXT(", "), Towns[i].Name);
	if (!Missing.IsEmpty()) M.Lines.Add(TEXT("Lore stones still to read stand just south-east of: ") + Missing);
	M.Lines.Add(TEXT("Souls: frontier bosses, Captain Saltgrave and the Colossi. Trinkets: walk into Snarlwick, Gorrumhold and Sylvaranth."));
}

void UVenturesFeature::TabReliq(FGPanelModel& M)
{
	M.Title = TEXT("Hidden Reliquaries");
	if (Puz.bOn)
	{
		const int32 Left = FMath::Max(0, (int32)((Puz.EndMs - NowMs() + 999) / 1000));
		FString Seq;
		for (int32 i = 0; i < Puz.Seq.Num(); ++i) Seq += FString::Printf(TEXT("%s%s%s"), i ? TEXT(" ") : TEXT(""), i < Puz.At ? TEXT("~") : TEXT(""), Runes[Puz.Seq[i]]);
		M.Title = FString::Printf(TEXT("Seal (tier %d) - %ds"), Puz.Site.Tier, Left);
		M.Lines.Add(Seq);
		M.Lines.Add(TEXT("Press the runes in order before time runs out."));
		for (int32 r = 0; r < 4; ++r) VAct(M, Runes[r], [this, r] { RelicPress(r); });
		return;
	}
	if (RelicDay != TodayUtc()) { RelicDay = TodayUtc(); RelicDug.Reset(); }
	M.Lines.Add(FString::Printf(TEXT("Dug today %d/16 - all-time %d"), RelicDug.Num(), RelicTotal));
	FVentureRelic S; float D = 0.f;
	if (!Owns(TEXT("Dowsing rod"))) M.Lines.Add(TEXT("You need a Dowsing rod (Crafting 10, or Wayfinder Orla in Larkhaven for 150 coins)."));
	else if (RelicNear(S, D)) M.Lines.Add(FString::Printf(TEXT("Nearest cache: %d paces away (tier %d)."), FMath::RoundToInt(D), S.Tier));
	else M.Lines.Add(TEXT("All caches opened today."));
	M.Lines.Add(TEXT("Higher Thieving gives more time and coins. 16 caches near the towns, reset daily."));
	VAct(M, TEXT("Dowse / Dig"), [this] { RelicDig(); });
}
