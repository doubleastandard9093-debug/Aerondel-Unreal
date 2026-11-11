#include "Features/VenturesFeature.h"
#include "UI/AerondelPanels.h"
#include "Core/AerondelPlayerState.h"
#include "Combat/AerondelMob.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// Ventures group 16 (web adapt/leyweave.js): a shared web of small nodes, notables and a few keystones.
// Spend Ley (from fights and bosses — not Anima, which stays on the Lattice). Opens at combat level 50.

namespace VenturesLey
{
	struct FNode { FString Id, Name, Desc, Fx, Key, Kind; int32 Cost = 1; int32 Ring = 0; };
	struct FWeb { TArray<FNode> Nodes; TMap<FString, int32> Index; TMap<FString, TArray<FString>> Links; };
	static uint32 HashId(const FString& S)
	{
		uint32 H = 2166136261u;
		for (int32 i = 0; i < S.Len(); ++i) { H ^= (uint32)S[i]; H *= 16777619u; }
		return H;
	}
	static const FWeb& Web()
	{
		static FWeb W;
		static bool bBuilt = false;
		if (bBuilt) return W;
		bBuilt = true;
		auto Add = [&](const FNode& N) { W.Index.Add(N.Id, W.Nodes.Num()); W.Nodes.Add(N); W.Links.FindOrAdd(N.Id); };
		auto Link = [&](const FString& A, const FString& B)
		{
			if (A == B) return;
			TArray<FString>& LA = W.Links.FindOrAdd(A);
			TArray<FString>& LB = W.Links.FindOrAdd(B);
			LA.AddUnique(B); LB.AddUnique(A);
		};
		FNode Start; Start.Id = TEXT("start"); Start.Name = TEXT("Ley Heart"); Start.Kind = TEXT("start"); Start.Cost = 0; Start.Desc = TEXT("The centre of the weave. Path outward from here.");
		Add(Start);
		struct FKey { const TCHAR* Id; const TCHAR* Name; const TCHAR* Desc; };
		static const FKey Keys[] = {
			{ TEXT("rarecarve"), TEXT("Rare Carve"), TEXT("Carved hunt parts are rarer, but a carve always yields a set piece when one exists.") },
			{ TEXT("stillblade"), TEXT("Still Blade"), TEXT("A Guardbreak riposte window lasts a little longer.") },
			{ TEXT("seasonkeep"), TEXT("Season's Keep"), TEXT("Greenstead can cover one extra plot so a crop can grow out of season.") },
			{ TEXT("wordecho"), TEXT("Word-echo"), TEXT("A Wordcry also briefly slows nearby foes.") },
			{ TEXT("leyblood"), TEXT("Leyblood"), TEXT("Kills grant more Ley, and Dull facets drop a little more often.") },
			{ TEXT("kindsteel"), TEXT("Kind Steel"), TEXT("Unsocketing a Facet Stone is free to your purse, and stunning skills hold a breath longer.") },
		};
		struct FNote { const TCHAR* Id; const TCHAR* Name; const TCHAR* Desc; };
		static const FNote Notes[] = {
			{ TEXT("clean-carve"), TEXT("Clean Carve"), TEXT("A slightly longer carve window on a hunt.") },
			{ TEXT("wide-guard"), TEXT("Wide Guard"), TEXT("Reaping Whirl reaches half a metre farther.") },
			{ TEXT("iron-hold"), TEXT("Iron Hold"), TEXT("Stuns last a little longer.") },
			{ TEXT("green-hand"), TEXT("Green Hand"), TEXT("Crops on the farm grow a little faster.") },
			{ TEXT("square-hand"), TEXT("Square Hand"), TEXT("You furnish a Townstead a little faster.") },
			{ TEXT("ley-sight"), TEXT("Ley Sight"), TEXT("Bosses drop more Ley.") },
			{ TEXT("soft-step"), TEXT("Soft Step"), TEXT("A successful dodge refunds a little run energy.") },
			{ TEXT("word-rest"), TEXT("Word Rest"), TEXT("Wordcries come off cooldown a little sooner.") },
		};
		static const TCHAR* SmallFx[] = { TEXT("ley"), TEXT("carve"), TEXT("stun"), TEXT("green"), TEXT("square"), TEXT("word") };
		const int32 Counts[] = { 0, 8, 12, 16, 18, 20, 22, 16, 12 };
		TArray<TArray<FString>> Rings; Rings.Add({ TEXT("start") });
		int32 Ni = 0, Ki = 0;
		for (int32 R = 1; R < UE_ARRAY_COUNT(Counts); ++R)
		{
			const int32 N = Counts[R];
			TArray<FString> Row;
			for (int32 K = 0; K < N; ++K)
			{
				const FString Id = FString::Printf(TEXT("r%dk%d"), R, K);
				FNode Node; Node.Id = Id; Node.Ring = R; Node.Cost = 1; Node.Kind = TEXT("small");
				Node.Name = TEXT("Ley thread"); Node.Desc = TEXT("A small thread of the weave, used to path outward.");
				if (R >= 7 && Ki < UE_ARRAY_COUNT(Keys) && (K % FMath::Max(1, N / (int32)UE_ARRAY_COUNT(Keys))) == 0)
				{
					const FKey& Key = Keys[Ki++];
					Node.Kind = TEXT("key"); Node.Name = Key.Name; Node.Desc = Key.Desc; Node.Fx = Key.Id; Node.Key = Key.Id; Node.Cost = 6;
				}
				else if (R >= 2 && (R % 2) == 0 && (K % 4) == 0 && Ni < UE_ARRAY_COUNT(Notes))
				{
					const FNote& Note = Notes[Ni++];
					Node.Kind = TEXT("notable"); Node.Name = Note.Name; Node.Desc = Note.Desc; Node.Fx = Note.Id; Node.Cost = 3;
				}
				else
				{
					const uint32 H = HashId(Id) + (uint32)R + (uint32)K;
					Node.Fx = SmallFx[H % UE_ARRAY_COUNT(SmallFx)];
				}
				Add(Node); Row.Add(Id);
			}
			Rings.Add(Row);
			for (int32 K = 0; K < Row.Num(); ++K) Link(Row[K], Row[(K + 1) % Row.Num()]);
			const TArray<FString>& Prev = Rings[R - 1];
			for (int32 K = 0; K < Row.Num(); ++K)
			{
				const int32 T = FMath::RoundToInt((float)K * Prev.Num() / (float)Row.Num()) % Prev.Num();
				Link(Row[K], Prev[T]);
				Link(Row[K], Prev[(T + 1) % Prev.Num()]);
			}
		}
		return W;
	}
	static const FNode* FindNode(const FString& Id)
	{
		const FWeb& W = Web();
		if (const int32* I = W.Index.Find(Id)) return &W.Nodes[*I];
		return nullptr;
	}
	static TArray<FString> Shortest(const FString& From, const FString& To)
	{
		const FWeb& W = Web();
		TArray<FString> Q; Q.Add(From);
		TMap<FString, FString> Prev; Prev.Add(From, FString());
		int32 Head = 0;
		while (Head < Q.Num())
		{
			const FString C = Q[Head++];
			if (C == To) break;
			if (const TArray<FString>* L = W.Links.Find(C))
				for (const FString& N : *L) if (!Prev.Contains(N)) { Prev.Add(N, C); Q.Add(N); }
		}
		TArray<FString> Path;
		if (!Prev.Contains(To)) return Path;
		for (FString C = To; !C.IsEmpty(); )
		{
			Path.Insert(C, 0);
			const FString* P = Prev.Find(C);
			C = P ? *P : FString();
			if (C == To) break;
		}
		return Path;
	}
	static const TCHAR* Starts[][2] = {
		{ TEXT("wanderer"), TEXT("Wanderer") },
		{ TEXT("melee"), TEXT("Melee") },
		{ TEXT("ranged"), TEXT("Ranged") },
		{ TEXT("magic"), TEXT("Magic") },
		{ TEXT("sword"), TEXT("Swordsmanship") },
	};
	static bool IsStart(const FString& Id) { for (const auto& S : Starts) if (Id == S[0]) return true; return false; }
	static void VAct16(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
}
using namespace VenturesLey;

void UVenturesFeature::ResetLey()
{
	LeyNodes.Reset();
	LeyAmt = 0;
	LeySpent = 0;
	LeyStart.Reset();
	bLeyRec = false;
}

void UVenturesFeature::SaveLey(FJsonObject& A) const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> N;
	for (const FString& Id : LeyNodes) N.Add(MakeShared<FJsonValueString>(Id));
	S->SetArrayField(TEXT("nodes"), N);
	S->SetNumberField(TEXT("ley"), LeyAmt);
	S->SetNumberField(TEXT("spent"), LeySpent);
	if (LeyStart.IsEmpty()) S->SetField(TEXT("start"), MakeShared<FJsonValueNull>());
	else S->SetStringField(TEXT("start"), LeyStart);
	A.SetObjectField(TEXT("ley"), S);
}

