#include "Features/VenturesFeature.h"
#include "UI/AerondelPanels.h"
#include "Core/AerondelPlayerState.h"
#include "Inventory/InventoryComponent.h"
#include "Data/ItemDef.h"
#include "World/AerondelScalability.h"
#include "Dom/JsonObject.h"

// Ventures group 29 (web adapt/wayfreight.js): supply lines between homes you already own.
// Linked chests share a small freight pool. This is not a new house and not a shared multiplayer stash.
// Construction XP is small. Townstead chests stay private until you deposit.
// The Hold forge can pull from the pool when you are on the island and a line includes the Hold.

namespace VenturesFreight
{
	static constexpr int32 LineGold = 200, LineXp = 25, StackCap = 50, TypeCap = 12, LineCapLow = 3, LineCapHigh = 5;
	struct FHome { const TCHAR* Id; const TCHAR* Name; const TCHAR* Kind; };
	static const FHome Homes[] = {
		{ TEXT("lark-e"),  TEXT("East Larkhaven cottage"),        TEXT("stead") },
		{ TEXT("lark-n"),  TEXT("North Larkhaven cottage"),       TEXT("stead") },
		{ TEXT("lark-w"),  TEXT("West Larkhaven cottage"),        TEXT("stead") },
		{ TEXT("lark-s"),  TEXT("South Larkhaven cottage"),       TEXT("stead") },
		{ TEXT("ember-a"), TEXT("Emberfold farmhouse"),           TEXT("stead") },
		{ TEXT("ember-b"), TEXT("Emberfold cottage"),             TEXT("stead") },
		{ TEXT("salt-a"),  TEXT("Saltreach fisherman's cottage"), TEXT("stead") },
		{ TEXT("high-a"),  TEXT("Highcrest townhouse"),           TEXT("stead") },
		{ TEXT("white-e"), TEXT("East Whitecairn house"),         TEXT("stead") },
		{ TEXT("white-w"), TEXT("West Whitecairn house"),         TEXT("stead") },
		{ TEXT("hold"),    TEXT("Hearthhold"),                    TEXT("hold") },
		{ TEXT("green"),   TEXT("Greenstead"),                    TEXT("green") },
		{ TEXT("claim"),   TEXT("Claimridge"),                    TEXT("frame") },
		{ TEXT("camp"),    TEXT("Waycamp"),                       TEXT("camp") },
	};
	static const FHome* FindHome(const FString& Id) { for (const FHome& H : Homes) if (Id == H.Id) return &H; return nullptr; }
	static void VAct29(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
}
using namespace VenturesFreight;

FString UVenturesFeature::FreightLineKey(const FString& A, const FString& B) const
{
	return (A < B) ? (A + TEXT("|") + B) : (B + TEXT("|") + A);
}

int32 UVenturesFeature::FreightCap() const
{
	return UAerondelScalability::GetSavedTier() <= EAerondelTier::Low ? LineCapLow : LineCapHigh;
}

bool UVenturesFeature::FreightOwned(const FString& Id) const
{
	const FHome* H = FindHome(Id); if (!H) return false;
	const FString K = H->Kind;
	if (K == TEXT("stead")) return SteadOwned.Contains(Id);
	if (K == TEXT("hold")) return bHoldOwned;
	if (K == TEXT("green")) return bGreenDeed;
	if (K == TEXT("frame")) return FrameClaimed();
	if (K == TEXT("camp")) return bCampOwned;
	return false;
}

bool UVenturesFeature::FreightLinked(const FString& A, const FString& B) const
{
	return FreightLines.Contains(FreightLineKey(A, B));
}

void UVenturesFeature::ResetFreight()
{
	FreightLines.Reset();
	FreightPool.Reset();
	FreightPick.Reset();
}

void UVenturesFeature::SaveFreight(FJsonObject& A) const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Lines;
	for (const FString& K : FreightLines)
	{
		FString AId, BId;
		if (!K.Split(TEXT("|"), &AId, &BId)) continue;
		TSharedRef<FJsonObject> L = MakeShared<FJsonObject>();
		L->SetStringField(TEXT("a"), AId);
		L->SetStringField(TEXT("b"), BId);
		Lines.Add(MakeShared<FJsonValueObject>(L));
	}
	S->SetArrayField(TEXT("lines"), Lines);
	PutIntMap(*S, TEXT("pool"), FreightPool);
	A.SetObjectField(TEXT("freight"), S);
}

