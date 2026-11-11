#include "Features/VenturesFeature.h"
#include "UI/AerondelPanels.h"
#include "Core/AerondelPlayerState.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/EquipmentComponent.h"
#include "Data/ItemDef.h"
#include "Combat/AerondelMob.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// Ventures group 15 (web adapt/facets.js): socketed stones that change how a skill or technique behaves.
// Grafts stay flat stats. Sigils stay toggle perks. Facets must not add a large damage percent.

namespace VenturesFacets
{
	struct FFacet { const TCHAR* Name; const TCHAR* Id; const TCHAR* Desc; };
	static const FFacet Stones[] = {
		{ TEXT("Twin-cut Facet"),   TEXT("echo"),  TEXT("A socketed skill lands a second, lighter hit.") },
		{ TEXT("Storm-glass Facet"),TEXT("frost"), TEXT("A socketed skill briefly binds the foe in frost.") },
		{ TEXT("Ironroot Facet"),   TEXT("stun"),  TEXT("Stunning skills hold a foe a little longer.") },
		{ TEXT("Wide-reap Facet"),  TEXT("whirl"), TEXT("Reaping Whirl reaches farther.") },
		{ TEXT("Trail-disc Facet"), TEXT("trail"), TEXT("Ricochet or a chakram leaves a short cutting trail.") },
	};
	static constexpr int32 WeaponSlots = 2;
	static constexpr int32 FrameSlots = 2;
	static const FFacet* FindStone(const FString& Name) { for (const FFacet& S : Stones) if (Name == S.Name) return &S; return nullptr; }
	static void VAct15(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
}
using namespace VenturesFacets;

void UVenturesFeature::ResetFacet()
{
	FacetOnWeap.Reset();
	FacetFrame.Reset();
	bFacetFrame = false;
	FacetSel.Reset();
	FacetLastSpecial = NAME_None;
	FacetLastClass = NAME_None;
	FacetTrails.Reset();
}

void UVenturesFeature::SaveFacet(FJsonObject& A) const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> On = MakeShared<FJsonObject>();
	for (const auto& E : FacetOnWeap)
	{
		TArray<TSharedPtr<FJsonValue>> L;
		for (const FString& N : E.Value) L.Add(MakeShared<FJsonValueString>(N));
		On->SetArrayField(E.Key, L);
	}
	S->SetObjectField(TEXT("on"), On);
	TArray<TSharedPtr<FJsonValue>> Fr;
	for (const FString& N : FacetFrame) Fr.Add(MakeShared<FJsonValueString>(N));
	S->SetArrayField(TEXT("frame"), Fr);
	S->SetBoolField(TEXT("hasFrame"), bFacetFrame);
	A.SetObjectField(TEXT("facets"), S);
}

void UVenturesFeature::LoadFacet(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> S = JObj(A, TEXT("facets"));
	if (!S) return;
	bFacetFrame = JBool(*S, TEXT("hasFrame"));
	if (const TSharedPtr<FJsonObject> On = JObj(*S, TEXT("on")))
	{
		for (const auto& E : On->Values)
		{
			if (!E.Value.IsValid() || E.Value->Type != EJson::Array) continue;
			TArray<FString> L;
			for (const TSharedPtr<FJsonValue>& V : E.Value->AsArray())
			{
				if (!V.IsValid() || V->Type != EJson::String) continue;
				const FString N = V->AsString();
				if (FindStone(N) && L.Num() < WeaponSlots) L.Add(N);
			}
			if (L.Num()) FacetOnWeap.Add(FString(*E.Key), L);
		}
	}
	if (const TArray<TSharedPtr<FJsonValue>>* Fr = nullptr; S->TryGetArrayField(TEXT("frame"), Fr) && Fr)
	{
		for (const TSharedPtr<FJsonValue>& V : *Fr)
		{
			if (!V.IsValid() || V->Type != EJson::String) continue;
			const FString N = V->AsString();
			if (FindStone(N) && FacetFrame.Num() < FrameSlots) FacetFrame.Add(N);
		}
	}
}

TArray<FString> UVenturesFeature::FacetSocketed() const
{
	TArray<FString> Out;
	if (const UItemDef* W = Weapon()) if (const TArray<FString>* L = FacetOnWeap.Find(W->DisplayName.ToString())) Out.Append(*L);
	if (bFacetFrame) Out.Append(FacetFrame);
	return Out;
}

bool UVenturesFeature::FacetOn(const FString& Id) const
{
	for (const FString& N : FacetSocketed()) if (const FFacet* S = FindStone(N)) if (Id == S->Id) return true;
	return false;
}

