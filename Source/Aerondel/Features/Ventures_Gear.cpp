#include "Features/VenturesFeature.h"
#include "UI/AerondelPanels.h"
#include "Core/AerondelPlayerState.h"
#include "Combat/AerondelMob.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/EquipmentComponent.h"
#include "Data/GameDatabase.h"
#include "Data/AerondelId.h"
#include "Data/ItemDef.h"
#include "Data/MobDef.h"
#include "Dom/JsonValue.h"

// ============================================================================================
// Ventures group 4 (web adapt/geodes.js, ascent.js, lumen.js, lattice.js):
// Sealed Geodes, Ascent Rolls, Lumen Rise, Anima Lattice.
// ============================================================================================

namespace VenturesGear
{
	struct FGeode { const TCHAR* Name; const TCHAR* Boss; const TCHAR* Breach; const TCHAR* Relic; const TCHAR* Stat; float V; };
	static const FGeode Geodes[] = {
		{ TEXT("Ember geode"), TEXT("Cinder Warden"), nullptr, TEXT("Emberheart Relic"), TEXT("dmg"), 0.04f },
		{ TEXT("Rime geode"), TEXT("Peak Warlord"), nullptr, TEXT("Rimewall Relic"), TEXT("dr"), 0.04f },
		{ TEXT("Storm geode"), nullptr, TEXT("ward"), TEXT("Stormsight Relic"), TEXT("acc"), 0.05f },
		{ TEXT("Void geode"), nullptr, TEXT("nodes"), TEXT("Voidspring Relic"), TEXT("xp"), 0.06f },
	};
	static const TCHAR* GeodePartsAll[] = { TEXT("core"), TEXT("hilt"), TEXT("shell"), TEXT("sigil-plate") };
	static const FGeode* FindGeode(const FString& N) { for (const FGeode& G : Geodes) if (N == G.Name) return &G; return nullptr; }
	static bool IsPart(const FString& P) { for (const TCHAR* X : GeodePartsAll) if (P == X) return true; return false; }
	static const TCHAR* StatLabel(const FString& K)
	{
		return K == TEXT("dmg") ? TEXT("damage") : K == TEXT("dr") ? TEXT("less damage taken") : K == TEXT("acc") ? TEXT("accuracy") : K == TEXT("xp") ? TEXT("combat XP")
			: K == TEXT("flare") ? TEXT("Overflare gain") : TEXT("stagger");
	}

	struct FPerk { const TCHAR* Key; const TCHAR* Name; const TCHAR* Stat; float V; const TCHAR* Desc; };
	static const FPerk Perks[] = {
		{ TEXT("honed"), TEXT("Honed"), TEXT("dmg"), 0.03f, TEXT("+3% damage") },
		{ TEXT("steady"), TEXT("Steady"), TEXT("acc"), 0.04f, TEXT("+4% accuracy") },
		{ TEXT("warded"), TEXT("Warded"), TEXT("dr"), 0.03f, TEXT("3% less damage taken") },
		{ TEXT("hunter"), TEXT("Bosshunter"), TEXT("boss"), 0.05f, TEXT("+5% damage to bosses") },
		{ TEXT("scholar"), TEXT("Studious"), TEXT("xp"), 0.04f, TEXT("+4% combat XP") },
		{ TEXT("kindled"), TEXT("Kindled"), TEXT("flare"), 0.1f, TEXT("+10% Overflare gain") },
		{ TEXT("brutal"), TEXT("Brutal"), TEXT("stag"), 0.15f, TEXT("+15% stagger") },
	};
	static const FPerk* FindPerk(const FString& K) { for (const FPerk& P : Perks) if (K == P.Key) return &P; return nullptr; }
	static TArray<FString> RollPerks()
	{
		const int32 N = (int32)UE_ARRAY_COUNT(Perks);
		const int32 A = FMath::RandRange(0, N - 1);
		int32 B = FMath::RandRange(0, N - 2);
		if (B >= A) ++B;
		return { FString(Perks[A].Key), FString(Perks[B].Key) };
	}

