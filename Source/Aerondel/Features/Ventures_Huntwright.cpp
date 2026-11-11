#include "Features/VenturesFeature.h"
#include "UI/AerondelPanels.h"
#include "Core/AerondelPlayerState.h"
#include "Combat/AerondelMob.h"
#include "Data/MobDef.h"
#include "Data/ItemDef.h"
#include "Inventory/EquipmentComponent.h"
#include "Rules/AerondelRules.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// ============================================================================================
// Ventures group 7 (web adapt/huntwright.js): named mid-fight part breaks, a carve after the kill,
// a short track to a hunt, and Hunter-gated gear crafted from those parts. Crestbreak stays the stagger engine.
// Introduces the Hunter skill (starts at 1, caps at 1000, not in combat level).
// ============================================================================================

namespace VenturesHunt
{
	struct FPartDef { const TCHAR* Id; const TCHAR* Item; const TCHAR* Label; };
	struct FHuntDef
	{
		const TCHAR* Boss;
		int32 Level = 0;
		int32 HunterReq = 0;
		const TCHAR* Set;
		const TCHAR* Weapon;
		FPartDef Parts[4];
		int32 NumParts = 0;
		float X = 0.f;
		float Z = 0.f;
		const TCHAR* SetFx; // carveHaste / partWindow / trackHaste (panel text only; values below)
	};

