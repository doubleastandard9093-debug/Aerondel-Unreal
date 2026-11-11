#include "Features/VenturesFeature.h"
#include "UI/AerondelPanels.h"
#include "Core/AerondelPlayerState.h"
#include "Core/PlayerStatsComponent.h"
#include "Combat/HealthComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Data/ItemDef.h"
#include "World/AerondelScalability.h"
#include "Dom/JsonObject.h"

// Ventures group 30 (web adapt/waycamp.js): one portable camp you place in the wild and pack up again.
// Not Freeframe, not a town house, not the farm. Tiny cap. No Holdraids. Construction XP is small —
// Freeframe is still the grind. One at a time. Not in a town, not on Greenstead, not on Claimridge.

namespace VenturesCamp
{
	static constexpr int32 TownR = 16, PlaceXp = 15, CraftXp = 40, CraftLvl = 8, PieceCapLow = 8, PieceCapHigh = 12;
	static constexpr float CampR = 4.f, PlotX = 22.f, PlotZ = 78.f, PlotHalf = 12.f;
	static const TCHAR* CampItem = TEXT("Waycamp kit");
	static const TCHAR* BasePieces[] = { TEXT("bed"), TEXT("chest"), TEXT("bench"), TEXT("fire"), TEXT("lantern") };
	struct FTown { const TCHAR* Name; float X, Z; };
	static const FTown Towns[] = {
		{ TEXT("Larkhaven"), 0.f, 0.f }, { TEXT("Emberfold"), 50.f, -24.f }, { TEXT("Saltreach"), -80.f, 60.f },
		{ TEXT("Highcrest"), 120.f, -40.f }, { TEXT("Whitecairn"), -60.f, -80.f }
	};
	struct FExtra { const TCHAR* Id; const TCHAR* Name; int32 Logs; int32 Xp; };
	static const FExtra Extras[] = {
		{ TEXT("stool"), TEXT("Stool"), 1, 5 }, { TEXT("crate"), TEXT("Crate"), 1, 5 }, { TEXT("rack"), TEXT("Rack"), 1, 5 }
	};
	static bool IsBase(const FString& P) { for (const TCHAR* B : BasePieces) if (P == B) return true; return false; }
	static void EnsureBase(TArray<FString>& Pieces)
	{
		if (!Pieces.Num()) for (const TCHAR* B : BasePieces) Pieces.Add(B);
	}
	static void VAct30(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
}
using namespace VenturesCamp;

int32 UVenturesFeature::CampCap() const
{
	return UAerondelScalability::GetSavedTier() <= EAerondelTier::Low ? PieceCapLow : PieceCapHigh;
}

FString UVenturesFeature::CampForbidden(float X, float Z) const
{
	for (const FTown& T : Towns)
		if (FMath::Hypot(X - T.X, Z - T.Z) < TownR) return T.Name;
	const FVector2D Hold = ZoneC(TEXT("hold"));
	if (FMath::Abs(X - Hold.X) < 20.f && FMath::Abs(Z - Hold.Y) < 16.f) return TEXT("Greenstead");
	if (FMath::Abs(X - PlotX) <= PlotHalf && FMath::Abs(Z - PlotZ) <= PlotHalf) return TEXT("Claimridge");
	return FString();
}

bool UVenturesFeature::CampHere() const
{
	if (!bCampPlaced) return false;
	FVector2D P; if (!PlayerWebPos(P)) return false;
	return FMath::Hypot(P.X - CampX, P.Y - CampZ) < CampR + 0.5f;
}

void UVenturesFeature::ResetCamp()
{
	bCampOwned = false;
	bCampPlaced = false;
	CampX = CampZ = 0.f;
	CampChest.Reset();
	CampPieces.Reset();
}

void UVenturesFeature::SaveCamp(FJsonObject& A) const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	S->SetBoolField(TEXT("owned"), bCampOwned);
	if (bCampPlaced)
	{
		TSharedRef<FJsonObject> P = MakeShared<FJsonObject>();
		P->SetNumberField(TEXT("x"), CampX);
		P->SetNumberField(TEXT("z"), CampZ);
		P->SetNumberField(TEXT("rot"), 0);
		S->SetObjectField(TEXT("placed"), P);
	}
	PutIntMap(*S, TEXT("chest"), CampChest);
	PutStrArr(*S, TEXT("pieces"), CampPieces);
	A.SetObjectField(TEXT("camp"), S);
}