	// 19-node hex board (q, r) with |q|,|r|,|q+r| <= 2, in web order.
	struct FHex { int32 Q; int32 R; };
	static TArray<FHex> MakeHex()
	{
		TArray<FHex> H;
		for (int32 Q = -2; Q <= 2; ++Q)
			for (int32 R = FMath::Max(-2, -Q - 2); R <= FMath::Min(2, -Q + 2); ++R) H.Add({ Q, R });
		return H;
	}
	static const TArray<FHex>& Hex() { static const TArray<FHex> H = MakeHex(); return H; }
	static const TCHAR* LatTypes[] = { TEXT("dmg"), TEXT("acc"), TEXT("dr"), TEXT("xp"), TEXT("flare"), TEXT("stag") };
	static float LatVal(const FString& K)
	{
		return K == TEXT("dmg") ? 0.015f : K == TEXT("acc") ? 0.02f : K == TEXT("dr") ? 0.012f : K == TEXT("xp") ? 0.02f : K == TEXT("flare") ? 0.05f : 0.06f;
	}
	static int32 Centre() { const TArray<FHex>& H = Hex(); for (int32 i = 0; i < H.Num(); ++i) if (!H[i].Q && !H[i].R) return i; return 0; }
	static FString NodeType(int32 i)
	{
		const TArray<FHex>& H = Hex();
		if (i == Centre()) return TEXT("dmg");
		const int32 Ix = ((i * 7 + H[i].Q * 3) % 6 + 6) % 6; // JS % keeps the sign; HEX q >= -2 so i*7 + q*3 >= 0 except i=0 (q=-2): -6 % 6 = 0
		return LatTypes[Ix];
	}
	static int32 Ring(int32 i) { const FHex& H = Hex()[i]; return FMath::Max3(FMath::Abs(H.Q), FMath::Abs(H.R), FMath::Abs(H.Q + H.R)); }
	static bool Adj(int32 a, int32 b)
	{
		const FHex& A = Hex()[a]; const FHex& B = Hex()[b];
		const int32 Dq = A.Q - B.Q, Dr = A.R - B.R;
		return FMath::Max3(FMath::Abs(Dq), FMath::Abs(Dr), FMath::Abs(Dq + Dr)) == 1;
	}
	static int32 NodeCost(int32 i) { return 3 + Ring(i) * 3; }
	static const TCHAR* LatStyles[] = { TEXT("Melee"), TEXT("Ranged"), TEXT("Magic"), TEXT("Swordsmanship") };
	static int32 WeekNow() { return (int32)FMath::FloorToDouble((FDateTime::UtcNow().ToUnixTimestamp() * 1000.0 / 86400000.0 + 3.0) / 7.0); }
}
using namespace VenturesGear;

static void VAct4(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true)
{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; M.Actions.Add(MoveTemp(A)); }

static FString GearName(const UItemDef* I) { return I ? I->DisplayName.ToString() : FString(); }

// ---------------------------------------------------------------- state

void UVenturesFeature::ResetGear()
{
	GeodeParts.Reset();
	RelicsForged.Reset();
	AscentRolls.Reset();
	AscentDis.Reset();
	AscentPat.Reset();
	AscentSel.Reset();
	LumenScore = 1000.f;
	LumenWeek = 0;
	LumenPinn = 0;
	LatticeBoards.Reset();
	LatticeSpent = 0;
	LatticeView.Reset();
}