void UVenturesFeature::LoadLey(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> S = JObj(A, TEXT("ley"));
	if (!S) return;
	LeyAmt = FMath::Max<int32>(0, (int32)JInt(*S, TEXT("ley")));
	LeySpent = FMath::Max<int32>(0, (int32)JInt(*S, TEXT("spent")));
	const FString St = JStr(*S, TEXT("start"));
	if (IsStart(St)) LeyStart = St;
	if (const TArray<TSharedPtr<FJsonValue>>* N = nullptr; S->TryGetArrayField(TEXT("nodes"), N) && N)
	{
		for (const TSharedPtr<FJsonValue>& V : *N)
		{
			if (!V.IsValid() || V->Type != EJson::String) continue;
			const FString Id = V->AsString();
			if (FindNode(Id) && !LeyNodes.Contains(Id)) LeyNodes.Add(Id);
		}
	}
}

bool UVenturesFeature::LeyOpen() const { return CombatLevel() >= 50; }

bool UVenturesFeature::LeyKey(const FString& Id) const
{
	if (!LeyNodes.Contains(TEXT("start"))) return false;
	for (const FNode& N : Web().Nodes) if (N.Key == Id && LeyNodes.Contains(N.Id)) return true;
	return false;
}

bool UVenturesFeature::LeyHas(const FString& Id) const
{
	if (!LeyNodes.Contains(TEXT("start"))) return false;
	for (const FNode& N : Web().Nodes) if (N.Fx == Id && LeyNodes.Contains(N.Id)) return true;
	return false;
}

