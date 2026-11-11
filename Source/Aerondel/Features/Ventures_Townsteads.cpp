#include "Features/VenturesFeature.h"
#include "UI/AerondelPanels.h"
#include "Core/AerondelPlayerState.h"
#include "Core/PlayerStatsComponent.h"
#include "Combat/HealthComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Data/ItemDef.h"
#include "Features/StandingFeature.h"
#include "Rules/AerondelRules.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// Ventures group 12 (web adapt/townsteads.js): buy existing town houses as storage.
// A chest, a bed and display hooks. Not a farm, not a Hearthhold, not a Freeframe plot.
// Buying the deed does not train Construction; placing furniture does.

namespace VenturesStead
{
	struct FHouse { const TCHAR* Id; const TCHAR* Name; const TCHAR* Town; float X, Z; int32 Gold, Rep; const TCHAR* Faction; };
	static const FHouse Houses[] = {
		{ TEXT("lark-e"),  TEXT("East Larkhaven cottage"),        TEXT("Larkhaven"),  13.f,  -6.f, 1500,   0, TEXT("watch") },
		{ TEXT("lark-n"),  TEXT("North Larkhaven cottage"),       TEXT("Larkhaven"),  12.5f,  7.f, 2000,   0, TEXT("watch") },
		{ TEXT("lark-w"),  TEXT("West Larkhaven cottage"),        TEXT("Larkhaven"), -12.f,   5.5f,2000,   0, TEXT("watch") },
		{ TEXT("lark-s"),  TEXT("South Larkhaven cottage"),       TEXT("Larkhaven"), -14.f,  -7.f, 2000,   0, TEXT("watch") },
		{ TEXT("ember-a"), TEXT("Emberfold farmhouse"),           TEXT("Emberfold"),  52.f, -35.f, 3500, 100, TEXT("watch") },
		{ TEXT("ember-b"), TEXT("Emberfold cottage"),             TEXT("Emberfold"),  60.f, -22.f, 3000, 100, TEXT("watch") },
		{ TEXT("salt-a"),  TEXT("Saltreach fisherman's cottage"), TEXT("Saltreach"), -88.f,  67.f, 4000, 100, TEXT("mariners") },
		{ TEXT("high-a"),  TEXT("Highcrest townhouse"),           TEXT("Highcrest"), 110.f, -33.f, 6000, 100, TEXT("circle") },
		{ TEXT("white-e"), TEXT("East Whitecairn house"),         TEXT("Whitecairn"),-70.f, -72.f, 5500, 100, TEXT("lantern") },
		{ TEXT("white-w"), TEXT("West Whitecairn house"),         TEXT("Whitecairn"),-50.f, -72.f, 5500, 100, TEXT("lantern") },
	};
	static const FHouse* FindHouse(const FString& Id) { for (const FHouse& H : Houses) if (Id == H.Id) return &H; return nullptr; }
	static void VAct12(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
}
using namespace VenturesStead;

void UVenturesFeature::ResetStead() { SteadOwned.Reset(); SteadChests.Reset(); SteadFurn.Reset(); SteadBuildBusyMs = 0; }

void UVenturesFeature::SaveStead(FJsonObject& A) const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	PutStrArr(*S, TEXT("owned"), SteadOwned);
	TSharedPtr<FJsonObject> Chests = MakeShared<FJsonObject>();
	for (const auto& E : SteadChests)
	{
		TSharedPtr<FJsonObject> B = MakeShared<FJsonObject>();
		for (const auto& I : E.Value) if (I.Value > 0) B->SetNumberField(I.Key, I.Value);
		Chests->SetObjectField(E.Key, B);
	}
	S->SetObjectField(TEXT("chests"), Chests);
	TSharedPtr<FJsonObject> Furn = MakeShared<FJsonObject>();
	for (const auto& E : SteadFurn)
	{
		TSharedPtr<FJsonObject> F = MakeShared<FJsonObject>();
		F->SetBoolField(TEXT("chest"), E.Value.bChest); F->SetBoolField(TEXT("bed"), E.Value.bBed);
		PutStrArr(*F, TEXT("hooks"), E.Value.Hooks); Furn->SetObjectField(E.Key, F);
	}
	S->SetObjectField(TEXT("furn"), Furn); A.SetObjectField(TEXT("steads"), S);
}