	// First elite spawn (web REGION_DATA / COLOSSI route[0]), web metres.
	static const FHuntDef Hunts[] = {
		{ TEXT("Cinder Warden"),     140,   1, TEXT("Brandhide"),     TEXT("Brandhide cleaver"),     { { TEXT("horn"), TEXT("Cinder horn"), TEXT("cinder horn") }, { TEXT("plate"), TEXT("Ash plate"), TEXT("ash plate") }, { TEXT("tail"), TEXT("Ember tail"), TEXT("ember tail") } }, 3,  512.f,  -26.f, TEXT("carveHaste") },
		{ TEXT("Bramble Colossus"),  220,  20, TEXT("Briarhide"),     TEXT("Briarhide hook"),        { { TEXT("horn"), TEXT("Bramble horn"), TEXT("bramble horn") }, { TEXT("plate"), TEXT("Thorn plate"), TEXT("thorn plate") }, { TEXT("tail"), TEXT("Vine lash"), TEXT("vine lash") } }, 3,  352.f,  374.f, TEXT("carveHaste") },
		{ TEXT("Glacier Behemoth"),  320,  40, TEXT("Rimehide"),      TEXT("Rimehide pike"),         { { TEXT("horn"), TEXT("Rime horn"), TEXT("rime horn") }, { TEXT("plate"), TEXT("Frost plate"), TEXT("frost plate") }, { TEXT("tail"), TEXT("Ice lash"), TEXT("ice lash") } }, 3,  -28.f,  514.f, TEXT("partWindow") },
		{ TEXT("Peak Warlord"),      450,  60, TEXT("Stormhide"),     TEXT("Stormhide fan"),         { { TEXT("horn"), TEXT("Peak horn"), TEXT("peak horn") }, { TEXT("plate"), TEXT("Storm plate"), TEXT("storm plate") }, { TEXT("wing"), TEXT("Warlord plume"), TEXT("warlord plume") } }, 3, -418.f,  374.f, TEXT("trackHaste") },
		{ TEXT("Bog Revenant"),      600,  80, TEXT("Mirehide"),      TEXT("Mirehide disc"),         { { TEXT("horn"), TEXT("Bog crest"), TEXT("bog crest") }, { TEXT("plate"), TEXT("Mire plate"), TEXT("mire plate") }, { TEXT("tail"), TEXT("Revenant coil"), TEXT("revenant coil") } }, 3, -573.f,  -36.f, TEXT("carveHaste") },
		{ TEXT("Dune Tyrant"),       720, 120, TEXT("Dunewrought"),   TEXT("Dunewrought scimitar"),  { { TEXT("horn"), TEXT("Dune horn"), TEXT("dune horn") }, { TEXT("plate"), TEXT("Sand plate"), TEXT("sand plate") }, { TEXT("tail"), TEXT("Tyrant lash"), TEXT("tyrant lash") } }, 3, -438.f, -466.f, TEXT("partWindow") },
		{ TEXT("Molten Archon"),     880, 200, TEXT("Cinderwrought"), TEXT("Cinderwrought glaive"),  { { TEXT("horn"), TEXT("Archon horn"), TEXT("archon horn") }, { TEXT("plate"), TEXT("Magma plate"), TEXT("magma plate") }, { TEXT("core"), TEXT("Ember core"), TEXT("ember core") } }, 3,  -28.f, -571.f, TEXT("carveHaste") },
		{ TEXT("Voidborn Knight"),   950, 350, TEXT("Nightwrought"),  TEXT("Nightwrought katana"),   { { TEXT("horn"), TEXT("Void crest"), TEXT("void crest") }, { TEXT("plate"), TEXT("Night plate"), TEXT("night plate") }, { TEXT("core"), TEXT("Knight core"), TEXT("knight core") } }, 3,  446.f, -406.f, TEXT("partWindow") },
		{ TEXT("Sunderlord Vexmar"),1000, 500, TEXT("Eclipshide"),    TEXT("Eclipshide edge"),       { { TEXT("horn"), TEXT("Eclipse horn"), TEXT("eclipse horn") }, { TEXT("plate"), TEXT("Sunder plate"), TEXT("sunder plate") }, { TEXT("core"), TEXT("Last-light core"), TEXT("last-light core") }, { TEXT("tail"), TEXT("Eclipse lash"), TEXT("eclipse lash") } }, 4, 392.f, -456.f, TEXT("carveHaste") },
		{ TEXT("Stonehorn"),         140,  15, TEXT("Hillhide"),      TEXT("Hillhide maul-blade"),   { { TEXT("horn"), TEXT("Stone horn"), TEXT("stone horn") }, { TEXT("legs"), TEXT("Hill plate"), TEXT("hill plate") } }, 2,   96.f, -100.f, TEXT("partWindow") },
		{ TEXT("Frostmaw"),          150,  25, TEXT("Scarhide"),      TEXT("Scarhide fang"),         { { TEXT("horn"), TEXT("Frost horn"), TEXT("frost horn") }, { TEXT("legs"), TEXT("Scar plate"), TEXT("scar plate") } }, 2,  160.f, -110.f, TEXT("carveHaste") },
		{ TEXT("Ashwyrm"),           165,  30, TEXT("Cinderback"),    TEXT("Cinderback glaive"),     { { TEXT("horn"), TEXT("Wyrm horn"), TEXT("wyrm horn") }, { TEXT("legs"), TEXT("Ashwyrm scale"), TEXT("ashwyrm scale") }, { TEXT("tail"), TEXT("Cinder tail"), TEXT("cinder tail") } }, 3, 96.f, -150.f, TEXT("partWindow") },
	};

	static const FHuntDef* FindHunt(const FString& Boss)
	{
		for (const FHuntDef& H : Hunts) if (Boss == H.Boss) return &H;
		return nullptr;
	}

	static float SetCarveHaste(const FString& Set)
	{
		if (Set == TEXT("Eclipshide")) return 0.25f;
		if (Set == TEXT("Rimehide") || Set == TEXT("Dunewrought") || Set == TEXT("Nightwrought") || Set == TEXT("Hillhide") || Set == TEXT("Cinderback") || Set == TEXT("Stormhide")) return 0.f;
		if (Set.IsEmpty()) return 0.f;
		return 0.2f;
	}
	static int32 SetPartWindow(const FString& Set)
	{
		return (Set == TEXT("Rimehide") || Set == TEXT("Dunewrought") || Set == TEXT("Nightwrought") || Set == TEXT("Hillhide") || Set == TEXT("Cinderback") || Set == TEXT("Eclipshide") || Set == TEXT("Stormhide")) ? 1 : 0;
	}
	static float SetTrackHaste(const FString& Set) { return Set == TEXT("Stormhide") ? 0.15f : 0.f; }

