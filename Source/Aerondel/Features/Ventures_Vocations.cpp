#include "Features/VenturesFeature.h"
#include "UI/AerondelPanels.h"
#include "Core/AerondelPlayerState.h"
#include "Core/PlayerStatsComponent.h"
#include "Combat/AerondelMob.h"
#include "Combat/HealthComponent.h"
#include "Inventory/EquipmentComponent.h"
#include "Data/ItemDef.h"
#include "Rules/AerondelRules.h"
#include "GameFramework/Pawn.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// Ventures group 10 (web adapt/vocations.js): a job kit on the same character.
// Path is your role, Stance is your discipline pair, Vocation is your job kit.
// Swap at Ventures, a bank, the Hearthstead lectern or Pathwarden Idris — not in a fight.

namespace VenturesVoc
{
	struct FVocDef { const TCHAR* Id; const TCHAR* Name; const TCHAR* Hint; const TCHAR* Desc; const TCHAR* Pass; const TCHAR* UnlockSkill; int32 UnlockLvl = 0; const TCHAR* Skills[3]; };
	static const FVocDef Vocs[] = {
		{ TEXT("warder"),     TEXT("Warder"),       TEXT("Bulwark"), TEXT("A heavy-blade job that holds the line."), TEXT("With a shield, light hits do not chip your poise, and Guard Stance blunts a little more."), nullptr, 0, { TEXT("vbrace"), TEXT("vshove"), TEXT("vhold") } },
		{ TEXT("longbow"),    TEXT("Longbow"),      TEXT("Striker"), TEXT("A bow job of pins and footwork."), TEXT("Arrows that miss can drop at your feet to be picked up."), TEXT("Ranged"), 20, { TEXT("vpin"), TEXT("vhop"), TEXT("vretrieve") } },
		{ TEXT("leysage"),    TEXT("Leysage"),      TEXT("Mender"), TEXT("A mage job of focus and ley-winding."), TEXT("A clear mind: after a spell, you recover a little Prayer."), TEXT("Magic"), 20, { TEXT("vfocus"), TEXT("vslowpulse"), TEXT("vclarity") } },
		{ TEXT("hearthcant"), TEXT("Hearthcant"),   TEXT("Mender"), TEXT("A mender job of calm and recovery."), TEXT("Once a fight, dropping low on life mends a little of it."), TEXT("Prayer"), 20, { TEXT("vmend"), TEXT("vcalm"), TEXT("vsoothe") } },
		{ TEXT("swash"),      TEXT("Swashbuckler"), TEXT("Striker"), TEXT("A blade job of footwork and openings."), TEXT("A riposte window lasts a little longer."), TEXT("Swordsmanship"), 20, { TEXT("vstep"), TEXT("vflourish"), TEXT("vriposte") } },
		{ TEXT("huntward"),   TEXT("Huntward"),     TEXT("Bulwark"), TEXT("A hunt job that likes Guardbreak."), TEXT("Your hits fill poise a little faster, and a fallen hunt stays carvable a few seconds longer."), TEXT("Hunter"), 20, { TEXT("vread"), TEXT("vbind"), TEXT("vtrail") } },
	};
	struct FVocSkill { const TCHAR* Id; const TCHAR* Voc; const TCHAR* Name; const TCHAR* Desc; int32 Cd = 20; bool bSelf = false; };
	static const FVocSkill Skills[] = {
		{ TEXT("vbrace"),     TEXT("warder"),     TEXT("Guard Stance"),  TEXT("Blunt incoming blows for a few seconds (1 less damage per hit)."), 30, true },
		{ TEXT("vshove"),     TEXT("warder"),     TEXT("Shield Shove"),  TEXT("Shove the foe back a step. No extra damage."), 22, false },
		{ TEXT("vhold"),      TEXT("warder"),     TEXT("Iron Hold"),     TEXT("Your poise will not break for a short time."), 50, true },
		{ TEXT("vpin"),       TEXT("longbow"),    TEXT("Pinning Shot"),  TEXT("A shot that roots the foe briefly."), 24, false },
		{ TEXT("vhop"),       TEXT("longbow"),    TEXT("Backstep"),      TEXT("Hop back a few paces."), 20, true },
		{ TEXT("vretrieve"),  TEXT("longbow"),    TEXT("Retrieve"),      TEXT("Your next missed shot returns the arrow to you."), 40, true },
		{ TEXT("vfocus"),     TEXT("leysage"),    TEXT("Ley Focus"),     TEXT("Your next spell does not spend runes."), 28, true },
		{ TEXT("vslowpulse"), TEXT("leysage"),    TEXT("Slow Pulse"),    TEXT("A pulse that chills and slows the target."), 26, false },
		{ TEXT("vclarity"),   TEXT("leysage"),    TEXT("Clarity"),       TEXT("Restore a little Prayer."), 45, true },
		{ TEXT("vmend"),      TEXT("hearthcant"), TEXT("Soft Mend"),     TEXT("Heal a little of your life (8%)."), 28, true },
		{ TEXT("vcalm"),      TEXT("hearthcant"), TEXT("Calm"),          TEXT("Clear a stun on yourself."), 30, true },
		{ TEXT("vsoothe"),    TEXT("hearthcant"), TEXT("Soothe"),        TEXT("Regenerate a little life over a few seconds."), 50, true },
		{ TEXT("vstep"),      TEXT("swash"),      TEXT("Sidestep"),      TEXT("A short sidestep out of a blow."), 18, true },
		{ TEXT("vflourish"),  TEXT("swash"),      TEXT("Flourish"),      TEXT("A flourish that fills the foe's poise bar (no extra damage)."), 22, false },
		{ TEXT("vriposte"),   TEXT("swash"),      TEXT("Ready Riposte"), TEXT("The next Guardbreak opening lasts longer."), 40, true },
		{ TEXT("vread"),      TEXT("huntward"),   TEXT("Read the Hide"), TEXT("This fight, your hits fill poise faster."), 24, false },
		{ TEXT("vbind"),      TEXT("huntward"),   TEXT("Hunter's Bind"), TEXT("A short bind that roots the foe."), 28, false },
		{ TEXT("vtrail"),     TEXT("huntward"),   TEXT("Linger"),        TEXT("A nearby fallen hunt stays carvable a little longer."), 40, true },
	};
	static const FVocDef* FindVoc(const FString& Id) { for (const FVocDef& V : Vocs) if (Id == V.Id) return &V; return nullptr; }
	static const FVocSkill* FindSkill(const FString& Id) { for (const FVocSkill& S : Skills) if (Id == S.Id) return &S; return nullptr; }
	static void VAct10(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
}
using namespace VenturesVoc;

void UVenturesFeature::ResetVoc()
{
	VocId.Reset();
	VocUnlocked.Reset(); VocUnlocked.Add(TEXT("warder"));
	VocBars.Reset();
	for (const FVocDef& V : Vocs) { TArray<FString> B; for (int32 i = 0; i < 3; ++i) B.Add(V.Skills[i]); VocBars.Add(V.Id, B); }
	VocFightLeft = 0; VocBraceUntil = VocHoldUntil = VocSootheUntil = VocReadUntil = 0;
	bVocRetrieve = bVocFocus = bVocRiposteArm = bVocCantProc = false; VocCd.Reset(); VocReadMob.Reset();
}

void UVenturesFeature::SaveVoc(FJsonObject& A) const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	S->SetStringField(TEXT("id"), VocId); PutStrArr(*S, TEXT("unlocked"), VocUnlocked);
	TSharedPtr<FJsonObject> Bars = MakeShared<FJsonObject>();
	for (const auto& E : VocBars) PutStrArr(*Bars, *E.Key, E.Value);
	S->SetObjectField(TEXT("bars"), Bars); A.SetObjectField(TEXT("voc"), S);
}