FLeyFx UVenturesFeature::LeyFx() const
{
	FLeyFx Fx;
	if (!LeyNodes.Contains(TEXT("start"))) return Fx;
	for (const FString& Id : LeyNodes)
	{
		const FNode* N = FindNode(Id); if (!N) continue;
		if (N->Fx == TEXT("ley") || N->Fx == TEXT("ley-sight")) Fx.LeyGain += N->Kind == TEXT("notable") ? 1.f : 0.2f;
		if (N->Fx == TEXT("wide-guard")) Fx.WhirlR += 0.5f;
		if (N->Fx == TEXT("iron-hold") || N->Fx == TEXT("stun") || N->Fx == TEXT("kindsteel")) Fx.Stun += (N->Kind == TEXT("key") || N->Kind == TEXT("notable")) ? 1 : 0;
		if (N->Fx == TEXT("green-hand") || N->Fx == TEXT("green")) Fx.GrowSpeed += N->Kind == TEXT("notable") ? 0.04f : 0.005f;
		if (N->Fx == TEXT("square-hand") || N->Fx == TEXT("square")) Fx.BuildSpeed += N->Kind == TEXT("notable") ? 0.04f : 0.005f;
		if (N->Fx == TEXT("clean-carve") || N->Fx == TEXT("carve")) Fx.CarveWin += N->Kind == TEXT("notable") ? 400 : 50;
		if (N->Fx == TEXT("word-rest") || N->Fx == TEXT("word")) Fx.CryCd += N->Kind == TEXT("notable") ? 0.12f : 0.02f;
	}
	if (LeyKey(TEXT("leyblood"))) Fx.LeyGain += 1.f;
	if (LeyKey(TEXT("kindsteel"))) Fx.Stun += 1;
	return Fx;
}