	static void VAct7(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
}
using namespace VenturesHunt;

FString UVenturesFeature::HuntWornSet() const
{
	const AAerondelPlayerState* PS = GetPS();
	if (!PS || !PS->Equipment) return FString();
	const UItemDef* Head = PS->Equipment->GetEquipped(EEquipSlot::Head);
	const UItemDef* Body = PS->Equipment->GetEquipped(EEquipSlot::Body);
	const UItemDef* Legs = PS->Equipment->GetEquipped(EEquipSlot::Legs);
	if (!Head || !Body || !Legs) return FString();
	const FName A = Head->SetName, B = Body->SetName, C = Legs->SetName;
	if (A.IsNone() || A != B || A != C) return FString();
	for (const FHuntDef& H : Hunts) if (A == FName(H.Set)) return H.Set;
	return FString();
}

void UVenturesFeature::ResetHunt()
{
	HuntCarved.Reset();
	HuntUnlocked.Reset();
	HuntPartsBroken = 0;
	HuntTracks = 0;
	HuntLive.Reset();
	HuntCarveJob = FHuntCarveJob();
	HuntTrackJob = FHuntTrackJob();
}

void UVenturesFeature::SaveHunt(FJsonObject& A) const
{
	TSharedRef<FJsonObject> H = MakeShared<FJsonObject>();
	PutIntMap(*H, TEXT("carved"), HuntCarved);
	PutStrArr(*H, TEXT("unlocked"), HuntUnlocked);
	H->SetNumberField(TEXT("partsBroken"), HuntPartsBroken);
	H->SetNumberField(TEXT("tracks"), HuntTracks);
	A.SetObjectField(TEXT("hunt"), H);
}

void UVenturesFeature::LoadHunt(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> H = JObj(A, TEXT("hunt"));
	if (!H) return;
	HuntCarved = JIntMap(*H, TEXT("carved"));
	HuntUnlocked = JStrArr(*H, TEXT("unlocked"));
	if (HuntUnlocked.Num() > 80) HuntUnlocked.SetNum(80);
	HuntPartsBroken = FMath::Max(0, (int32)JInt(*H, TEXT("partsBroken")));
	HuntTracks = FMath::Max(0, (int32)JInt(*H, TEXT("tracks")));
}

int32 UVenturesFeature::HitHunt(AAerondelMob* Mob, int32 D)
{
	if (D <= 0 || !Mob || !IsBossy(Mob)) return D;
	const FString Name = Mob->Definition ? Mob->Definition->DisplayName.ToString() : FString();
	const FHuntDef* Def = FindHunt(Name);
	FHuntLive* Live = HuntLive.Find(Mob);
	if (!Live)
	{
		FHuntLive N;
		const int32 MaxHp = FMath::Max(1, Mob->GetMaxHp());
		if (Def)
		{
			const float Share = 0.22f + (Def->NumParts > 3 ? 0.04f : 0.f);
			for (int32 i = 0; i < Def->NumParts; ++i)
			{
				FHuntPart P;
				P.Id = Def->Parts[i].Id; P.Item = Def->Parts[i].Item; P.Label = Def->Parts[i].Label;
				P.Hp = P.Max = FMath::Max(8, FMath::RoundToInt(MaxHp * Share));
				N.Parts.Add(P);
			}
		}
		else
		{
			for (const TCHAR* Id : { TEXT("crest"), TEXT("core") })
			{
				FHuntPart P; P.Id = Id; P.Item = TEXT("Hunt remnant"); P.Label = Id;
				P.Hp = P.Max = FMath::Max(8, FMath::RoundToInt(MaxHp * 0.18f));
				N.Parts.Add(P);
			}
		}
		Live = &HuntLive.Add(Mob, N);
	}
	TArray<FHuntPart*> Remaining;
	for (FHuntPart& P : Live->Parts) if (P.Hp > 0) Remaining.Add(&P);
	if (Remaining.Num() == 0) return D;
	FHuntPart* Target = Remaining[0];
	FVector2D Pos;
	if (PlayerWebPos(Pos))
	{
		const FVector MLoc = Mob->GetActorLocation();
		const FVector2D MobWeb(MLoc.X / 100.0, MLoc.Y / 100.0);
		const FVector Fwd = Mob->GetActorForwardVector();
		const FVector2D To = (Pos - MobWeb).GetSafeNormal();
		const float Behind = To.X * (float)(Fwd.X) + To.Y * (float)(Fwd.Y);
		if (Behind < -0.35f)
		{
			for (FHuntPart* P : Remaining) if (P->Id == TEXT("tail")) { Target = P; break; }
			if (Target == Remaining[0] && Remaining.Num() > 1) Target = Remaining.Last();
		}
	}
	Target->Hp -= D;
	if (Target->Hp > 0) return D;
	Target->Hp = 0;
	if (Live->Broke.Contains(Target->Id)) return D;
	Live->Broke.Add(Target->Id);
	HuntPartsBroken++;
	GiveItem(Target->Item, 1);
	const AAerondelPlayerState* PS = GetPS();
	const float Yield = 1.f + UAerondelRules::Perk(PS, TEXT("hunter"), TEXT("carveYield")) + SetCarveHaste(HuntWornSet()) * 0.25f;
	const int32 Xp = FMath::RoundToInt((20.f + (Mob->Definition ? Mob->Definition->CombatLevel : 50) * 1.2f) * Yield);
	GiveXp(TEXT("Hunter"), (float)Xp);
	Mob->Stun(2 + SetPartWindow(HuntWornSet()));
	Say(FString::Printf(TEXT("The %s's %s breaks! You take a %s. (+%d Hunter)"), *Name, *Target->Label, *Target->Item, Xp));
	return D;
}

void UVenturesFeature::DiedHunt(AAerondelMob* Mob, bool bByPlayer)
{
	if (!Mob || !bByPlayer) return;
	const FString Name = Mob->Definition ? Mob->Definition->DisplayName.ToString() : FString();
	if (!IsBossy(Mob) && !FindHunt(Name)) return;
	FHuntLive& Live = HuntLive.FindOrAdd(Mob);
	Live.CarveUntilMs = NowMs() + 16000 + LeyFx().CarveWin;
	Live.bCarved = false;
	Say(FString::Printf(TEXT("The %s can be carved. Open Ventures -> Huntwright, or stand close and carve."), *Name));
}

bool UVenturesFeature::HuntCarve(AAerondelMob* Mob)
{
	if (!Mob)
	{
		int64 Best = 0;
		for (auto& Pair : HuntLive)
		{
			if (!Pair.Key.IsValid() || Pair.Value.bCarved || Pair.Value.CarveUntilMs <= NowMs()) continue;
			FVector2D Pos;
			if (!PlayerWebPos(Pos)) continue;
			const FVector MLoc = Pair.Key->GetActorLocation();
			const float Dist = FVector2D::Distance(Pos, FVector2D(MLoc.X / 100.0, MLoc.Y / 100.0));
			if (Dist < 5.f && Pair.Value.CarveUntilMs > Best) { Best = Pair.Value.CarveUntilMs; Mob = Pair.Key.Get(); }
		}
	}
	if (!Mob) { Say(TEXT("There is nothing left to carve.")); return false; }
	FHuntLive* Live = HuntLive.Find(Mob);
	if (!Live || Live->bCarved || Live->CarveUntilMs <= NowMs()) { Say(TEXT("There is nothing left to carve.")); return false; }
	FVector2D Pos;
	if (!PlayerWebPos(Pos)) return false;
	const FVector MLoc = Mob->GetActorLocation();
	if (FVector2D::Distance(Pos, FVector2D(MLoc.X / 100.0, MLoc.Y / 100.0)) > 4.f) { Say(TEXT("Stand closer to carve.")); return false; }
	const AAerondelPlayerState* PS = GetPS();
	const float Haste = SetCarveHaste(HuntWornSet()) + UAerondelRules::Perk(PS, TEXT("hunter"), TEXT("carveYield")) * 0.5f;
	const int32 Ms = FMath::Max(1100, FMath::RoundToInt(3000.f / (1.f + Haste)));
	HuntCarveJob.Mob = Mob;
	HuntCarveJob.UntilMs = NowMs() + Ms;
	HuntCarveJob.Start = Pos;
	HuntCarveJob.bOn = true;
	const FString Name = Mob->Definition ? Mob->Definition->DisplayName.ToString() : TEXT("hunt");
	Say(FString::Printf(TEXT("You start carving the %s..."), *Name));
	return true;
}

bool UVenturesFeature::HuntTrack(const FString& Boss)
{
	const FHuntDef* Def = FindHunt(Boss);
	if (!Def) { Say(TEXT("No such hunt.")); return false; }
	if (SkillLevel(TEXT("Hunter")) < Def->HunterReq)
	{
		Say(FString::Printf(TEXT("You need Hunter %d to track a %s."), Def->HunterReq, Def->Boss));
		return false;
	}
	FVector2D From;
	if (!PlayerWebPos(From)) return false;
	const FVector2D Dest(Def->X, Def->Z);
	const AAerondelPlayerState* PS = GetPS();
	const float Mul = 1.f + UAerondelRules::Perk(PS, TEXT("hunter"), TEXT("trackSpeed")) + SetTrackHaste(HuntWornSet());
	const int32 Steps = FMath::Max(3, FMath::RoundToInt(8.f / Mul));
	HuntTrackJob = FHuntTrackJob();
	HuntTrackJob.Boss = Boss;
	HuntTrackJob.At = 0;
	HuntTrackJob.bOn = true;
	for (int32 i = 1; i <= Steps; ++i)
	{
		const float F = i / (float)Steps;
		HuntTrackJob.Marks.Add(FMath::Lerp(From, Dest, F));
	}
	Say(FString::Printf(TEXT("You pick up a %s trail. Follow the spoors (Ventures -> Huntwright)."), Def->Boss));
	return true;
}

bool UVenturesFeature::HuntCraft(const FString& Boss, const FString& Kind)
{
	const FHuntDef* Def = FindHunt(Boss);
	if (!Def) return false;
	if (SkillLevel(TEXT("Hunter")) < Def->HunterReq)
	{
		Say(FString::Printf(TEXT("You need Hunter %d to craft %s gear."), Def->HunterReq, Def->Set));
		return false;
	}
	const FString Item = Kind == TEXT("weapon") ? FString(Def->Weapon) : FString::Printf(TEXT("%s %s"), Def->Set, *Kind);
	TMap<FString, int32> Need;
	Need.Add(Def->Parts[0].Item, 2);
	if (Def->NumParts > 1) Need.Add(Def->Parts[1].Item, 1);
	if (Def->Level >= 400 && Def->NumParts > 2) Need.Add(Def->Parts[2].Item, 1);
	if (Def->Level >= 450) Need.FindOrAdd(TEXT("Iron bar")) += 2;
	for (const auto& P : Need) if (CountItem(P.Key) < P.Value)
	{
		FString List;
		for (const auto& Q : Need) { if (!List.IsEmpty()) List += TEXT(", "); List += FString::Printf(TEXT("%d %s"), Q.Value, *Q.Key); }
		Say(FString::Printf(TEXT("You need %s."), *List));
		return false;
	}
	for (const auto& P : Need) TakeItem(P.Key, P.Value);
	GiveItem(Item, 1);
	HuntUnlocked.AddUnique(Item);
	GiveXp(TEXT("Hunter"), 80.f + Def->Level * 0.4f);
	if (Need.Contains(TEXT("Iron bar"))) GiveXp(TEXT("Smithing"), 40.f);
	Say(FString::Printf(TEXT("You craft a %s."), *Item));
	return true;
}

void UVenturesFeature::TickHunt()
{
	if (HuntCarveJob.bOn)
	{
		if (!HuntCarveJob.Mob.IsValid()) { HuntCarveJob = FHuntCarveJob(); }
		else
		{
			FVector2D Pos;
			if (!PlayerWebPos(Pos) || FVector2D::Distance(Pos, HuntCarveJob.Start) > 0.8f)
			{
				Say(TEXT("You move and stop carving."));
				HuntCarveJob = FHuntCarveJob();
			}
			else if (NowMs() >= HuntCarveJob.UntilMs)
			{
				AAerondelMob* Mob = HuntCarveJob.Mob.Get();
				HuntCarveJob = FHuntCarveJob();
				FHuntLive* Live = HuntLive.Find(Mob);
				if (Mob && Live && !Live->bCarved)
				{
					Live->bCarved = true;
					Live->CarveUntilMs = 0;
					const FString Name = Mob->Definition ? Mob->Definition->DisplayName.ToString() : FString();
					const FHuntDef* Def = FindHunt(Name);
					const AAerondelPlayerState* PS = GetPS();
					const float Yield = 1.f + UAerondelRules::Perk(PS, TEXT("hunter"), TEXT("carveYield")) + SetCarveHaste(HuntWornSet()) * 0.25f;
					const bool bRareCarve = LeyKey(TEXT("rarecarve"));
					const float Rare = UAerondelRules::Perk(PS, TEXT("hunter"), TEXT("rarePart")) * (bRareCarve ? 0.5f : 1.f);
					const int32 N = FMath::Max(1, FMath::RoundToInt((1.f + (Live->Broke.Num() ? Live->Broke.Num() * 0.35f : 0.f)) * Yield));
					TArray<FString> Got;
					if (Def)
					{
						for (int32 i = 0; i < N; ++i)
						{
							const bool bRare = FMath::FRand() < (0.15f + Rare);
							const FPartDef& Part = Def->Parts[bRare ? Def->NumParts - 1 : FMath::RandRange(0, Def->NumParts - 1)];
							GiveItem(Part.Item, 1);
							Got.Add(Part.Item);
						}
						if (bRareCarve && Def->NumParts > 0)
						{
							const FPartDef& Set = Def->Parts[0];
							bool bHas = false; for (const FString& G : Got) if (G == Set.Item) { bHas = true; break; }
							if (!bHas) { GiveItem(Set.Item, 1); Got.Add(Set.Item); }
						}
						if (FMath::FRand() < 0.35f) { GiveItem(TEXT("Hunt remnant"), 1); Got.Add(TEXT("Hunt remnant")); }
						HuntCarved.FindOrAdd(Name)++;
						ScoreOn(TEXT("hunt"));
					}
					else
					{
						GiveItem(TEXT("Hunt remnant"), N);
						Got.Add(FString::Printf(TEXT("%d Hunt remnant"), N));
					}
					const int32 Xp = FMath::RoundToInt((40.f + (Mob->Definition ? Mob->Definition->CombatLevel : 50) * 2.f) * Yield);
					GiveXp(TEXT("Hunter"), (float)Xp);
					Say(FString::Printf(TEXT("You carve the %s: %s. (+%d Hunter)"), *Name, *FString::Join(Got, TEXT(", ")), Xp));
				}
			}
		}
	}
	if (HuntTrackJob.bOn)
	{
		FVector2D Pos;
		if (PlayerWebPos(Pos) && HuntTrackJob.Marks.IsValidIndex(HuntTrackJob.At))
		{
			const FVector2D& Mark = HuntTrackJob.Marks[HuntTrackJob.At];
			if (FVector2D::Distance(Pos, Mark) < 2.6f)
			{
				HuntTrackJob.At++;
				if (HuntTrackJob.At >= HuntTrackJob.Marks.Num())
				{
					const FHuntDef* Def = FindHunt(HuntTrackJob.Boss);
					const AAerondelPlayerState* PS = GetPS();
					const float Mul = 1.f + UAerondelRules::Perk(PS, TEXT("hunter"), TEXT("trackSpeed")) + SetTrackHaste(HuntWornSet());
					const int32 Xp = FMath::RoundToInt((30.f + (Def ? Def->Level : 50) * 0.8f) * Mul);
					GiveXp(TEXT("Hunter"), (float)Xp);
					HuntTracks++;
					Say(FString::Printf(TEXT("The %s trail ends here. (+%d Hunter)"), *HuntTrackJob.Boss, Xp));
					HuntTrackJob = FHuntTrackJob();
				}
				else Say(FString::Printf(TEXT("You follow the spoor (%d/%d)."), HuntTrackJob.At, HuntTrackJob.Marks.Num()));
			}
		}
	}
	// Drop stale carve windows.
	for (auto It = HuntLive.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid() && It->Value.CarveUntilMs && It->Value.CarveUntilMs < NowMs() - 60000) It.RemoveCurrent();
	}
}

