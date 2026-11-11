#include "Features/VenturesFeature.h"
#include "Features/StandingFeature.h"
#include "UI/AerondelPanels.h"
#include "Core/AerondelPlayerState.h"
#include "Inventory/InventoryComponent.h"
#include "Combat/AerondelMob.h"
#include "Data/ItemDef.h"
#include "Dom/JsonObject.h"

// Ventures group 34 (web adapt/wardfine.js): a failed pickpocket in a town raises a fine, then a
// guard stop: pay, wait at the keep, or resist. Unique and quest items are never taken. Larkhaven
// is always solvable (pay, wait, or walk to the bank). This is not a prison-crawl. A Wary banner
// may make a town quicker to fine, but the fine stays payable. The bank and the Saga stay open.

namespace VenturesKeep
{
	static constexpr float TownR = 16.f, FleeR = 28.f;
	static constexpr int64 WaitMs = 1400, UnwelcomeMs = 10ll * 60 * 1000;
	struct FKeep { const TCHAR* Town; float X, Z, CX, CZ; };
	static const FKeep Keeps[] = {
		{ TEXT("Larkhaven"),    0.f,   0.f,  -6.f,  -3.5f },
		{ TEXT("Emberfold"),   50.f, -24.f,  48.f, -23.f },
		{ TEXT("Saltreach"),  -80.f,  60.f, -80.f,  53.f },
		{ TEXT("Highcrest"),  120.f, -40.f, 124.f, -25.f },
		{ TEXT("Whitecairn"), -60.f, -80.f, -56.f, -73.f },
	};
	struct FAmt { const TCHAR* Name; int32 Gold; };
	static const FAmt FineOf[] = {
		{ TEXT("Man"), 20 }, { TEXT("Woman"), 20 }, { TEXT("Farmer"), 25 }, { TEXT("Sailor"), 35 },
		{ TEXT("Warrior"), 40 }, { TEXT("Guard"), 60 }, { TEXT("Knight"), 80 }, { TEXT("Paladin"), 90 }, { TEXT("Hero"), 120 }
	};
	static const FKeep* FindKeep(const FString& Town)
	{
		for (const FKeep& K : Keeps) if (Town == K.Town) return &K;
		return nullptr;
	}
	static int32 AmountFor(const FString& Type)
	{
		for (const FAmt& A : FineOf) if (Type.Equals(A.Name, ESearchCase::IgnoreCase)) return A.Gold;
		return 25;
	}
	static void VAct34(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
}
using namespace VenturesKeep;

FString UVenturesFeature::TownAt(float X, float Z) const
{
	const FKeep* Best = nullptr;
	float Bd = TownR;
	for (const FKeep& K : Keeps)
	{
		const float D = FMath::Hypot(X - K.X, Z - K.Z);
		if (D < Bd) { Bd = D; Best = &K; }
	}
	return Best ? FString(Best->Town) : FString();
}

int32 UVenturesFeature::FineOf(const FString& Town) const { return KeepFines.FindRef(Town); }
int32 UVenturesFeature::FineTotal() const { int32 T = 0; for (const auto& P : KeepFines) T += P.Value; return T; }
bool UVenturesFeature::FineUnwelcome(const FString& Town) const { return KeepUnwelcome.FindRef(Town) > NowMs(); }

bool UVenturesFeature::FineProtected(const UItemDef* Item) const
{
	if (!Item) return true;
	if (Item->bUnique || Item->bQuestCape) return true;
	const FString T = Item->ItemType.ToString();
	return T.Equals(TEXT("quest"), ESearchCase::IgnoreCase) || T.Equals(TEXT("questItem"), ESearchCase::IgnoreCase);
}

void UVenturesFeature::ResetKeep()
{
	KeepFines.Reset(); KeepHeat.Reset(); KeepUnwelcome.Reset(); KeepHeld.Reset();
	KeepHeldTown.Reset(); FineStopTown.Reset(); FineResistTown.Reset();
	KeepJailUntil = 0; bFineStop = false; FineResistAt = FVector2D::ZeroVector;
	if (AAerondelMob* G = FineGuard.Get()) G->Destroy();
	FineGuard.Reset();
}

void UVenturesFeature::SaveKeep(FJsonObject& A) const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	PutIntMap(*S, TEXT("fines"), KeepFines);
	PutIntMap(*S, TEXT("heat"), KeepHeat);
	TSharedRef<FJsonObject> U = MakeShared<FJsonObject>();
	for (const auto& P : KeepUnwelcome) U->SetNumberField(P.Key, (double)P.Value);
	S->SetObjectField(TEXT("unwelcome"), U);
	PutIntMap(*S, TEXT("held"), KeepHeld);
	S->SetStringField(TEXT("heldTown"), KeepHeldTown);
	S->SetNumberField(TEXT("jailUntil"), KeepJailUntil);
	A.SetObjectField(TEXT("keep"), S);
}