void UVenturesFeature::LoadCamp(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> S = JObj(A, TEXT("camp"));
	if (!S) return;
	bCampOwned = JBool(*S, TEXT("owned"));
	if (const TSharedPtr<FJsonObject> P = JObj(*S, TEXT("placed")))
	{
		bCampPlaced = true;
		CampX = (float)JNum(*P, TEXT("x"));
		CampZ = (float)JNum(*P, TEXT("z"));
	}
	CampChest = JIntMap(*S, TEXT("chest"));
	CampPieces = JStrArr(*S, TEXT("pieces"));
	if (CampPieces.Num() > PieceCapHigh) CampPieces.SetNum(PieceCapHigh);
	EnsureBase(CampPieces);
}

bool UVenturesFeature::CampCraft()
{
	if (bCampOwned || CountItem(CampItem) >= 1) { Say(TEXT("You already have a Waycamp kit.")); return false; }
	if (SkillLevel(TEXT("Construction")) < CraftLvl) { Say(TEXT("You need Construction 8 to pack a Waycamp.")); return false; }
	if (CountItem(TEXT("Logs")) < 8 || CountItem(TEXT("Iron bar")) < 2) { Say(TEXT("You need 8 Logs and 2 Iron bars.")); return false; }
	TakeItem(TEXT("Logs"), 8); TakeItem(TEXT("Iron bar"), 2);
	GiveItem(CampItem, 1);
	bCampOwned = true;
	EnsureBase(CampPieces);
	GiveXp(TEXT("Construction"), (float)CraftXp);
	Say(TEXT("You pack a Waycamp kit. Place it in the wild — not in a town, not on Greenstead, not on Claimridge."));
	return true;
}

bool UVenturesFeature::CampPlaceAt(float X, float Z)
{
	if (bCampPlaced) { Say(TEXT("You already have a camp down. Pack it up first.")); return false; }
	if (!bCampOwned && CountItem(CampItem) >= 1) bCampOwned = true;
	if (CountItem(CampItem) < 1) { Say(TEXT("Craft a Waycamp kit first.")); return false; }
	const FString Why = CampForbidden(X, Z);
	if (!Why.IsEmpty()) { Say(FString::Printf(TEXT("You cannot place a Waycamp in %s. Find a spot in the wild."), *Why)); return false; }
	if (!TakeItem(CampItem, 1)) return false;
	bCampPlaced = true; CampX = X; CampZ = Z;
	EnsureBase(CampPieces);
	GiveXp(TEXT("Construction"), (float)PlaceXp);
	Say(FString::Printf(TEXT("You set down your Waycamp. Pack it up from Ventures → Waycamp when you want to move it. +%d Construction XP."), PlaceXp));
	return true;
}

bool UVenturesFeature::CampPlace()
{
	FVector2D P; if (!PlayerWebPos(P)) { Say(TEXT("Move to a clear wild spot first.")); return false; }
	return CampPlaceAt(P.X, P.Y);
}

bool UVenturesFeature::CampPack()
{
	if (!bCampPlaced) { Say(TEXT("No camp is down.")); return false; }
	bCampPlaced = false;
	int32 ExtrasN = 0;
	CampPieces = CampPieces.FilterByPredicate([&](const FString& P) { if (IsBase(P)) return true; ++ExtrasN; return false; });
	EnsureBase(CampPieces);
	if (ExtrasN) GiveItem(TEXT("Logs"), ExtrasN);
	GiveItem(CampItem, 1);
	Say(TEXT("You pack the Waycamp. The kit is in your pack; the chest is still packed with it."));
	return true;
}

bool UVenturesFeature::CampRest()
{
	if (!bCampPlaced) { Say(TEXT("Place the camp first.")); return false; }
	if (!CampHere()) { Say(TEXT("Stand at your Waycamp to rest.")); return false; }
	if (!CampPieces.Contains(TEXT("bed"))) { Say(TEXT("The camp has no bed.")); return false; }
	AAerondelPlayerState* PS = GetPS(); if (!PS || !PS->Health) return false;
	const int32 Heal = FMath::Max(1, FMath::RoundToInt(PS->Health->GetMax() * 0.2f));
	PS->Health->Heal(Heal); if (PS->Stats) PS->Stats->RunEnergy = 100.f;
	Say(FString::Printf(TEXT("You rest at your Waycamp. (+%d hitpoints, run energy restored.)"), Heal));
	RoadRest(70);
	return true;
}