void UVenturesFeature::TabHunt(FGPanelModel& M)
{
	M.Title = TEXT("Huntwright");
	const FString Set = HuntWornSet();
	M.Lines.Add(FString::Printf(TEXT("Hunter %d · parts broken %d · trails %d"), SkillLevel(TEXT("Hunter")), HuntPartsBroken, HuntTracks));
	if (!Set.IsEmpty()) M.Lines.Add(FString::Printf(TEXT("Wearing %s (carve or break window only — no raw damage)."), *Set));
	if (HuntCarveJob.bOn) M.Lines.Add(TEXT("You are carving. Stay still."));
	else
	{
		bool bCan = false;
		for (const auto& Pair : HuntLive) if (Pair.Key.IsValid() && !Pair.Value.bCarved && Pair.Value.CarveUntilMs > NowMs()) bCan = true;
		if (bCan) VAct7(M, TEXT("Carve the fallen hunt"), [this] { HuntCarve(nullptr); }, true, true);
	}
	if (HuntTrackJob.bOn) M.Lines.Add(FString::Printf(TEXT("Tracking %s: spoor %d / %d."), *HuntTrackJob.Boss, HuntTrackJob.At, HuntTrackJob.Marks.Num()));
	M.Lines.Add(TEXT("Break named parts mid-fight, carve the body, then follow a trail. Sets speed carving or hold a break a little longer. Crestbreak still handles stagger."));
	for (const FHuntDef& H : Hunts)
	{
		const int32 Carved = HuntCarved.FindRef(H.Boss);
		const bool bLock = SkillLevel(TEXT("Hunter")) < H.HunterReq;
		M.Lines.Add(FString::Printf(TEXT("%s — Lv %d · Hunter %d%s · carved %d"), H.Boss, H.Level, H.HunterReq, bLock ? TEXT(" (locked)") : TEXT(""), Carved));
		FString Have;
		for (int32 i = 0; i < H.NumParts; ++i)
		{
			if (!Have.IsEmpty()) Have += TEXT(", ");
			Have += FString::Printf(TEXT("%s x%d"), H.Parts[i].Item, CountItem(H.Parts[i].Item));
		}
		M.Lines.Add(Have);
		const FString Boss = H.Boss;
		VAct7(M, FString::Printf(TEXT("Track %s"), H.Boss), [this, Boss] { HuntTrack(Boss); }, !bLock, true);
		VAct7(M, FString::Printf(TEXT("Craft %s"), H.Weapon), [this, Boss] { HuntCraft(Boss, TEXT("weapon")); }, !bLock);
		VAct7(M, FString::Printf(TEXT("Craft %s helm"), H.Set), [this, Boss] { HuntCraft(Boss, TEXT("helm")); }, !bLock);
		VAct7(M, FString::Printf(TEXT("Craft %s cuirass"), H.Set), [this, Boss] { HuntCraft(Boss, TEXT("cuirass")); }, !bLock);
		VAct7(M, FString::Printf(TEXT("Craft %s greaves"), H.Set), [this, Boss] { HuntCraft(Boss, TEXT("greaves")); }, !bLock);
	}
}