void UVenturesFeature::LoadVoc(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> S = JObj(A, TEXT("voc"));
	if (!S) return;
	for (const FString& Id : JStrArr(*S, TEXT("unlocked"))) if (FindVoc(Id) && VocUnlocked.Num() < 8) VocUnlocked.AddUnique(Id);
	if (!VocUnlocked.Contains(TEXT("warder"))) VocUnlocked.Insert(TEXT("warder"), 0);
	const FString Id = JStr(*S, TEXT("id"));
	if (FindVoc(Id) && VocUnlocked.Contains(Id)) VocId = Id;
	if (TSharedPtr<FJsonObject> Bars = JObj(*S, TEXT("bars")))
		for (const auto& E : Bars->Values)
			if (FindVoc(FString(*E.Key)) && E.Value.IsValid() && E.Value->Type == EJson::Array)
			{
				TArray<FString> B;
				for (const TSharedPtr<FJsonValue>& V : E.Value->AsArray()) if (V.IsValid() && V->Type == EJson::String && FindSkill(V->AsString()) && B.Num() < 8) B.Add(V->AsString());
				if (B.Num()) VocBars.Add(FString(*E.Key), B);
			}
}

bool UVenturesFeature::VocInCombat() const
{
	if (VocFightLeft > 0) return true;
	if (const AAerondelMob* T = CurrentTarget()) if (!T->IsDead()) return true;
	return false;
}
bool UVenturesFeature::VocCan(const FString& Id) const
{
	const FVocDef* D = FindVoc(Id); if (!D) return false;
	return !D->UnlockSkill || SkillLevel(D->UnlockSkill) >= D->UnlockLvl;
}
bool UVenturesFeature::VocUnlock(const FString& Id)
{
	const FVocDef* D = FindVoc(Id);
	if (!D || VocUnlocked.Contains(Id)) return false;
	if (!VocCan(Id)) { Say(FString::Printf(TEXT("You need %s %d to take the %s job."), D->UnlockSkill, D->UnlockLvl, D->Name)); return false; }
	VocUnlocked.Add(Id); Say(FString::Printf(TEXT("You unlock the %s vocation."), D->Name)); return true;
}
bool UVenturesFeature::VocSwap(const FString& Id)
{
	const FVocDef* D = FindVoc(Id); if (!D) return false;
	if (VocInCombat()) { Say(TEXT("You cannot change vocation in a fight. Step out of combat first.")); return false; }
	if (!VocUnlocked.Contains(Id) && !VocUnlock(Id)) return false;
	if (!VocUnlocked.Contains(Id)) return false;
	VocId = Id;
	Say(FString::Printf(TEXT("You take up the %s vocation. Path is still your role; Stance is still your discipline pair."), D->Name));
	return true;
}
bool UVenturesFeature::VocClear()
{
	if (VocInCombat()) { Say(TEXT("You cannot change vocation in a fight.")); return false; }
	VocId.Reset(); Say(TEXT("You set aside your vocation. The usual bar returns.")); return true;
}
bool UVenturesFeature::VocHoldActive() const { return VocHoldUntil && TickNow() < VocHoldUntil; }
bool UVenturesFeature::ConsumeVocFocus() { if (!bVocFocus) return false; bVocFocus = false; return true; }
bool UVenturesFeature::ConsumeVocFocus(const UObject* WorldContext) { UVenturesFeature* V = Get(WorldContext); return V ? V->ConsumeVocFocus() : false; }
void UVenturesFeature::VocMarkFight() { VocFightLeft = 12; }
void UVenturesFeature::VocHop(float Back, float Right)
{
	AAerondelPlayerState* PS = GetPS(); APawn* P = PS ? PS->GetPawn() : nullptr; if (!P) return;
	P->SetActorLocation(P->GetActorLocation() + P->GetActorForwardVector() * (-220.f * Back) + P->GetActorRightVector() * (-220.f * Right), true);
}