bool UVenturesFeature::FacetOn(const UObject* WorldContext, const FString& Id)
{
	const UVenturesFeature* V = Get(WorldContext);
	return V && V->FacetOn(Id);
}

float UVenturesFeature::FacetWhirlR() const { return FacetOn(TEXT("whirl")) ? 2.f : 0.f; }

float UVenturesFeature::WhirlBonusM(const UObject* WorldContext)
{
	const UVenturesFeature* V = Get(WorldContext);
	return V ? V->FacetWhirlR() + V->LeyWhirlBonus() : 0.f;
}

int32 UVenturesFeature::StunBonusTicks(const UObject* WorldContext, FName Special)
{
	const UVenturesFeature* V = Get(WorldContext);
	if (!V) return 0;
	int32 N = V->LeyStunBonus();
	if (V->FacetOn(TEXT("stun")) && (Special == TEXT("bash") || Special == TEXT("gale"))) N += 3;
	return N;
}

void UVenturesFeature::NoteCombatSpecial(const UObject* WorldContext, FName Special, FName WeaponClass)
{
	if (UVenturesFeature* V = Get(WorldContext)) { V->FacetLastSpecial = Special; V->FacetLastClass = WeaponClass; }
}


float UVenturesFeature::FacetDullChance() const
{
	return 0.22f + (LeyKey(TEXT("leyblood")) ? 0.12f : 0.f);
}

FString UVenturesFeature::FacetWeaponKey() const
{
	const UItemDef* W = Weapon();
	return W ? W->DisplayName.ToString() : FString();
}

bool UVenturesFeature::FacetSocket(const FString& Where, const FString& Stone)
{
	const FFacet* S = FindStone(Stone);
	if (!S || CountItem(Stone) < 1) { Say(TEXT("You do not have that Facet Stone.")); return false; }
	for (const FString& N : FacetSocketed())
	{
		if (N == Stone) { Say(TEXT("That kind of Facet is already socketed.")); return false; }
		if (const FFacet* O = FindStone(N)) if (O->Id == S->Id) { Say(TEXT("That kind of Facet is already socketed.")); return false; }
	}
	if (Where == TEXT("frame"))
	{
		if (!bFacetFrame) { Say(TEXT("Install a Facet frame first.")); return false; }
		if (FacetFrame.Num() >= FrameSlots) { Say(TEXT("The frame has no free sockets.")); return false; }
		if (!TakeItem(Stone, 1)) return false;
		FacetFrame.Add(Stone);
	}
	else
	{
		const FString W = FacetWeaponKey();
		if (W.IsEmpty()) { Say(TEXT("Wear a weapon first.")); return false; }
		TArray<FString>& L = FacetOnWeap.FindOrAdd(W);
		if (L.Num() >= WeaponSlots) { Say(TEXT("That weapon has no free Facet sockets (2).")); return false; }
		if (!TakeItem(Stone, 1)) return false;
		L.Add(Stone);
	}
	Say(FString::Printf(TEXT("You socket the %s. %s"), *Stone, S->Desc));
	return true;
}

bool UVenturesFeature::FacetUnsocket(const FString& Where, int32 I)
{
	TArray<FString>* L = nullptr;
	if (Where == TEXT("frame")) L = &FacetFrame;
	else
	{
		const FString W = FacetWeaponKey();
		if (W.IsEmpty()) return false;
		L = FacetOnWeap.Find(W);
	}
	if (!L || !L->IsValidIndex(I)) return false;
	const FString Stone = (*L)[I];
	L->RemoveAt(I);
	if (Where != TEXT("frame") && L->Num() == 0) FacetOnWeap.Remove(FacetWeaponKey());
	GiveItem(Stone, 1);
	Say(FString::Printf(TEXT("You unsocket the %s. The skill behaves as it used to."), *Stone));
	return true;
}

bool UVenturesFeature::FacetInstall()
{
	if (bFacetFrame) { Say(TEXT("A Facet frame is already installed.")); return false; }
	if (CountItem(TEXT("Facet frame")) < 1) { Say(TEXT("Craft or find a Facet frame first.")); return false; }
	if (!TakeItem(TEXT("Facet frame"), 1)) return false;
	bFacetFrame = true;
	Say(TEXT("You install a Facet frame. It holds two more stones beside your weapon."));
	return true;
}

int32 UVenturesFeature::HitFacet(AAerondelMob* Mob, int32 D)
{
	if (!Mob || D <= 0) return D;
	if (FacetOn(TEXT("frost")) && !FacetLastSpecial.IsNone())
	{
		Mob->Stun(2 + LeyStunBonus());
	}
	if (FacetOn(TEXT("trail")) && (FacetLastSpecial == TEXT("ricochet") || FacetLastClass == TEXT("chakram")))
	{
		FFacetTrail T; T.Mob = Mob; T.DueMs = NowMs() + 350; FacetTrails.Add(T);
	}
	return D;
}