void UVenturesFeature::LoadStead(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> S = JObj(A, TEXT("steads")); if (!S) return;
	for (const FString& Id : JStrArr(*S, TEXT("owned"))) if (FindHouse(Id) && SteadOwned.Num() < 12) SteadOwned.AddUnique(Id);
	if (TSharedPtr<FJsonObject> Chests = JObj(*S, TEXT("chests")))
		for (const auto& E : Chests->Values)
		{
			if (!FindHouse(FString(*E.Key)) || !E.Value.IsValid() || E.Value->Type != EJson::Object) continue;
			TMap<FString, int32> Bag;
			for (const auto& I : E.Value->AsObject()->Values)
				if (I.Value.IsValid() && I.Value->Type == EJson::Number && I.Value->AsNumber() > 0)
					Bag.Add(FString(*I.Key), FMath::Min(10000, (int32)I.Value->AsNumber()));
			if (Bag.Num()) SteadChests.Add(FString(*E.Key), Bag);
		}
	if (TSharedPtr<FJsonObject> Furn = JObj(*S, TEXT("furn")))
		for (const auto& E : Furn->Values)
		{
			if (!FindHouse(FString(*E.Key)) || !E.Value.IsValid() || E.Value->Type != EJson::Object) continue;
			const TSharedPtr<FJsonObject> F = E.Value->AsObject();
			FSteadFurn U; U.bChest = JBool(*F, TEXT("chest")); U.bBed = JBool(*F, TEXT("bed")); U.Hooks = JStrArr(*F, TEXT("hooks"));
			if (U.Hooks.Num() > 16) U.Hooks.SetNum(16);
			SteadFurn.Add(FString(*E.Key), U);
		}
}

FSteadFurn& UVenturesFeature::SteadFurnOf(const FString& Id)
{ if (FSteadFurn* F = SteadFurn.Find(Id)) return *F; return SteadFurn.Add(Id, FSteadFurn()); }
TMap<FString, int32>& UVenturesFeature::SteadChestOf(const FString& Id)
{ if (TMap<FString, int32>* B = SteadChests.Find(Id)) return *B; return SteadChests.Add(Id, TMap<FString, int32>()); }

int32 UVenturesFeature::SteadHookSlots() const
{
	const int32 L = SkillLevel(TEXT("Construction"));
	int32 N = 1; if (L >= 25) ++N; if (L >= 50) ++N; if (L >= 75) ++N; if (L >= 99) N += 2;
	N += FMath::Max(0, L - 99) / 100;
	if (const AAerondelPlayerState* PS = GetPS()) N += FMath::RoundToInt(UAerondelRules::Perk(PS, TEXT("construction"), TEXT("pieceCap")));
	return FMath::Clamp(N, 1, 12);
}
int32 UVenturesFeature::SteadBuildMs() const
{
	float Speed = 0.f;
	if (const AAerondelPlayerState* PS = GetPS()) Speed = UAerondelRules::Perk(PS, TEXT("construction"), TEXT("buildSpeed"));
	return FMath::Max(350, FMath::RoundToInt(1600.f / (1.f + Speed)));
}

const FString* UVenturesFeature::SteadHere() const
{
	FVector2D P; if (!PlayerWebPos(P)) return nullptr;
	const FHouse* Best = nullptr; float Bd = 3.4f;
	for (const FHouse& H : Houses) { const float D = FVector2D::Distance(P, FVector2D(H.X, H.Z)); if (D < Bd) { Bd = D; Best = &H; } }
	if (!Best) return nullptr;
	static FString Id; Id = Best->Id; return &Id;
}

bool UVenturesFeature::SteadBuy(const FString& Id)
{
	const FHouse* H = FindHouse(Id); if (!H) return false;
	if (SteadOwned.Contains(Id)) { Say(FString::Printf(TEXT("You already own the %s."), H->Name)); return false; }
	if (Coins() < H->Gold) { Say(FString::Printf(TEXT("The %s costs %d coins."), H->Name, H->Gold)); return false; }
	if (H->Rep > 0)
	{
		const UStandingFeature* St = Feature<UStandingFeature>();
		const int32 Rep = St ? St->RepOf(FName(H->Faction)) : 0;
		if (Rep < H->Rep) { Say(FString::Printf(TEXT("You need %d standing with that town to buy the %s."), H->Rep, H->Name)); return false; }
	}
	if (!TakeCoins(H->Gold)) return false;
	SteadOwned.Add(Id); SteadFurnOf(Id); SteadChestOf(Id);
	Say(FString::Printf(TEXT("You buy the %s as a Townstead. It is for storage — a chest, a bed and hooks — not a farm or a workshop."), H->Name));
	return true;
}