void UVenturesFeature::SaveGear(FJsonObject& A) const
{
	TSharedRef<FJsonObject> G = MakeShared<FJsonObject>(), Parts = MakeShared<FJsonObject>();
	for (const auto& E : GeodeParts) PutStrArr(*Parts, *E.Key, E.Value);
	G->SetObjectField(TEXT("parts"), Parts);
	PutStrArr(*G, TEXT("relics"), RelicsForged);
	A.SetObjectField(TEXT("geodes"), G);

	TSharedRef<FJsonObject> As = MakeShared<FJsonObject>(), Rolls = MakeShared<FJsonObject>(), Dis = MakeShared<FJsonObject>();
	for (const auto& E : AscentRolls) PutStrArr(*Rolls, *E.Key, E.Value);
	for (const auto& E : AscentDis) Dis->SetNumberField(FString(*E.Key), E.Value);
	As->SetObjectField(TEXT("rolls"), Rolls);
	As->SetObjectField(TEXT("dis"), Dis);
	PutStrArr(*As, TEXT("pat"), AscentPat);
	A.SetObjectField(TEXT("ascent"), As);

	TSharedRef<FJsonObject> L = MakeShared<FJsonObject>();
	L->SetNumberField(TEXT("score"), LumenScore);
	L->SetNumberField(TEXT("week"), LumenWeek);
	L->SetNumberField(TEXT("pinn"), LumenPinn);
	A.SetObjectField(TEXT("lumen"), L);

	TSharedRef<FJsonObject> La = MakeShared<FJsonObject>(), Boards = MakeShared<FJsonObject>();
	for (const auto& E : LatticeBoards)
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (int32 i : E.Value) Arr.Add(MakeShared<FJsonValueNumber>(i));
		Boards->SetArrayField(FString(*E.Key), Arr);
	}
	La->SetObjectField(TEXT("boards"), Boards);
	La->SetNumberField(TEXT("spent"), LatticeSpent);
	A.SetObjectField(TEXT("lattice"), La);
}

void UVenturesFeature::LoadGear(const FJsonObject& A)
{
	if (const TSharedPtr<FJsonObject> G = JObj(A, TEXT("geodes")))
	{
		if (const TSharedPtr<FJsonObject> Parts = JObj(*G, TEXT("parts")))
			for (const FGeode& Ge : Geodes)
			{
				TArray<FString> Have;
				for (const FString& P : JStrArr(*Parts, Ge.Name)) if (IsPart(P)) Have.AddUnique(P);
				if (Have.Num()) GeodeParts.Add(Ge.Name, Have);
			}
		for (const FString& R : JStrArr(*G, TEXT("relics")))
			for (const FGeode& Ge : Geodes) if (R == Ge.Relic) RelicsForged.AddUnique(R);
	}
	if (const TSharedPtr<FJsonObject> As = JObj(A, TEXT("ascent")))
	{
		if (const TSharedPtr<FJsonObject> Rolls = JObj(*As, TEXT("rolls")))
			for (const auto& E : Rolls->Values)
			{
				TArray<FString> P;
				for (const FString& K : JStrArr(*Rolls, *E.Key)) if (FindPerk(K) && P.Num() < 2) P.Add(K);
				if (P.Num()) AscentRolls.Add(FString(*E.Key), P);
			}
		if (const TSharedPtr<FJsonObject> Dis = JObj(*As, TEXT("dis")))
			for (const auto& E : Dis->Values) AscentDis.Add(FString(*E.Key), FMath::Clamp((int32)JInt(*Dis, *E.Key), 0, 99));
		for (const FString& N : JStrArr(*As, TEXT("pat"))) AscentPat.AddUnique(N);
	}
	if (const TSharedPtr<FJsonObject> L = JObj(A, TEXT("lumen")))
	{
		LumenScore = FMath::Clamp((float)JNum(*L, TEXT("score"), 1000.0), 1000.f, 1100.f);
		LumenWeek = (int32)JInt(*L, TEXT("week"));
		LumenPinn = FMath::Clamp((int32)JInt(*L, TEXT("pinn")), 0, 9);
	}
	if (const TSharedPtr<FJsonObject> La = JObj(A, TEXT("lattice")))
	{
		if (const TSharedPtr<FJsonObject> Boards = JObj(*La, TEXT("boards")))
			for (const TCHAR* S : LatStyles)
			{
				const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
				if (!Boards->TryGetArrayField(FString(S), Arr) || !Arr) continue;
				TArray<int32> B;
				for (const TSharedPtr<FJsonValue>& V : *Arr)
				{
					double D = -1.0;
					if (V.IsValid() && V->TryGetNumber(D) && D >= 0.0 && D < Hex().Num() && D == FMath::FloorToDouble(D)) B.AddUnique((int32)D);
				}
				if (B.Num()) LatticeBoards.Add(S, B);
			}
		LatticeSpent = FMath::Max(0, (int32)JInt(*La, TEXT("spent")));
	}
}

// ---------------------------------------------------------------- tick / kills

void UVenturesFeature::TickGear()
{
	if (TickN % 10 == 0) AscentScan();
}