float UVenturesFeature::LeyWhirlBonus() const { return LeyFx().WhirlR; }
int32 UVenturesFeature::LeyStunBonus() const { return LeyFx().Stun; }
float UVenturesFeature::SteadLeyBuildBonus() const { return LeyFx().BuildSpeed; }
float UVenturesFeature::GreenLeyGrowBonus() const { return LeyFx().GrowSpeed; }
int32 UVenturesFeature::GreenCoverCap() const { return 1 + (LeyKey(TEXT("seasonkeep")) ? 1 : 0); }

FString UVenturesFeature::LeyDefaultStart() const
{
	if (VocId == TEXT("longbow")) return TEXT("ranged");
	if (VocId == TEXT("leysage") || VocId == TEXT("hearthcant")) return TEXT("magic");
	if (VocId == TEXT("swash")) return TEXT("sword");
	if (VocId == TEXT("warder")) return TEXT("melee");
	if (StanceMain == TEXT("Ranged")) return TEXT("ranged");
	if (StanceMain == TEXT("Magic")) return TEXT("magic");
	if (StanceMain == TEXT("Swordsmanship")) return TEXT("sword");
	if (StanceMain == TEXT("Melee")) return TEXT("melee");
	return TEXT("wanderer");
}

bool UVenturesFeature::LeyPickStart(const FString& Id)
{
	if (!LeyOpen()) { Say(TEXT("The Leyweave opens at combat level 50.")); return false; }
	if (!IsStart(Id)) return false;
	if (!LeyStart.IsEmpty() && LeyNodes.Num()) { Say(TEXT("Refund the weave at a bank before choosing a new start.")); return false; }
	LeyStart = Id;
	if (!LeyNodes.Contains(TEXT("start"))) LeyNodes.Add(TEXT("start"));
	FString Name = Id;
	for (const auto& S : Starts) if (Id == S[0]) Name = S[1];
	Say(FString::Printf(TEXT("You begin the Leyweave as a %s."), *Name));
	return true;
}

bool UVenturesFeature::LeyFill(const FString& Id)
{
	if (!LeyOpen()) { Say(TEXT("The Leyweave opens at combat level 50.")); return false; }
	const FNode* N = FindNode(Id); if (!N) return false;
	if (LeyStart.IsEmpty()) { if (!LeyPickStart(LeyDefaultStart())) return false; }
	if (LeyNodes.Contains(Id)) return false;
	if (Id != TEXT("start") && !LeyNodes.Contains(TEXT("start"))) { if (!LeyFill(TEXT("start"))) return false; }
	if (Id != TEXT("start"))
	{
		const TArray<FString>* L = Web().Links.Find(Id);
		bool bAdj = false; if (L) for (const FString& X : *L) if (LeyNodes.Contains(X)) { bAdj = true; break; }
		if (!bAdj) { Say(TEXT("Fill a node next to one you already have, starting at the Ley Heart.")); return false; }
	}
	const int32 Need = Id == TEXT("start") ? 50 : FMath::Min(1000, 50 + N->Ring * 80);
	if (CombatLevel() < Need) { Say(FString::Printf(TEXT("That ring of the weave opens at combat level %d."), Need)); return false; }
	if (LeyAmt < N->Cost) { Say(FString::Printf(TEXT("That node costs %d Ley (you have %d)."), N->Cost, LeyAmt)); return false; }
	LeyAmt -= N->Cost; LeySpent += N->Cost; LeyNodes.Add(Id);
	Say(N->Kind == TEXT("key") ? FString::Printf(TEXT("Keystone: %s. %s"), *N->Name, *N->Desc) : FString::Printf(TEXT("You path through %s."), *N->Name));
	return true;
}

bool UVenturesFeature::LeyRefund()
{
	if (LeyNodes.Num() == 0 && LeyStart.IsEmpty()) { Say(TEXT("The weave is already empty.")); return false; }
	const int32 Back = LeySpent * 7 / 10;
	if (!TakeCoins(500)) { Say(TEXT("Refunding the Leyweave costs 500 coins (or talk to a bank).")); return false; }
	LeyAmt += Back; LeySpent = 0; LeyNodes.Reset(); LeyStart.Reset();
	Say(FString::Printf(TEXT("The Leyweave is refunded. %d Ley returns to you."), Back));
	return true;
}