void UVenturesFeature::LoadKeep(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> S = JObj(A, TEXT("keep"));
	if (!S) return;
	KeepFines = JIntMap(*S, TEXT("fines"));
	KeepHeat = JIntMap(*S, TEXT("heat"));
	KeepHeld = JIntMap(*S, TEXT("held"));
	KeepHeldTown = JStr(*S, TEXT("heldTown"));
	KeepJailUntil = JInt(*S, TEXT("jailUntil"));
	if (const TSharedPtr<FJsonObject> U = JObj(*S, TEXT("unwelcome")))
		for (const auto& P : U->Values) KeepUnwelcome.Add(P.Key, JInt(*U, *P.Key));
	// A mid-wait save should wake on load, like the web onReady path.
	if (KeepJailUntil) FineWake();
}

bool UVenturesFeature::FineOnFail(const AActor* At, const FString& TargetType)
{
	FVector2D P;
	if (At)
	{
		const FVector L = At->GetActorLocation();
		P = FVector2D(L.X / 100.f, L.Y / 100.f);
	}
	else if (!PlayerWebPos(P)) return false;
	const FString Town = TownAt(P.X, P.Y);
	if (Town.IsEmpty() || !FindKeep(Town)) return false;
	int32 Amt = AmountFor(TargetType);
	bool bWary = false;
	if (const UStandingFeature* St = Feature<UStandingFeature>())
		bWary = BannerWary(St->FactionForTown(Town));
	if (bWary) Amt = FMath::CeilToInt((float)Amt * 1.25f);
	KeepFines.FindOrAdd(Town) += Amt;
	KeepHeat.FindOrAdd(Town) += bWary ? 2 : 1;
	FineStopTown = Town;
	bFineStop = true;
	Say(FString::Printf(TEXT("A guard stops you in %s. The fine is %d coins — pay, wait at the keep, or resist. This is no longer only a stun. Larkhaven is always solvable."), *Town, KeepFines[Town]));
	return true;
}

bool UVenturesFeature::FinePay(const FString& InTown)
{
	const FString Town = InTown.IsEmpty() ? FineStopTown : InTown;
	const int32 Amt = FineOf(Town);
	if (!Amt) { Say(TEXT("You owe no fine there.")); return false; }
	if (!TakeCoins(Amt)) { Say(TEXT("You do not have enough coins on you. The bank is still open in Larkhaven.")); return false; }
	KeepFines.Remove(Town);
	KeepUnwelcome.Remove(Town);
	if (FineStopTown == Town) { bFineStop = false; FineStopTown.Reset(); }
	Say(FString::Printf(TEXT("You pay the %d coin fine in %s. The mark is cleared."), Amt, *Town));
	return true;
}

bool UVenturesFeature::FineWake()
{
	const FString Town = !KeepHeldTown.IsEmpty() ? KeepHeldTown : (FineStopTown.IsEmpty() ? TEXT("Larkhaven") : FineStopTown);
	if (const FKeep* K = FindKeep(Town)) TeleportPlayer(FVector2D(K->CX, K->CZ));
	for (const auto& P : KeepHeld) GiveItem(P.Key, P.Value);
	KeepHeld.Reset();
	KeepHeldTown.Reset();
	KeepJailUntil = 0;
	KeepFines.Remove(Town);
	KeepUnwelcome.Remove(Town);
	bFineStop = false;
	FineStopTown.Reset();
	Say(FString::Printf(TEXT("You wake at the %s keep door. Ordinary items are returned. Unique and quest items were never taken."), *Town));
	return true;
}

bool UVenturesFeature::FineWait(const FString& InTown)
{
	const FString Town = InTown.IsEmpty() ? FineStopTown : InTown;
	if (!FindKeep(Town)) return false;
	KeepHeld.Reset();
	TArray<TPair<FString, int32>> Take;
	if (AAerondelPlayerState* PS = GetPS())
		if (PS->Inventory)
			for (const FItemStack& Sl : PS->Inventory->GetSlots())
			{
				if (Sl.IsEmpty() || !Sl.Item || FineProtected(Sl.Item)) continue;
				Take.Add(TPair<FString, int32>(Sl.Item->DisplayName.ToString(), Sl.Count));
			}
	for (const TPair<FString, int32>& It : Take)
		if (TakeItem(It.Key, It.Value)) KeepHeld.FindOrAdd(It.Key) += It.Value;
	KeepHeldTown = Town;
	KeepJailUntil = NowMs() + WaitMs;
	bFineStop = false;
	Say(TEXT("You agree to wait at the keep. Ordinary items are held and will be returned. Unique and quest items stay with you."));
	return true;
}