void UVenturesFeature::DiedGear(AAerondelMob* Mob, bool bByPlayer, bool bTideBoss)
{
	if (!Mob || !Mob->Definition) return;
	bool bVenture = false;
	for (const TWeakObjectPtr<AAerondelMob>& W : Run.Foes) if (W.Get() == Mob) bVenture = true;
	const FString Name = Mob->Definition->DisplayName.ToString();
	const bool bBossy = IsBossy(Mob);
	if (bByPlayer)
	{
		for (const FGeode& G : Geodes) if (G.Boss && Name == G.Boss && CountItem(G.Name) > 0) GeodeOpen(G.Name);
		if (!bVenture && FMath::FRand() < (bBossy ? 0.2f : 0.006f + FMath::Min(0.01f, FMath::Max(1, Mob->Definition->CombatLevel) / 50000.f)))
		{
			const FGeode& G = Geodes[FMath::RandRange(0, 3)];
			GiveItem(G.Name, 1);
			Say(FString::Printf(TEXT("A sealed %s drops! (%s)"), G.Name, G.Boss ? *FString::Printf(TEXT("opens on a %s kill"), G.Boss) : TEXT("opens on a Breach clear")));
		}
	}
	if (LumenOn())
	{
		if (bTideBoss) Pinnacle(TEXT("Burst Tide"));
		else if (bBossy) LumenGain(FMath::Max(0.05f, 0.5f - (LumenScore - 1000.f) / 250.f));
	}
	if (bBossy)
	{
		bool bAny = false;
		for (const TCHAR* S : LatStyles) if (LatticeOpen(S)) bAny = true;
		if (bAny) BreachAnima += 1;
	}
}

void UVenturesFeature::BreachClearGear(const FString& Mode)
{
	for (const FGeode& G : Geodes) if (G.Breach && Mode == G.Breach && CountItem(G.Name) > 0) GeodeOpen(G.Name);
	if (FMath::FRand() < 0.3f) GiveItem(Geodes[FMath::RandRange(0, 3)].Name, 1);
}

// ---------------------------------------------------------------- Geodes

bool UVenturesFeature::GeodeOpen(const FString& Name)
{
	const FGeode* G = FindGeode(Name);
	if (!G || !TakeItem(Name, 1)) return false;
	TArray<FString>& Have = GeodeParts.FindOrAdd(Name);
	TArray<FString> Left;
	for (const TCHAR* P : GeodePartsAll) if (!Have.Contains(P)) Left.Add(P);
	if (!Left.Num()) { GiveCoins(2000); Say(FString::Printf(TEXT("The %s cracks open: 2,000 coins (you already have every part)."), *Name)); return true; }
	const FString P = Left[FMath::RandRange(0, Left.Num() - 1)];
	Have.Add(P);
	Say(FString::Printf(TEXT("The %s cracks open: %s %s (%d/4)."), *Name, G->Relic, *P, Have.Num()));
	return true;
}

void UVenturesFeature::RelicForge(const FString& Name)
{
	const FGeode* G = FindGeode(Name);
	const TArray<FString>* Have = GeodeParts.Find(Name);
	if (!G || !Have || Have->Num() < 4 || RelicsForged.Contains(G->Relic)) return;
	if (SkillLevel(TEXT("Smithing")) < 40) { Say(TEXT("Forging a relic needs Smithing 40.")); return; }
	RelicsForged.Add(G->Relic);
	GiveXp(TEXT("Smithing"), 2500.f);
	Say(FString::Printf(TEXT("You forge the %s! It is a permanent bonus."), G->Relic));
}

float UVenturesFeature::RelicSum(const FString& Stat) const
{
	float T = 0.f;
	for (const FGeode& G : Geodes) if (Stat == G.Stat && RelicsForged.Contains(G.Relic)) T += G.V;
	return T;
}

// ---------------------------------------------------------------- Ascent

bool UVenturesFeature::AscentOk(const UItemDef* I)
{
	return I && I->EquipSlot == EEquipSlot::Weapon && !I->bUnique && I->Rarity.IsNone();
}