bool UVenturesFeature::CampDeposit(const FString& Item, int32 N)
{
	if (!bCampPlaced && !bCampOwned) return false;
	N = FMath::Max(1, N);
	if (Item.IsEmpty() || CountItem(Item) < N) { Say(TEXT("You do not have that.")); return false; }
	if (!TakeItem(Item, N)) return false;
	CampChest.FindOrAdd(Item) += N;
	Say(FString::Printf(TEXT("You put %d %s in the Waycamp chest."), N, *Item));
	return true;
}

bool UVenturesFeature::CampTake(const FString& Item, int32 N)
{
	N = FMath::Max(1, N);
	if (CampChest.FindRef(Item) < N) { Say(TEXT("The chest does not have that many.")); return false; }
	int32& V = CampChest.FindChecked(Item);
	V -= N; if (V <= 0) CampChest.Remove(Item);
	GiveItem(Item, N);
	return true;
}

bool UVenturesFeature::CampAdd(const FString& Kind)
{
	const FExtra* E = nullptr;
	for (const FExtra& X : Extras) if (Kind == X.Id) { E = &X; break; }
	if (!E) return false;
	if (!bCampPlaced || !CampHere()) { Say(TEXT("Stand at your placed camp to add a piece.")); return false; }
	if (CampPieces.Num() >= CampCap()) { Say(FString::Printf(TEXT("The camp is at its cap (%d pieces)."), CampCap())); return false; }
	if (CountItem(TEXT("Logs")) < E->Logs) { Say(FString::Printf(TEXT("You need %d Logs."), E->Logs)); return false; }
	TakeItem(TEXT("Logs"), E->Logs);
	CampPieces.Add(Kind);
	GiveXp(TEXT("Construction"), (float)E->Xp);
	Say(FString::Printf(TEXT("You add a %s to the camp. +%d Construction XP."), E->Name, E->Xp));
	return true;
}

void UVenturesFeature::TabCamp(FGPanelModel& M)
{
	M.Title = TEXT("Waycamp");
	M.Lines.Add(TEXT("One portable camp. Place it in the wild and pack it up to move. Not Freeframe, not a town house, not the farm. Raids never come for it. Construction XP is small — Freeframe is still the grind. One at a time."));
	if (!bCampOwned && CountItem(CampItem) < 1)
		VAct30(M, TEXT("Craft a Waycamp kit (8 Logs, 2 Iron bars, Construction 8)"), [this] { CampCraft(); });
	else if (!bCampPlaced)
	{
		M.Lines.Add(CountItem(CampItem) ? TEXT("You have a packed kit.") : TEXT("You have a packed kit (it is not in your pack — check the bank)."));
		VAct30(M, TEXT("Place it here"), [this] { CampPlace(); });
	}
	else
	{
		M.Lines.Add(FString::Printf(TEXT("Camp is down at %.0f, %.0f. %s Pieces %d/%d."),
			CampX, CampZ, CampHere() ? TEXT("You are here.") : TEXT("Walk over to use the bed and chest."), CampPieces.Num(), CampCap()));
		if (CampHere()) VAct30(M, TEXT("Rest"), [this] { CampRest(); });
		VAct30(M, TEXT("Pack up"), [this] { CampPack(); });
	}
	if (bCampOwned)
	{
		M.Lines.Add(TEXT("Chest"));
		if (!CampChest.Num()) M.Lines.Add(TEXT("Chest is empty."));
		const bool bUse = CampHere() || !bCampPlaced;
		for (const auto& P : CampChest)
		{
			M.Lines.Add(FString::Printf(TEXT("%s ×%d"), *P.Key, P.Value));
			if (bUse) VAct30(M, FString::Printf(TEXT("Take 1 %s"), *P.Key), [this, It = P.Key] { CampTake(It, 1); });
		}
		if (CampHere())
		{
			if (const AAerondelPlayerState* PS = GetPS())
				if (PS->Inventory)
					for (const FItemStack& Sl : PS->Inventory->GetSlots())
					{
						if (Sl.IsEmpty() || !Sl.Item) continue;
						const FString Name = Sl.Item->DisplayName.ToString();
						VAct30(M, FString::Printf(TEXT("Deposit %s"), *Name), [this, Name] { CampDeposit(Name, 1); });
					}
			M.Lines.Add(TEXT("Add a piece"));
			for (const FExtra& E : Extras)
				VAct30(M, FString::Printf(TEXT("%s (1 Log)"), E.Name), [this, Id = FString(E.Id)] { CampAdd(Id); });
		}
	}
}