bool UVenturesFeature::FineResist(const FString& InTown)
{
	const FString Town = InTown.IsEmpty() ? FineStopTown : InTown;
	if (!FindKeep(Town)) return false;
	const int32 Extra = FMath::Max(40, FMath::RoundToInt((float)FineOf(Town) * 0.5f));
	KeepFines.FindOrAdd(Town) += Extra;
	bFineStop = false;
	FVector2D P;
	if (!PlayerWebPos(P)) if (const FKeep* K = FindKeep(Town)) P = FVector2D(K->CX, K->CZ);
	if (AAerondelMob* G = FineGuard.Get()) G->Destroy();
	AAerondelMob* G = SpawnTunedMob(TEXT("Guard"), P + FVector2D(2.f, 1.f), TEXT("Town guard"), 200, 20, 20, 20, 20, 1.f, false);
	FineGuard = G;
	FineResistTown = Town;
	FineResistAt = P;
	Say(FString::Printf(TEXT("You resist! A town guard steps in, and the fine in %s rises to %d. Fleeing will leave you unwelcome until you pay or time passes."), *Town, FineOf(Town)));
	return true;
}

bool UVenturesFeature::FineFlee(const FString& InTown)
{
	const FString Town = InTown.IsEmpty() ? (FineResistTown.IsEmpty() ? FineStopTown : FineResistTown) : InTown;
	if (!FindKeep(Town)) return false;
	KeepUnwelcome.Add(Town, NowMs() + UnwelcomeMs);
	if (AAerondelMob* G = FineGuard.Get()) G->Destroy();
	FineGuard.Reset();
	FineResistTown.Reset();
	Say(FString::Printf(TEXT("You flee %s. You are unwelcome there until the fine is paid, or a little time passes. Larkhaven's bank stays open."), *Town));
	return true;
}

void UVenturesFeature::TickKeep()
{
	if (KeepJailUntil && NowMs() >= KeepJailUntil) FineWake();
	if (!FineResistTown.IsEmpty())
	{
		FVector2D P;
		if (PlayerWebPos(P) && FVector2D::Distance(P, FineResistAt) > FleeR) FineFlee(FineResistTown);
	}
	const int64 Now = NowMs();
	for (auto It = KeepUnwelcome.CreateIterator(); It; ++It) if (It.Value() && It.Value() < Now) It.RemoveCurrent();
}

void UVenturesFeature::DiedKeep(AAerondelMob* Mob, bool bByPlayer)
{
	if (!Mob || FineGuard.Get() != Mob) return;
	FineGuard.Reset();
	FineResistTown.Reset();
	Say(TEXT("The guard falls. The fine still stands — pay it at Ventures → Wardfine, or at the keep."));
}

void UVenturesFeature::TabKeep(FGPanelModel& M)
{
	M.Title = TEXT("Wardfine");
	M.Lines.Add(TEXT("A failed pickpocket in a town raises a fine. A guard will stop you: pay, wait at the keep, or resist. Unique and quest items are never taken. Larkhaven is always solvable. This is not a prison-crawl. The bank and the Saga stay open."));
	if (bFineStop && FindKeep(FineStopTown))
	{
		const int32 Amt = FineOf(FineStopTown);
		M.Lines.Add(FString::Printf(TEXT("A guard stops you in %s. The fine is %d coins."), *FineStopTown, Amt));
		VAct34(M, FString::Printf(TEXT("Pay %dc"), Amt), [this] { FinePay(FineStopTown); }, Coins() >= Amt);
		VAct34(M, TEXT("Wait at the keep"), [this] { FineWait(FineStopTown); });
		VAct34(M, TEXT("Resist"), [this] { FineResist(FineStopTown); });
		if (FineStopTown == TEXT("Larkhaven"))
			VAct34(M, TEXT("Go to the bank"), [this] { bFineStop = false; Say(TEXT("The fine still stands. The bank is open.")); });
		if (Coins() < Amt) M.Lines.Add(FineStopTown == TEXT("Larkhaven")
			? TEXT("You do not have enough coins on you. Wait, or go to the bank.")
			: TEXT("You do not have enough coins on you. Wait, or come back to pay."));
	}
	M.Lines.Add(FString::Printf(TEXT("Total owed: %d coins."), FineTotal()));
	for (const FKeep& K : Keeps)
	{
		const int32 F = FineOf(K.Town);
		const bool bU = FineUnwelcome(K.Town);
		M.Lines.Add(FString::Printf(TEXT("%s — %s%s"), K.Town, F ? *FString::Printf(TEXT("fine %dc"), F) : TEXT("clear"), bU ? TEXT("  · unwelcome") : TEXT("")));
		if (F) VAct34(M, FString::Printf(TEXT("Pay %s (%dc)"), K.Town, F), [this, T = FString(K.Town)] { FinePay(T); }, Coins() >= F);
	}
}