bool UVenturesFeature::SteadPlace(const FString& Id, const FString& Kind)
{
	const FHouse* H = FindHouse(Id);
	if (!H || !SteadOwned.Contains(Id)) { Say(TEXT("You do not own this house.")); return false; }
	FSteadFurn& F = SteadFurnOf(Id);
	const bool bChest = Kind == TEXT("chest"), bBed = Kind == TEXT("bed"), bHook = Kind == TEXT("hook");
	if (!bChest && !bBed && !bHook) return false;
	if ((bChest && F.bChest) || (bBed && F.bBed)) { Say(FString::Printf(TEXT("This house already has a %s."), *Kind)); return false; }
	if (bHook && F.Hooks.Num() >= SteadHookSlots()) { Say(FString::Printf(TEXT("Your Construction only supports %d hooks here."), SteadHookSlots())); return false; }
	if (SteadBuildBusyMs && NowMs() < SteadBuildBusyMs) { Say(TEXT("You are still setting the last piece.")); return false; }
	if (bChest && CountItem(TEXT("Logs")) < 5) { Say(TEXT("You need 5 Logs.")); return false; }
	if (bBed && CountItem(TEXT("Logs")) < 4) { Say(TEXT("You need 4 Logs.")); return false; }
	if (bHook && (CountItem(TEXT("Logs")) < 2 || CountItem(TEXT("Iron bar")) < 1)) { Say(TEXT("You need 2 Logs and 1 Iron bar.")); return false; }
	if (bChest) { TakeItem(TEXT("Logs"), 5); F.bChest = true; }
	else if (bBed) { TakeItem(TEXT("Logs"), 4); F.bBed = true; }
	else { TakeItem(TEXT("Logs"), 2); TakeItem(TEXT("Iron bar"), 1); F.Hooks.Add(FString()); }
	SteadBuildBusyMs = NowMs() + SteadBuildMs();
	float Bonus = 0.f;
	if (const AAerondelPlayerState* PS = GetPS())
		Bonus = UAerondelRules::Perk(PS, TEXT("construction"), TEXT("xpBonus")) + UAerondelRules::Perk(PS, TEXT("construction"), TEXT("buildSpeed")) * 0.25f;
	const int32 Xp = FMath::RoundToInt((bChest ? 45 : bBed ? 35 : 25) * (1.f + Bonus));
	GiveXp(TEXT("Construction"), (float)Xp);
	if (const AAerondelPlayerState* PS = GetPS())
		if (UAerondelRules::Perk(PS, TEXT("construction"), TEXT("xpBonus")) > 0.f && !bHook) GiveXp(TEXT("Crafting"), (float)FMath::RoundToInt(Xp * 0.15f));
	Say(FString::Printf(TEXT("You place a %s in the %s. (+%d Construction)"), *Kind, H->Name, Xp));
	return true;
}

bool UVenturesFeature::SteadRest(const FString& Id)
{
	const FHouse* H = FindHouse(Id);
	if (!H || !SteadOwned.Contains(Id)) return false;
	if (!SteadFurnOf(Id).bBed) { Say(TEXT("Place a bed first.")); return false; }
	AAerondelPlayerState* PS = GetPS(); if (!PS || !PS->Health) return false;
	const int32 Heal = FMath::Max(1, FMath::RoundToInt(PS->Health->GetMax() * 0.2f));
	PS->Health->Heal(Heal); if (PS->Stats) PS->Stats->RunEnergy = 100.f;
	Say(FString::Printf(TEXT("You rest in the %s. (+%d hitpoints, run energy restored.)"), H->Name, Heal));
	return true;
}