bool UVenturesFeature::VocUse(const FString& SkillId)
{
	const FVocSkill* Sk = FindSkill(SkillId);
	if (!Sk) return false;
	if (VocId != Sk->Voc) { Say(TEXT("That technique belongs to another vocation.")); return false; }
	if (const int32* Until = VocCd.Find(SkillId)) if (TickNow() < *Until) { Say(FString::Printf(TEXT("%s is still cooling."), Sk->Name)); return false; }
	AAerondelPlayerState* PS = GetPS(); AAerondelMob* T = CurrentTarget();
	if (SkillId == TEXT("vbrace")) { VocBraceUntil = TickNow() + 8; Say(TEXT("Guard Stance: incoming blows are blunted.")); }
	else if (SkillId == TEXT("vhold")) { VocHoldUntil = TickNow() + 6; PlayerPoiseCd = 0; Say(TEXT("Iron Hold: your poise will not break.")); }
	else if (SkillId == TEXT("vhop")) { VocHop(1.f, 0.f); Say(TEXT("You hop back.")); }
	else if (SkillId == TEXT("vretrieve")) { bVocRetrieve = true; Say(TEXT("You watch the next shot to recover it if it misses.")); }
	else if (SkillId == TEXT("vfocus")) { bVocFocus = true; Say(TEXT("Ley Focus: your next spell spends no runes.")); }
	else if (SkillId == TEXT("vclarity"))
	{
		if (!PS || !PS->Stats) return false;
		PS->Stats->PrayerPoints = FMath::Min((float)UAerondelRules::MaxPrayerPoints(PS), PS->Stats->PrayerPoints + 6.f);
		Say(TEXT("Clarity: +6 Prayer points."));
	}
	else if (SkillId == TEXT("vmend"))
	{
		if (!PS || !PS->Health) return false;
		const int32 H = FMath::Max(1, FMath::RoundToInt(PS->Health->GetMax() * 0.08f));
		PS->Health->Heal(H); Say(FString::Printf(TEXT("Soft Mend: +%d hitpoints."), H));
	}
	else if (SkillId == TEXT("vcalm")) { if (PS && PS->Stats) PS->Stats->StunUntil = 0; Say(TEXT("Calm: you shake off the stun.")); }
	else if (SkillId == TEXT("vsoothe")) { VocSootheUntil = TickNow() + 8; Say(TEXT("Soothe: you mend a little each tick.")); }
	else if (SkillId == TEXT("vstep")) { VocHop(-0.7f, 1.f); Say(TEXT("You sidestep.")); }
	else if (SkillId == TEXT("vriposte")) { bVocRiposteArm = true; Say(TEXT("The next Guardbreak opening will last longer.")); }
	else if (SkillId == TEXT("vtrail"))
	{
		bool bAny = false;
		for (auto& E : HuntLive)
		{
			AAerondelMob* M = E.Key.Get();
			if (!M || !M->IsDead() || E.Value.CarveUntilMs <= NowMs()) continue;
			E.Value.CarveUntilMs += 6000; Say(FString::Printf(TEXT("Linger: the %s can be carved a little longer."), *M->GetDisplayName().ToString()));
			bAny = true; break;
		}
		if (!bAny) { Say(TEXT("No fallen hunt is close enough to linger.")); return false; }
	}
	else if (SkillId == TEXT("vshove") || SkillId == TEXT("vpin") || SkillId == TEXT("vslowpulse") || SkillId == TEXT("vflourish") || SkillId == TEXT("vread") || SkillId == TEXT("vbind"))
	{
		if (!T || T->IsDead()) { Say(TEXT("You need a foe in reach.")); return false; }
		if (SkillId == TEXT("vshove"))
		{
			APawn* P = PS ? PS->GetPawn() : nullptr;
			if (P) { FVector D = T->GetActorLocation() - P->GetActorLocation(); D.Z = 0.f; if (!D.Normalize() && P) D = P->GetActorForwardVector(); T->SetActorLocation(T->GetActorLocation() + D * 220.f, true); }
			Say(FString::Printf(TEXT("You shove the %s back."), *T->GetDisplayName().ToString()));
		}
		else if (SkillId == TEXT("vflourish"))
		{
			if (FGuardPoise* G = GuardPoise.Find(T)) G->Poise = FMath::Min((float)GuardPoiseMax(T), G->Poise + 28.f);
			Say(FString::Printf(TEXT("You flourish at the %s."), *T->GetDisplayName().ToString()));
		}
		else if (SkillId == TEXT("vread")) { VocReadUntil = TickNow() + 20; VocReadMob = T; Say(TEXT("You read the hide — your hits will fill poise faster.")); }
		else { T->Stun(SkillId == TEXT("vslowpulse") ? 3 : 2); Say(FString::Printf(TEXT("The %s is bound."), *T->GetDisplayName().ToString())); }
	}
	else return false;
	VocCd.Add(SkillId, TickNow() + Sk->Cd); VocMarkFight(); return true;
}