void UVenturesFeature::AscentScan()
{
	const AAerondelPlayerState* PS = GetPS();
	if (!PS) return;
	TArray<const UItemDef*> Cand;
	if (PS->Inventory) for (const FItemStack& S : PS->Inventory->GetSlots()) if (!S.IsEmpty()) Cand.Add(S.Item.Get());
	Cand.Add(Weapon());
	for (const UItemDef* I : Cand)
	{
		if (!AscentOk(I)) continue;
		const FString N = GearName(I);
		if (AscentRolls.Contains(N)) continue;
		const TArray<FString> R = RollPerks();
		AscentRolls.Add(N, R);
		Say(FString::Printf(TEXT("Your %s rolls Ascent perks: %s + %s."), *N, FindPerk(R[0])->Name, FindPerk(R[1])->Name));
	}
}

static const UItemDef* ItemByName(const FString& N)
{
	const UGameDatabase* Db = UGameDatabase::Get();
	return Db ? Db->FindItem(AerondelId::FromName(N)) : nullptr;
}

void UVenturesFeature::AscentReroll(const FString& N)
{
	const UItemDef* I = ItemByName(N);
	if (!AscentOk(I)) return;
	const int32 Cost = FMath::Max(200, FMath::RoundToInt((I->Value > 0 ? I->Value : 50) * 2.f));
	if (!TakeCoins(Cost)) { Say(FString::Printf(TEXT("Rerolling costs %d coins."), Cost)); return; }
	const TArray<FString> R = RollPerks();
	AscentRolls.Add(N, R);
	Say(FString::Printf(TEXT("Rerolled: %s + %s."), FindPerk(R[0])->Name, FindPerk(R[1])->Name));
}

void UVenturesFeature::AscentDismantle(const FString& N)
{
	const UItemDef* I = ItemByName(N);
	if (!AscentOk(I) || CountItem(N) <= 0) { Say(TEXT("Dismantle a spare copy from your pack (not the one you wield).")); return; }
	if (!TakeItem(N, 1)) return;
	int32& D = AscentDis.FindOrAdd(N);
	D++;
	GiveXp(TEXT("Smithing"), 20.f + I->Value / 20.f);
	if (D >= 5 && !AscentPat.Contains(N)) { AscentPat.Add(N); Say(FString::Printf(TEXT("Pattern learned: %s. You can now pick its perks exactly."), *N)); }
	else Say(FString::Printf(TEXT("Dismantled %s (%d/5 toward its pattern)."), *N, FMath::Min(5, D)));
}

void UVenturesFeature::AscentSet(const FString& N, int32 Slot, const FString& K)
{
	if (!AscentPat.Contains(N) || !FindPerk(K) || Slot < 0 || Slot > 1) return;
	TArray<FString> R = AscentRolls.Contains(N) ? AscentRolls[N] : RollPerks();
	while (R.Num() < 2) R.Add(RollPerks()[0]);
	R[Slot] = K;
	if (R[0] == R[1])
		for (const FPerk& P : Perks) if (K != P.Key) { R[1 - Slot] = P.Key; break; }
	AscentRolls.Add(N, R);
}

float UVenturesFeature::AscentCur(const FString& Stat) const
{
	const TArray<FString>* R = AscentRolls.Find(GearName(Weapon()));
	if (!R) return 0.f;
	float T = 0.f;
	for (const FString& K : *R) if (const FPerk* P = FindPerk(K)) if (Stat == P->Stat) T += P->V;
	return T;
}

// ---------------------------------------------------------------- Lumen

bool UVenturesFeature::LumenOn() const
{
	static const TCHAR* Sk[] = { TEXT("Attack"), TEXT("Strength"), TEXT("Defence"), TEXT("Ranged"), TEXT("Magic"), TEXT("Swordsmanship"), TEXT("Hitpoints") };
	for (const TCHAR* S : Sk) if (SkillLevel(S) >= 1000) return true;
	return false;
}

void UVenturesFeature::LumenGain(float N)
{
	if (!LumenOn()) return;
	const float B = LumenScore;
	LumenScore = FMath::Min(1100.f, LumenScore + N);
	if (FMath::FloorToInt(LumenScore) > FMath::FloorToInt(B)) Say(FString::Printf(TEXT("Lumen rises to %d."), FMath::FloorToInt(LumenScore)));
}