void UVenturesFeature::LoadFreight(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> S = JObj(A, TEXT("freight"));
	if (!S) return;
	FreightPool = JIntMap(*S, TEXT("pool"));
	for (auto It = FreightPool.CreateIterator(); It; ++It)
	{
		It.Value() = FMath::Clamp(It.Value(), 0, StackCap);
		if (It.Value() <= 0) It.RemoveCurrent();
	}
	while (FreightPool.Num() > TypeCap)
	{
		auto It = FreightPool.CreateIterator(); if (It) It.RemoveCurrent(); else break;
	}
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (S->TryGetArrayField(TEXT("lines"), Arr) && Arr)
		for (const auto& V : *Arr)
		{
			if (!V.IsValid() || V->Type != EJson::Object) continue;
			const TSharedPtr<FJsonObject> L = V->AsObject();
			const FString AId = JStr(*L, TEXT("a")), BId = JStr(*L, TEXT("b"));
			if (AId.IsEmpty() || BId.IsEmpty() || AId == BId) continue;
			FreightLines.AddUnique(FreightLineKey(AId, BId));
		}
}

bool UVenturesFeature::FreightLink(const FString& A, const FString& B)
{
	if (A == B) { Say(TEXT("Pick two different homes.")); return false; }
	if (!FreightOwned(A) || !FreightOwned(B)) { Say(TEXT("You can only link homes you already own.")); return false; }
	if (FreightLinked(A, B)) { Say(TEXT("Those homes are already linked.")); return false; }
	if (FreightLines.Num() >= FreightCap())
	{
		Say(FString::Printf(TEXT("You can keep at most %d freight lines (%s)."), FreightCap(),
			UAerondelScalability::GetSavedTier() <= EAerondelTier::Low ? TEXT("Low") : TEXT("High")));
		return false;
	}
	if (!TakeCoins(LineGold)) { Say(FString::Printf(TEXT("A freight line costs %d coins."), LineGold)); return false; }
	FreightLines.Add(FreightLineKey(A, B));
	GiveXp(TEXT("Construction"), (float)LineXp);
	const FHome* HA = FindHome(A); const FHome* HB = FindHome(B);
	Say(FString::Printf(TEXT("You string a freight line between the %s and the %s. Linked chests share a small pool. +%d Construction XP."),
		HA ? HA->Name : *A, HB ? HB->Name : *B, LineXp));
	return true;
}

bool UVenturesFeature::FreightUnlink(const FString& A, const FString& B)
{
	const int32 N = FreightLines.RemoveSingle(FreightLineKey(A, B));
	if (!N) return false;
	Say(TEXT("You take the freight line down. The pool stays; that path is closed."));
	return true;
}

bool UVenturesFeature::FreightDeposit(const FString& Item, int32 N)
{
	N = FMath::Max(1, N);
	if (Item.IsEmpty() || CountItem(Item) < N) { Say(TEXT("That is not something you can freight.")); return false; }
	if (!FreightPool.Contains(Item) && FreightPool.Num() >= TypeCap) { Say(TEXT("The freight pool has no more item kinds (12).")); return false; }
	const int32 Room = StackCap - FreightPool.FindRef(Item);
	if (Room <= 0) { Say(FString::Printf(TEXT("The pool already holds %d %s."), StackCap, *Item)); return false; }
	const int32 Take = FMath::Min(N, Room);
	if (!TakeItem(Item, Take)) return false;
	FreightPool.FindOrAdd(Item) += Take;
	Say(FString::Printf(TEXT("You deposit %d %s into the freight pool."), Take, *Item));
	return true;
}

bool UVenturesFeature::FreightCanPull() const
{
	if (!bHoldOwned || !HoldHome()) return false;
	for (const FString& L : FreightLines) if (L.Contains(TEXT("hold"))) return true;
	return false;
}

int32 UVenturesFeature::FreightCount(const FString& Item) const { return FreightPool.FindRef(Item); }

int32 UVenturesFeature::CountWithFreight(const FString& Item) const
{
	return CountItem(Item) + (FreightCanPull() ? FreightCount(Item) : 0);
}