void UVenturesFeature::TickFacet()
{
	if (FacetTrails.Num() == 0) return;
	const int64 Now = NowMs();
	TArray<FFacetTrail> Left;
	for (FFacetTrail& T : FacetTrails)
	{
		if (Now < T.DueMs) { Left.Add(T); continue; }
		AAerondelMob* M = T.Mob.Get();
		if (M && !M->IsDead() && M->GetHp() > 0)
		{
			Strike(M, 0.1f, -0.35f, TEXT("facet-trail"));
			Say(TEXT("The disc trail cuts again."));
		}
	}
	FacetTrails = MoveTemp(Left);
}

void UVenturesFeature::DiedFacet(AAerondelMob* Mob, bool bByPlayer)
{
	if (!Mob || !bByPlayer || !IsBossy(Mob)) return;
	if (FMath::FRand() >= FacetDullChance()) return;
	GiveItem(TEXT("Dull facet"), 1);
	Say(TEXT("A Dull facet falls from the fight. Cut it with Glyphcraft or Crafting."));
}

void UVenturesFeature::TabFacet(FGPanelModel& M)
{
	M.Title = TEXT("Facet Stones");
	M.Lines.Add(TEXT("These change how a skill behaves. Grafts still add stats; Sigils are still your toggles. A stone does not add a large damage percent."));
	TArray<FString> Held;
	if (const AAerondelPlayerState* PS = GetPS()) if (PS->Inventory)
	{
		for (const FItemStack& St : PS->Inventory->GetSlots())
		{
			if (St.IsEmpty()) continue;
			const FString N = St.Item->DisplayName.ToString();
			if (FindStone(N)) Held.AddUnique(N);
		}
	}
	if (Held.Num() == 0) M.Lines.Add(TEXT("Cut a Dull facet (Glyphcraft or Crafting) or take one from a boss."));
	for (const FString& N : Held)
	{
		const bool bSel = FacetSel == N;
		VAct15(M, FString::Printf(TEXT("%s%s ×%d"), bSel ? TEXT("• ") : TEXT(""), *N, CountItem(N)), [this, N, bSel] { FacetSel = bSel ? FString() : N; });
	}
	const FString W = FacetWeaponKey();
	const TArray<FString> Weap = W.IsEmpty() ? TArray<FString>() : FacetOnWeap.FindRef(W);
	M.Lines.Add(W.IsEmpty() ? TEXT("Weapon (wear one first)") : FString::Printf(TEXT("Weapon · %s (%d/%d)"), *W, Weap.Num(), WeaponSlots));
	for (int32 I = 0; I < Weap.Num(); ++I) VAct15(M, FString::Printf(TEXT("Unsocket %s from weapon"), *Weap[I]), [this, I] { FacetUnsocket(TEXT("weapon"), I); });
	if (!FacetSel.IsEmpty() && !W.IsEmpty() && Weap.Num() < WeaponSlots) VAct15(M, FString::Printf(TEXT("Socket %s in weapon"), *FacetSel), [this] { FacetSocket(TEXT("weapon"), FacetSel); });
	if (bFacetFrame)
	{
		M.Lines.Add(FString::Printf(TEXT("Facet frame (%d/%d)"), FacetFrame.Num(), FrameSlots));
		for (int32 I = 0; I < FacetFrame.Num(); ++I) VAct15(M, FString::Printf(TEXT("Unsocket %s from frame"), *FacetFrame[I]), [this, I] { FacetUnsocket(TEXT("frame"), I); });
		if (!FacetSel.IsEmpty() && FacetFrame.Num() < FrameSlots) VAct15(M, FString::Printf(TEXT("Socket %s in frame"), *FacetSel), [this] { FacetSocket(TEXT("frame"), FacetSel); });
	}
	else if (CountItem(TEXT("Facet frame")) > 0) VAct15(M, TEXT("Install Facet frame (2 more sockets)"), [this] { FacetInstall(); });
	else M.Lines.Add(TEXT("Craft a Facet frame (Crafting 30) to add 2 sockets."));
	const TArray<FString> All = FacetSocketed();
	if (All.Num())
	{
		FString Now = TEXT("Now:");
		for (const FString& N : All) if (const FFacet* S = FindStone(N)) { Now += TEXT(" "); Now += S->Desc; }
		M.Lines.Add(Now);
	}
	else M.Lines.Add(TEXT("No stones socketed."));
}