int32 UVenturesFeature::VocIncoming(int32 Amount)
{
	if (Amount <= 0) return Amount;
	VocMarkFight();
	if (VocHoldActive()) PlayerPoiseCd = 0;
	if (VocId == TEXT("warder") && VocBraceUntil && TickNow() < VocBraceUntil) Amount = FMath::Max(0, Amount - 1);
	AAerondelPlayerState* PS = GetPS();
	if (VocId == TEXT("hearthcant") && !bVocCantProc && PS && PS->Health)
	{
		const int32 Max = PS->Health->GetMax(), Cur = PS->Health->GetCurrent();
		if (Max > 0 && Cur - Amount <= FMath::RoundToInt(Max * 0.3f))
		{
			bVocCantProc = true;
			const int32 H = FMath::Max(1, FMath::RoundToInt(Max * 0.06f));
			PS->Health->Heal(H); Say(FString::Printf(TEXT("Hearthcant: you catch your breath (+%d HP)."), H));
		}
	}
	return Amount;
}

int32 UVenturesFeature::HitVoc(AAerondelMob* Mob, int32 D)
{
	if (!Mob) return D;
	VocMarkFight();
	if (VocId == TEXT("leysage") && D > 0 && IsMagic())
		if (AAerondelPlayerState* PS = GetPS()) if (PS->Stats)
			PS->Stats->PrayerPoints = FMath::Min((float)UAerondelRules::MaxPrayerPoints(PS), PS->Stats->PrayerPoints + 1.f);
	if (bVocRetrieve && D == 0 && !IsMagic())
		if (const AAerondelPlayerState* PS = GetPS()) if (PS->Equipment)
			if (const UItemDef* Ammo = PS->Equipment->GetEquipped(EEquipSlot::Ammo))
			{ GiveItem(Ammo->DisplayName.ToString(), 1); bVocRetrieve = false; Say(TEXT("You recover the missed shot.")); }
	if (VocId == TEXT("huntward") && D > 0)
		if (FGuardPoise* G = GuardPoise.Find(Mob))
		{
			const int32 Extra = 2 + ((VocReadUntil && TickNow() < VocReadUntil && VocReadMob.Get() == Mob) ? 4 : 0);
			G->Poise = FMath::Min((float)GuardPoiseMax(Mob), G->Poise + Extra);
		}
	return D;
}