bool UVenturesFeature::Pinnacle(const FString& Src)
{
	if (!LumenOn()) return false;
	const int32 W = WeekNow();
	if (LumenWeek != W) { LumenWeek = W; LumenPinn = 0; }
	if (LumenPinn >= 3) return false;
	LumenPinn++;
	Say(FString::Printf(TEXT("Pinnacle chest (%s)!"), *Src));
	LumenGain(FMath::Max(1.f, 6.f - (LumenScore - 1000.f) / 25.f));
	BonusDrop(nullptr, TEXT("the Pinnacle chest"));
	return true;
}

float UVenturesFeature::LumenBonus() const { return LumenOn() ? (LumenScore - 1000.f) * 0.001f : 0.f; }

// ---------------------------------------------------------------- Lattice

bool UVenturesFeature::LatticeOpen(const FString& S) const
{
	return SkillLevel(StyleSkill(S)) >= 1000 || (S == TEXT("Melee") && SkillLevel(TEXT("Strength")) >= 1000);
}

FString UVenturesFeature::LatticeBoard() const { return StanceMain.IsEmpty() ? Style() : StanceMain; }

float UVenturesFeature::LatticeSum(const FString& K, const FString& S) const
{
	if (!LatticeOpen(S)) return 0.f;
	const TArray<int32>* B = LatticeBoards.Find(S);
	if (!B) return 0.f;
	float T = 0.f;
	for (int32 i : *B)
	{
		if (NodeType(i) != K) continue;
		bool bLink = false;
		for (int32 j : *B) if (j != i && Adj(i, j) && NodeType(j) == K) bLink = true;
		T += LatVal(K) * (bLink ? 1.5f : 1.f);
	}
	return T;
}

void UVenturesFeature::LatticeFill(const FString& S, int32 i)
{
	if (!LatticeOpen(S)) { Say(FString::Printf(TEXT("Reach 1000 %s to open the %s lattice."), *StyleSkill(S), *S)); return; }
	if (i < 0 || i >= Hex().Num()) return;
	TArray<int32>& B = LatticeBoards.FindOrAdd(S);
	if (B.Contains(i)) return;
	bool bNext = i == Centre();
	for (int32 j : B) if (Adj(i, j)) bNext = true;
	if (!bNext) { Say(TEXT("Fill a node next to one you already have (start at the centre).")); return; }
	const int32 C = NodeCost(i);
	if (BreachAnima < C) { Say(FString::Printf(TEXT("That node costs %d Anima (you have %d)."), C, BreachAnima)); return; }
	BreachAnima -= C;
	LatticeSpent += C;
	B.Add(i);
}

void UVenturesFeature::LatticeReset(const FString& S)
{
	TArray<int32>* B = LatticeBoards.Find(S);
	int32 Back = 0;
	if (B) for (int32 i : *B) Back += NodeCost(i);
	BreachAnima += Back;
	LatticeSpent = FMath::Max(0, LatticeSpent - Back);
	LatticeBoards.Remove(S);
	Say(FString::Printf(TEXT("%s lattice reset; %d Anima refunded."), *S, Back));
}

// ---------------------------------------------------------------- totals

float UVenturesFeature::DmgGear(const UMobDef* Target) const
{
	float T = RelicSum(TEXT("dmg")) + AscentCur(TEXT("dmg")) + LumenBonus() + LatticeSum(TEXT("dmg"), LatticeBoard());
	if (Target)
	{
		bool bBossy = Target->bBoss;
		static const TCHAR* Words[] = { TEXT("Warden"), TEXT("Colossus"), TEXT("Behemoth"), TEXT("Warlord"), TEXT("Revenant"), TEXT("Tyrant"), TEXT("Archon"), TEXT("Knight"), TEXT("Sunderlord") };
		const FString N = Target->DisplayName.ToString();
		if (!bBossy && Target->CombatLevel >= 140) for (const TCHAR* Wd : Words) if (N.Contains(Wd)) bBossy = true;
		if (bBossy) T += AscentCur(TEXT("boss"));
	}
	return T;
}
float UVenturesFeature::AccGear() const { return RelicSum(TEXT("acc")) + AscentCur(TEXT("acc")) + LatticeSum(TEXT("acc"), LatticeBoard()); }
float UVenturesFeature::DrGear() const { return RelicSum(TEXT("dr")) + AscentCur(TEXT("dr")) + LumenBonus() / 2.f + LatticeSum(TEXT("dr"), LatticeBoard()); }
float UVenturesFeature::XpGear(FName SkillId) const { return RelicSum(TEXT("xp")) + AscentCur(TEXT("xp")) + LatticeSum(TEXT("xp"), LatticeBoard()); }
float UVenturesFeature::FlareGear() const { return AscentCur(TEXT("flare")) + LatticeSum(TEXT("flare"), LatticeBoard()); }
float UVenturesFeature::StagGear() const { return AscentCur(TEXT("stag")) + LatticeSum(TEXT("stag"), LatticeBoard()); }