bool UVenturesFeature::SteadStore(const FString& Id, const FString& Item, int32 N)
{
	if (!SteadOwned.Contains(Id) || !SteadFurnOf(Id).bChest) { Say(TEXT("Place a chest first.")); return false; }
	if (N <= 0 || CountItem(Item) < N || !TakeItem(Item, N)) return false;
	SteadChestOf(Id).FindOrAdd(Item) += N; return true;
}
bool UVenturesFeature::SteadTake(const FString& Id, const FString& Item, int32 N)
{
	TMap<FString, int32>& Bag = SteadChestOf(Id);
	int32* Have = Bag.Find(Item); if (!Have || *Have < N) return false;
	*Have -= N; if (*Have <= 0) Bag.Remove(Item);
	GiveItem(Item, N); return true;
}

void UVenturesFeature::TabStead(FGPanelModel& M)
{
	const FString* Here = SteadHere();
	M.Title = FString::Printf(TEXT("Townsteads · Construction %d · %d hook slots"), SkillLevel(TEXT("Construction")), SteadHookSlots());
	M.Lines.Add(TEXT("Town houses are storage. The farm and Hearthhold are where you make things; a claimed wild plot is for freeform building later."));
	M.Lines.Add(TEXT("Buying a deed does not train Construction; placing the chest, bed and hooks does. Construction 1 is enough for the first furniture."));
	for (const FHouse& H : Houses)
	{
		const bool bOwn = SteadOwned.Contains(H.Id), bHere = Here && *Here == H.Id;
		const FString Need = H.Rep ? FString::Printf(TEXT(" and %d standing"), H.Rep) : FString();
		M.Lines.Add(FString::Printf(TEXT("%s — %s%s%s"), H.Name, H.Town, bHere ? TEXT(" · you are here") : TEXT(""),
			bOwn ? TEXT(" · owned") : *FString::Printf(TEXT(" · %d coins%s"), H.Gold, *Need)));
		const FString Id = H.Id;
		if (!bOwn) { VAct12(M, FString::Printf(TEXT("Buy %s (%d coins)"), H.Name, H.Gold), [this, Id] { SteadBuy(Id); }); continue; }
		FSteadFurn& F = SteadFurnOf(Id); TMap<FString, int32>& Bag = SteadChestOf(Id);
		FString Stored; for (const auto& I : Bag) { if (!Stored.IsEmpty()) Stored += TEXT(", "); Stored += FString::Printf(TEXT("%d %s"), I.Value, *I.Key); }
		M.Lines.Add(FString::Printf(TEXT("Chest %s · Bed %s · Hooks %d/%d%s"), F.bChest ? TEXT("placed") : TEXT("not placed"), F.bBed ? TEXT("placed") : TEXT("not placed"),
			F.Hooks.Num(), SteadHookSlots(), Stored.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" · stored: %s"), *Stored)));
		if (!F.bChest) VAct12(M, TEXT("Place chest (5 Logs)"), [this, Id] { SteadPlace(Id, TEXT("chest")); });
		if (!F.bBed) VAct12(M, TEXT("Place bed (4 Logs)"), [this, Id] { SteadPlace(Id, TEXT("bed")); });
		VAct12(M, TEXT("Add hook (2 Logs, 1 Iron bar)"), [this, Id] { SteadPlace(Id, TEXT("hook")); }, F.Hooks.Num() < SteadHookSlots());
		if (F.bBed) VAct12(M, TEXT("Rest"), [this, Id] { SteadRest(Id); }, true, true);
		if (F.bChest)
		{
			if (const AAerondelPlayerState* PS = GetPS()) if (PS->Inventory)
			{ int32 Shown = 0; for (const FItemStack& St : PS->Inventory->GetSlots()) { if (St.IsEmpty() || Shown >= 8) continue; const FString Item = St.Item->DisplayName.ToString(); VAct12(M, FString::Printf(TEXT("Store %s"), *Item), [this, Id, Item] { SteadStore(Id, Item, 1); }); ++Shown; } }
			for (const auto& I : Bag) { const FString Item = I.Key; VAct12(M, FString::Printf(TEXT("Take %s"), *Item), [this, Id, Item] { SteadTake(Id, Item, 1); }); }
		}
		if (F.Hooks.Num())
		{
			FString Hooks; for (int32 i = 0; i < F.Hooks.Num(); ++i) { if (i) Hooks += TEXT(" · "); Hooks += FString::Printf(TEXT("Hook %d: %s"), i + 1, F.Hooks[i].IsEmpty() ? TEXT("empty") : *F.Hooks[i]); }
			M.Lines.Add(Hooks);
		}
	}
}