void UVenturesFeature::DiedVoc(AAerondelMob* Mob, bool bByPlayer)
{
	if (!bByPlayer || !Mob || VocId != TEXT("huntward")) return;
	if (FHuntLive* H = HuntLive.Find(Mob)) if (H->CarveUntilMs > NowMs()) H->CarveUntilMs += 2000;
}

void UVenturesFeature::TickVoc()
{
	if (VocFightLeft > 0 && --VocFightLeft == 0) bVocCantProc = false;
	if (VocSootheUntil && TickNow() < VocSootheUntil)
		if (AAerondelPlayerState* PS = GetPS()) if (PS->Health && PS->Health->GetCurrent() > 0 && PS->Health->GetCurrent() < PS->Health->GetMax())
			PS->Health->Heal(1);
	if (VocId == TEXT("swash") || bVocRiposteArm)
		for (auto& E : GuardPoise)
		{
			AAerondelMob* M = E.Key.Get();
			if (!M || M->IsDead() || E.Value.bUsed || !GuardRiposteOpen(M)) continue;
			const int32 Extra = (VocId == TEXT("swash") ? 1 : 0) + (bVocRiposteArm ? 2 : 0);
			if (Extra > 0 && E.Value.RiposteUntil > TickNow() && (E.Value.RiposteUntil - TickNow()) <= 6)
			{ E.Value.RiposteUntil += Extra; if (bVocRiposteArm) bVocRiposteArm = false; }
		}
}