// ---------------------------------------------------------------- tabs

void UVenturesFeature::TabGeodes(FGPanelModel& M)
{
	M.Title = TEXT("Sealed Geodes");
	for (const FGeode& G : Geodes)
	{
		const TArray<FString>* Have = GeodeParts.Find(G.Name);
		const bool bDone = RelicsForged.Contains(G.Relic);
		FString Parts;
		for (const TCHAR* P : GeodePartsAll) Parts += FString::Printf(TEXT("%s%s "), Have && Have->Contains(P) ? TEXT("[x]") : TEXT("[ ]"), P);
		const FString Opens = G.Boss ? FString::Printf(TEXT("a %s kill"), G.Boss) : FString::Printf(TEXT("a %s Breach clear"), FCString::Strcmp(G.Breach, TEXT("ward")) == 0 ? TEXT("Hold the Ward") : TEXT("Sever the Nodes"));
		M.Lines.Add(FString::Printf(TEXT("%s x%d - opens on %s. %s (+%d%% %s): %s%s"), G.Name, CountItem(G.Name), *Opens, G.Relic, FMath::RoundToInt(G.V * 100.f), StatLabel(G.Stat), *Parts, bDone ? TEXT(" FORGED") : TEXT("")));
		if (!bDone && Have && Have->Num() >= 4) { const FString N = G.Name; VAct4(M, FString::Printf(TEXT("Forge %s"), G.Relic), [this, N] { RelicForge(N); }); }
	}
	M.Lines.Add(TEXT("Geodes drop from foes (bosses often). Carry one when you beat its boss or clear its Breach to open it. All 4 parts + Smithing 40 forge a permanent relic."));
}

void UVenturesFeature::TabAscent(FGPanelModel& M)
{
	M.Title = TEXT("Ascent Rolls");
	TArray<FString> List;
	const AAerondelPlayerState* PS = GetPS();
	if (AscentOk(Weapon())) List.AddUnique(GearName(Weapon()));
	if (PS && PS->Inventory) for (const FItemStack& S : PS->Inventory->GetSlots()) if (!S.IsEmpty() && AscentOk(S.Item.Get())) List.AddUnique(GearName(S.Item.Get()));
	if (!List.Contains(AscentSel)) AscentSel = List.Num() ? List[0] : FString();
	if (AscentSel.IsEmpty()) { M.Lines.Add(TEXT("No ordinary weapon in your pack or hands. (Uniques never roll.)")); return; }
	if (List.Num() > 1) VAct4(M, FString::Printf(TEXT("Weapon: %s (next)"), *AscentSel), [this, List] { AscentSel = List[(List.IndexOfByKey(AscentSel) + 1) % List.Num()]; });
	const FString N = AscentSel;
	const TArray<FString>* R = AscentRolls.Find(N);
	const bool bPat = AscentPat.Contains(N);
	M.Lines.Add(N);
	if (R) for (int32 i = 0; i < R->Num(); ++i) if (const FPerk* P = FindPerk((*R)[i])) M.Lines.Add(FString::Printf(TEXT("Perk %d: %s - %s"), i + 1, P->Name, P->Desc));
	if (!R) M.Lines.Add(TEXT("Not rolled yet."));
	const UItemDef* I = ItemByName(N);
	const int32 Cost = I ? FMath::Max(200, FMath::RoundToInt((I->Value > 0 ? I->Value : 50) * 2.f)) : 200;
	const int32* D = AscentDis.Find(N);
	VAct4(M, FString::Printf(TEXT("Reroll (%d coins)"), Cost), [this, N] { AscentReroll(N); });
	VAct4(M, FString::Printf(TEXT("Dismantle a copy (%d/5)"), FMath::Min(5, D ? *D : 0)), [this, N] { AscentDismantle(N); }, CountItem(N) > 0);
	if (bPat && R)
		for (int32 Slot = 0; Slot < 2; ++Slot)
		{
			const FString CurK = R->IsValidIndex(Slot) ? (*R)[Slot] : FString();
			const int32 Ix = FMath::Max(0, [&]{ for (int32 k = 0; k < (int32)UE_ARRAY_COUNT(Perks); ++k) if (CurK == Perks[k].Key) return k; return 0; }());
			const FString NextK = Perks[(Ix + 1) % (int32)UE_ARRAY_COUNT(Perks)].Key;
			VAct4(M, FString::Printf(TEXT("Perk %d: next choice (%s)"), Slot + 1, FindPerk(NextK)->Name), [this, N, Slot, NextK] { AscentSet(N, Slot, NextK); });
		}
	M.Lines.Add(bPat ? FString(TEXT("Pattern known: choose perks exactly.")) : FString(TEXT("Dismantle 5 spare copies to learn the pattern.")));
}