void UVenturesFeature::DiedLey(AAerondelMob* Mob, bool bByPlayer)
{
	if (!Mob || !bByPlayer || !LeyOpen()) return;
	const FLeyFx Fx = LeyFx();
	int32 N = 0;
	if (IsBossy(Mob)) N = 2 + FMath::FloorToInt(Fx.LeyGain);
	else if (FMath::FRand() < 0.12f + FMath::Min(0.15f, Fx.LeyGain * 0.05f)) N = 1;
	if (N) LeyAmt += N;
}

void UVenturesFeature::TabLey(FGPanelModel& M)
{
	const int32 Cb = CombatLevel();
	M.Title = FString::Printf(TEXT("Leyweave · Ley %d · spent %d · combat %d"), LeyAmt, LeySpent, Cb);
	if (!LeyOpen())
	{
		M.Lines.Add(TEXT("Opens at combat level 50. Ley is a currency of its own — not Anima, which stays on the Lattice."));
		return;
	}
	M.Lines.Add(TEXT("Anima stays on the Lattice. Ley is earned from fights and bosses. Keystones change rules, not a large damage number. Path outward from the Ley Heart."));
	for (const auto& S : Starts)
	{
		const bool bOn = LeyStart == S[0];
		VAct16(M, FString::Printf(TEXT("%s%s"), bOn ? TEXT("• Start: ") : TEXT("Begin as "), S[1]), [this, Id = FString(S[0])] { LeyPickStart(Id); });
	}
	VAct16(M, bLeyRec ? TEXT("• Recommended path on") : TEXT("Recommended path"), [this] { bLeyRec = !bLeyRec; });
	VAct16(M, TEXT("Refund (500 coins, 70% Ley returns)"), [this] { LeyRefund(); });
	TArray<FString> FilledKeys;
	for (const FNode& N : Web().Nodes) if (N.Kind == TEXT("key") && LeyNodes.Contains(N.Id)) FilledKeys.Add(N.Name);
	M.Lines.Add(FString::Printf(TEXT("Filled %d / %d. %s"), LeyNodes.Num(), Web().Nodes.Num(), FilledKeys.Num() ? *FString::Join(FilledKeys, TEXT(" · ")) : TEXT("No keystone yet.")));
	if (bLeyRec)
	{
		const FNode* FirstKey = nullptr;
		for (const FNode& N : Web().Nodes) if (N.Kind == TEXT("key")) { FirstKey = &N; break; }
		if (FirstKey)
		{
			const TArray<FString> Path = Shortest(TEXT("start"), FirstKey->Id);
			FString Next;
			for (const FString& Id : Path) if (!LeyNodes.Contains(Id)) { Next = Id; break; }
			if (Next.IsEmpty()) M.Lines.Add(FString::Printf(TEXT("Recommended path complete (%s)."), *FirstKey->Name));
			else if (const FNode* N = FindNode(Next))
			{
				M.Lines.Add(FString::Printf(TEXT("Next on the recommended path: %s (%d Ley, ring %d)."), *N->Name, N->Cost, N->Ring));
				VAct16(M, FString::Printf(TEXT("Fill %s"), *N->Name), [this, Next] { LeyFill(Next); });
			}
		}
	}
	int32 Shown = 0;
	for (const FNode& N : Web().Nodes)
	{
		if (LeyNodes.Contains(N.Id)) continue;
		bool bAdj = N.Id == TEXT("start");
		if (!bAdj) if (const TArray<FString>* L = Web().Links.Find(N.Id)) for (const FString& X : *L) if (LeyNodes.Contains(X)) { bAdj = true; break; }
		if (!bAdj) continue;
		if (N.Kind == TEXT("small") && Shown >= 10) continue;
		VAct16(M, FString::Printf(TEXT("Fill %s (%d Ley, ring %d)"), *N.Name, N.Cost, N.Ring), [this, Id = N.Id] { LeyFill(Id); });
		++Shown;
		if (Shown >= 16) break;
	}
	if (Shown == 0 && LeyNodes.Num()) M.Lines.Add(TEXT("No neighbouring node can be filled right now (check combat level, or refund and start again)."));
}