void UVenturesFeature::TabVoc(FGPanelModel& M)
{
	const FVocDef* Cur = FindVoc(VocId);
	M.Title = Cur ? FString::Printf(TEXT("Vocation: %s"), Cur->Name) : FString(TEXT("Vocations"));
	M.Lines.Add(TEXT("Path is your role, Stance is your discipline pair, Vocation is your job kit. Each job has its own techniques (tools and identity, not a big damage boost)."));
	M.Lines.Add(TEXT("Swap here, at a bank, the Hearthstead lectern or Pathwarden Idris — not in a fight. Warder starts unlocked; the others need level 20 in a related skill."));
	if (VocInCombat()) M.Lines.Add(TEXT("You are in a fight — vocation swapping is locked."));
	for (const FVocDef& D : Vocs)
	{
		const bool bHave = VocUnlocked.Contains(D.Id), bCan = bHave || VocCan(D.Id), bOn = VocId == D.Id;
		const FString Need = D.UnlockSkill ? FString::Printf(TEXT("%s %d"), D.UnlockSkill, D.UnlockLvl) : FString(TEXT("free"));
		M.Lines.Add(FString::Printf(TEXT("%s — %s. Likes the %s Path. Passive: %s"), D.Name,
			bOn ? TEXT("active") : bHave ? TEXT("unlocked") : bCan ? *FString::Printf(TEXT("can unlock (%s)"), *Need) : *FString::Printf(TEXT("locked (%s)"), *Need), D.Hint, D.Pass));
		const FString Id = D.Id;
		if (bOn) VAct10(M, FString::Printf(TEXT("%s (active)"), D.Name), [] {});
		else if (bHave) VAct10(M, FString::Printf(TEXT("Swap to %s"), D.Name), [this, Id] { VocSwap(Id); }, !VocInCombat());
		else VAct10(M, FString::Printf(TEXT("Unlock %s"), D.Name), [this, Id] { VocUnlock(Id); }, bCan);
	}
	if (!VocId.IsEmpty()) VAct10(M, TEXT("Clear vocation"), [this] { VocClear(); }, !VocInCombat());
	if (!VocId.IsEmpty())
	{
		M.Lines.Add(TEXT("Techniques:"));
		for (const FVocSkill& S : Skills)
		{
			if (VocId != S.Voc) continue;
			const bool bReady = !VocCd.Contains(S.Id) || TickNow() >= VocCd.FindChecked(S.Id);
			M.Lines.Add(FString::Printf(TEXT("%s — %s"), S.Name, S.Desc));
			const FString Id = S.Id;
			VAct10(M, bReady ? FString(S.Name) : FString::Printf(TEXT("%s (cooling)"), S.Name), [this, Id] { VocUse(Id); }, bReady);
		}
	}
}