void UVenturesFeature::TabLumen(FGPanelModel& M)
{
	M.Title = TEXT("Lumen Rise");
	if (!LumenOn()) { M.Lines.Add(TEXT("Reach level 1000 in any combat skill to begin.")); return; }
	M.Title = FString::Printf(TEXT("Lumen %d / 1100"), FMath::FloorToInt(LumenScore));
	M.Lines.Add(FString::Printf(TEXT("+%.1f%% damage, %.1f%% less damage taken."), LumenBonus() * 100.f, LumenBonus() * 50.f));
	M.Lines.Add(FString::Printf(TEXT("Pinnacle chests this week: %d/3 (Vigil Gates clears, Burst Tide boss waves). Resets Monday 00:00 UTC."), LumenWeek == WeekNow() ? LumenPinn : 0));
}

void UVenturesFeature::TabLattice(FGPanelModel& M)
{
	if (LatticeView.IsEmpty()) LatticeView = LatticeBoard();
	const FString V = LatticeView;
	const bool bOpen = LatticeOpen(V);
	M.Title = FString::Printf(TEXT("Anima Lattice - %s"), *V);
	for (const TCHAR* S : LatStyles) { const FString St = S; VAct4(M, FString::Printf(TEXT("View %s%s"), S, LatticeOpen(St) ? TEXT("") : TEXT(" (locked)")), [this, St] { LatticeView = St; }); }
	M.Lines.Add(FString::Printf(TEXT("Anima %d - %s"), BreachAnima, bOpen ? *FString::Printf(TEXT("active board: %s"), *LatticeBoard()) : *FString::Printf(TEXT("needs %s 1000"), *StyleSkill(V))));
	if (!bOpen) return;
	const TArray<int32>* B = LatticeBoards.Find(V);
	for (int32 i = 0; i < Hex().Num(); ++i)
	{
		const bool bOn = B && B->Contains(i);
		bool bCan = !bOn && i == Centre();
		if (!bOn && B) for (int32 j : *B) if (Adj(i, j)) bCan = true;
		if (!bOn && !bCan) continue;
		const FString T = NodeType(i);
		VAct4(M, FString::Printf(TEXT("%s ring %d: %s (+%.1f%%)%s"), bOn ? TEXT("[x]") : TEXT("[ ]"), Ring(i), StatLabel(T), LatVal(T) * 100.f, bOn ? TEXT("") : *FString::Printf(TEXT(" - %d Anima"), NodeCost(i))), [this, V, i] { LatticeFill(V, i); }, !bOn);
	}
	FString Sum;
	for (const TCHAR* K : LatTypes) Sum += FString::Printf(TEXT("%s +%.1f%%  "), StatLabel(K), LatticeSum(K, V) * 100.f);
	M.Lines.Add(Sum);
	M.Lines.Add(FString::Printf(TEXT("%d/19 nodes filled. Matching neighbours count 1.5x."), B ? B->Num() : 0));
	VAct4(M, TEXT("Reset (full refund)"), [this, V] { LatticeReset(V); });
}