bool UVenturesFeature::FreightTake(const FString& Item, int32 N)
{
	N = FMath::Max(1, N);
	if (FreightCount(Item) < N) return false;
	int32& V = FreightPool.FindChecked(Item);
	V -= N;
	if (V <= 0) FreightPool.Remove(Item);
	return true;
}

bool UVenturesFeature::FreightWithdraw(const FString& Item, int32 N)
{
	if (!FreightTake(Item, N)) { Say(TEXT("The pool does not have that many.")); return false; }
	GiveItem(Item, N);
	Say(FString::Printf(TEXT("You take %d %s from the freight pool."), N, *Item));
	return true;
}

bool UVenturesFeature::TakeWithFreight(const FString& Item, int32 N)
{
	N = FMath::Max(1, N);
	const int32 Have = CountItem(Item);
	if (Have >= N) return TakeItem(Item, N);
	if (!FreightCanPull()) return false;
	const int32 Need = N - Have;
	if (FreightCount(Item) < Need) return false;
	if (Have && !TakeItem(Item, Have)) return false;
	return FreightTake(Item, Need);
}

void UVenturesFeature::TabFreight(FGPanelModel& M)
{
	M.Title = TEXT("Wayfreight");
	M.Lines.Add(TEXT("Link homes you already own. Linked chests share this pool. Townstead chests stay private until you deposit. The Hold forge can pull from the pool when you are on the island. This is not a new house, and it is not a shared multiplayer stash."));
	M.Lines.Add(FString::Printf(TEXT("Lines %d/%d · a line costs %d coins and grants %d Construction XP. Stack cap %d, %d kinds, %d lines on Low."),
		FreightLines.Num(), FreightCap(), LineGold, LineXp, StackCap, TypeCap, LineCapLow));
	M.Lines.Add(TEXT("Your homes"));
	int32 OwnedN = 0;
	for (const FHome& H : Homes)
	{
		if (!FreightOwned(H.Id)) continue;
		++OwnedN;
		const bool bOn = FreightPick == H.Id;
		VAct29(M, FString::Printf(TEXT("%s%s"), bOn ? TEXT("● ") : TEXT(""), H.Name), [this, Id = FString(H.Id)]
		{
			if (!FreightPick.IsEmpty() && FreightPick != Id) { FreightLink(FreightPick, Id); FreightPick.Reset(); }
			else FreightPick = (FreightPick == Id) ? FString() : Id;
		});
	}
	if (!OwnedN) M.Lines.Add(TEXT("Buy a Townstead, the Hold, Greenstead or Claimridge first."));
	M.Lines.Add(TEXT("Lines"));
	if (!FreightLines.Num()) M.Lines.Add(TEXT("No lines yet."));
	for (const FString& K : FreightLines)
	{
		FString A, B; if (!K.Split(TEXT("|"), &A, &B)) continue;
		const FHome* HA = FindHome(A); const FHome* HB = FindHome(B);
		VAct29(M, FString::Printf(TEXT("Unlink %s ↔ %s"), HA ? HA->Name : *A, HB ? HB->Name : *B),
			[this, A, B] { FreightUnlink(A, B); });
	}
	M.Lines.Add(TEXT("Freight pool"));
	if (!FreightPool.Num()) M.Lines.Add(TEXT("The pool is empty."));
	for (const auto& P : FreightPool)
		VAct29(M, FString::Printf(TEXT("Withdraw 1 %s (%d/%d)"), *P.Key, P.Value, StackCap),
			[this, It = P.Key] { FreightWithdraw(It, 1); });
	M.Lines.Add(TEXT("Deposit from pack"));
	bool bPack = false;
	if (const AAerondelPlayerState* PS = GetPS())
		if (PS->Inventory)
			for (const FItemStack& Sl : PS->Inventory->GetSlots())
			{
				if (Sl.IsEmpty() || !Sl.Item) continue;
				const FString Name = Sl.Item->DisplayName.ToString();
				if (Sl.Item->bUnique) continue;
				bPack = true;
				VAct29(M, FString::Printf(TEXT("Deposit %s ×%d"), *Name, Sl.Count), [this, Name] { FreightDeposit(Name, 1); });
			}
	if (!bPack) M.Lines.Add(TEXT("Your pack is empty."));
}
