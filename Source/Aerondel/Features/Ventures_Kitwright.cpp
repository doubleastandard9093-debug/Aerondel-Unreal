#include "Features/VenturesFeature.h"
#include "UI/AerondelPanels.h"
#include "Core/AerondelPlayerState.h"
#include "Inventory/EquipmentComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Data/GameDatabase.h"
#include "Data/ItemDef.h"
#include "Data/AerondelId.h"
#include "Dom/JsonObject.h"

// Ventures group 28 (web adapt/kitwright.js): attach up to two named kits to a worn weapon or
// armour piece. Kits change how the piece behaves. They are not grafts, Facets, Ascent or a
// damage percent. Frontier uniques cannot take a kit. Removing a kit returns most of the parts.

namespace VenturesKits
{
	static constexpr int32 KitSlots = 2;
	struct FIn { const TCHAR* Item; int32 N; };
	struct FKit
	{
		const TCHAR* Id; const TCHAR* Name; const TCHAR* Slot; const TCHAR* Skill; int32 Level; int32 Xp;
		const TCHAR* Desc; bool bMelee; bool bRanged; bool bSeason;
		FIn Inputs[4]; FIn Refund[4];
	};
	static const FKit Kits[] = {
		{ TEXT("longhaft"), TEXT("Longhaft kit"), TEXT("weapon"), TEXT("Smithing"), 10, 40,
			TEXT("A little more melee reach, a little slower. Not a damage bonus."), true, false, false,
			{ { TEXT("Logs"), 3 }, { TEXT("Iron bar"), 1 } }, { { TEXT("Logs"), 2 }, { TEXT("Iron bar"), 1 } } },
		{ TEXT("hush"), TEXT("Hush-string kit"), TEXT("weapon"), TEXT("Fletching"), 10, 40,
			TEXT("A quieter setup and a little less ranged reach. Shadework can use the hush later."), false, true, false,
			{ { TEXT("Bowstring"), 1 }, { TEXT("Flax"), 3 } }, { { TEXT("Bowstring"), 1 }, { TEXT("Flax"), 2 } } },
		{ TEXT("lamp"), TEXT("Lamp-rim kit"), TEXT("armour"), TEXT("Crafting"), 10, 40,
			TEXT("A short cone of light. On Low this is a pip, not a new mesh or lamp draw."), false, false, false,
			{ { TEXT("Iron bar"), 1 }, { TEXT("Coal"), 2 } }, { { TEXT("Iron bar"), 1 }, { TEXT("Coal"), 1 } } },
		{ TEXT("collar"), TEXT("Padded collar kit"), TEXT("armour"), TEXT("Crafting"), 12, 50,
			TEXT("A little more player poise. Ordinary scratches still will not stagger you."), false, false, false,
			{ { TEXT("Soft leather"), 2 }, { TEXT("Wool"), 1 } }, { { TEXT("Soft leather"), 1 }, { TEXT("Wool"), 1 } } },
		{ TEXT("sight"), TEXT("Watch-sight kit"), TEXT("weapon"), TEXT("Crafting"), 15, 55,
			TEXT("Stillwatch focus fills a little faster. It does not add damage."), false, false, false,
			{ { TEXT("Iron bar"), 1 }, { TEXT("Clear essence"), 5 } }, { { TEXT("Iron bar"), 1 }, { TEXT("Clear essence"), 3 } } },
		{ TEXT("ashthread"), TEXT("Ash-thread kit"), TEXT("armour"), TEXT("Crafting"), 8, 30,
			TEXT("A little more player poise, less than a padded collar. A Seasonboard schematic. Not a damage bonus."), false, false, true,
			{ { TEXT("Soft leather"), 1 }, { TEXT("Wool"), 1 }, { TEXT("Clear essence"), 1 } }, { { TEXT("Soft leather"), 1 }, { TEXT("Wool"), 1 } } },
	};
	static const FKit* FindKit(const FString& Id)
	{
		for (const FKit& K : Kits) if (Id == K.Id) return &K;
		return nullptr;
	}
	static FString ItemName(const UItemDef* I) { return I ? I->DisplayName.ToString() : FString(); }
	static bool IsArmourSlot(EEquipSlot S) { return S == EEquipSlot::Head || S == EEquipSlot::Body || S == EEquipSlot::Legs || S == EEquipSlot::Shield; }
	static void VAct28(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
}
using namespace VenturesKits;

TArray<FString> UVenturesFeature::KitList(const FString& Item) const
{
	TArray<FString> Out;
	if (const FString* S = KitOn.Find(Item)) S->ParseIntoArray(Out, TEXT(","), true);
	if (Out.Num() > KitSlots) Out.SetNum(KitSlots);
	return Out;
}

bool UVenturesFeature::KitHas(const FString& KitId) const
{
	const AAerondelPlayerState* PS = GetPS();
	if (!PS || !PS->Equipment) return false;
	for (const auto& P : PS->Equipment->GetAll())
		if (P.Value && KitList(ItemName(P.Value)).Contains(KitId)) return true;
	return false;
}

bool UVenturesFeature::KitOnItem(const FString& Item, const FString& KitId) const { return KitList(Item).Contains(KitId); }

int32 UVenturesFeature::KitPoiseBonus() const
{
	return (KitHas(TEXT("collar")) ? 8 : 0) + (KitHas(TEXT("ashthread")) ? 4 : 0);
}

float UVenturesFeature::KitRangeBonus(bool bRanged) const
{
	float D = 0.f;
	if (!bRanged && KitHas(TEXT("longhaft"))) D += 0.5f;
	if (bRanged && KitHas(TEXT("hush"))) D -= 1.f;
	return D;
}

int32 UVenturesFeature::KitSpeedBonus(bool bRanged) const
{
	return (!bRanged && KitHas(TEXT("longhaft"))) ? 1 : 0;
}

FString UVenturesFeature::KitExamine(const FString& Item, const FString& Text) const
{
	const TArray<FString> Ids = KitList(Item);
	if (!Ids.Num()) return Text;
	FString Extra = TEXT(" Kits: ");
	for (int32 i = 0; i < Ids.Num(); ++i)
	{
		if (i) Extra += TEXT(", ");
		if (const FKit* K = FindKit(Ids[i])) Extra += K->Name; else Extra += Ids[i];
	}
	Extra += TEXT(".");
	return Text + Extra;
}

void UVenturesFeature::ResetKits()
{
	KitOn.Reset();
	KitOwned.Reset();
	KitSel.Reset();
}

void UVenturesFeature::SaveKits(FJsonObject& A) const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	TSharedRef<FJsonObject> On = MakeShared<FJsonObject>();
	for (const auto& P : KitOn)
	{
		TArray<FString> Ids; P.Value.ParseIntoArray(Ids, TEXT(","), true);
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const FString& Id : Ids) Arr.Add(MakeShared<FJsonValueString>(Id));
		On->SetArrayField(P.Key, Arr);
	}
	S->SetObjectField(TEXT("on"), On);
	PutStrArr(*S, TEXT("owned"), KitOwned);
	A.SetObjectField(TEXT("kits"), S);
}

void UVenturesFeature::LoadKits(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> S = JObj(A, TEXT("kits"));
	if (!S) return;
	KitOwned = JStrArr(*S, TEXT("owned"));
	if (const TSharedPtr<FJsonObject> On = JObj(*S, TEXT("on")))
		for (const auto& P : On->Values)
		{
			TArray<FString> Ids;
			const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
			if (P.Value.IsValid() && P.Value->TryGetArray(Arr) && Arr)
				for (const auto& V : *Arr) if (V.IsValid() && V->Type == EJson::String) Ids.AddUnique(V->AsString());
			if (Ids.Num() > KitSlots) Ids.SetNum(KitSlots);
			if (Ids.Num()) KitOn.Add(P.Key, FString::Join(Ids, TEXT(",")));
		}
}

FString UVenturesFeature::KitCanTake(const FString& Item, const FString& KitId) const
{
	const FKit* K = FindKit(KitId);
	if (Item.IsEmpty() || !K) return TEXT("Unknown kit.");
	const UGameDatabase* Db = UGameDatabase::Get();
	const UItemDef* I = Db ? Db->FindItem(AerondelId::FromName(Item)) : nullptr;
	if (!I) return TEXT("Unknown item.");
	if (I->bUnique) return TEXT("Frontier uniques keep their identity and cannot take a kit.");
	const bool bWeapon = I->EquipSlot == EEquipSlot::Weapon || I->ItemType == FName(TEXT("weapon"));
	const bool bArmour = IsArmourSlot(I->EquipSlot);
	if (FCString::Strcmp(K->Slot, TEXT("weapon")) == 0 && !bWeapon) return TEXT("That kit only fits a weapon.");
	if (FCString::Strcmp(K->Slot, TEXT("armour")) == 0 && !bArmour) return TEXT("That kit only fits armour.");
	if (K->bMelee && I->bRangedWeapon) return TEXT("A longhaft needs a melee weapon.");
	if (K->bRanged && !I->bRangedWeapon) return TEXT("A hush-string needs a ranged weapon.");
	if (KitOnItem(Item, KitId)) return TEXT("That kit is already on this item.");
	if (KitList(Item).Num() >= KitSlots) return TEXT("That item already has two kits.");
	return FString();
}

bool UVenturesFeature::KitCraft(const FString& KitId)
{
	const FKit* K = FindKit(KitId); if (!K) return false;
	if (K->bSeason && !ScoreHas(TEXT("kit-ashthread"))) { Say(TEXT("The Ash-thread kit is a Seasonboard schematic. Claim that tier first.")); return false; }
	if (SkillLevel(K->Skill) < K->Level) { Say(FString::Printf(TEXT("You need %s %d to craft a %s."), K->Skill, K->Level, K->Name)); return false; }
	for (const FIn& In : K->Inputs) if (In.Item && CountItem(In.Item) < In.N) { Say(FString::Printf(TEXT("You need %d %s to craft a %s."), In.N, In.Item, K->Name)); return false; }
	for (const FIn& In : K->Inputs) if (In.Item) TakeItem(In.Item, In.N);
	GiveItem(K->Name, 1);
	KitOwned.AddUnique(KitId);
	GiveXp(K->Skill, (float)K->Xp);
	Say(FString::Printf(TEXT("You craft a %s."), K->Name));
	return true;
}

bool UVenturesFeature::KitAttach(const FString& Item, const FString& KitId)
{
	const FKit* K = FindKit(KitId); if (!K) return false;
	bool bWorn = false;
	if (const AAerondelPlayerState* PS = GetPS())
		if (PS->Equipment)
			for (const auto& P : PS->Equipment->GetAll()) if (P.Value && ItemName(P.Value) == Item) { bWorn = true; break; }
	if (!bWorn) { Say(TEXT("Wear the item first.")); return false; }
	if (CountItem(K->Name) < 1) { Say(FString::Printf(TEXT("You do not have a %s."), K->Name)); return false; }
	const FString Why = KitCanTake(Item, KitId);
	if (!Why.IsEmpty()) { Say(Why); return false; }
	if (!TakeItem(K->Name, 1)) return false;
	TArray<FString> Ids = KitList(Item);
	Ids.Add(KitId);
	KitOn.Add(Item, FString::Join(Ids, TEXT(",")));
	Say(FString::Printf(TEXT("You fit a %s to your %s. %s"), K->Name, *Item, K->Desc));
	return true;
}

bool UVenturesFeature::KitRemove(const FString& Item, int32 Index)
{
	TArray<FString> Ids = KitList(Item);
	if (!Ids.IsValidIndex(Index)) return false;
	const FString Id = Ids[Index];
	Ids.RemoveAt(Index);
	if (Ids.Num()) KitOn.Add(Item, FString::Join(Ids, TEXT(","))); else KitOn.Remove(Item);
	const FKit* K = FindKit(Id); if (!K) return true;
	GiveItem(K->Name, 1);
	for (const FIn& In : K->Refund) if (In.Item) GiveItem(In.Item, In.N);
	Say(FString::Printf(TEXT("You remove the %s. Most of the parts come back."), K->Name));
	return true;
}

void UVenturesFeature::TabKits(FGPanelModel& M)
{
	M.Title = TEXT("Kitwright");
	M.Lines.Add(TEXT("Up to two kits on a worn weapon or armour piece. Kits change how the piece behaves. They are not grafts or Facets, and they do not add a damage percent. Frontier uniques cannot take a kit."));
	M.Lines.Add(TEXT("Craft"));
	for (const FKit& K : Kits)
	{
		if (K.bSeason && !ScoreHas(TEXT("kit-ashthread"))) continue;
		FString Have;
		for (const FIn& In : K.Inputs) if (In.Item) { if (!Have.IsEmpty()) Have += TEXT(", "); Have += FString::Printf(TEXT("%d %s"), In.N, In.Item); }
		M.Lines.Add(FString::Printf(TEXT("%s — %s %s %d. %s"), K.Name, K.Desc, K.Skill, K.Level, *Have));
		VAct28(M, FString::Printf(TEXT("Craft %s"), K.Name), [this, Id = FString(K.Id)] { KitCraft(Id); });
	}
	M.Lines.Add(TEXT("Held kits"));
	bool bHeld = false;
	for (const FKit& K : Kits)
	{
		const int32 N = CountItem(K.Name);
		if (N <= 0) continue;
		bHeld = true;
		const bool bOn = KitSel == K.Id;
		VAct28(M, FString::Printf(TEXT("%s%s ×%d"), bOn ? TEXT("● ") : TEXT(""), K.Name, N), [this, Id = FString(K.Id)] { KitSel = (KitSel == Id) ? FString() : Id; });
	}
	if (!bHeld) M.Lines.Add(TEXT("Craft a kit first."));
	M.Lines.Add(TEXT("Worn gear"));
	bool bWorn = false;
	if (const AAerondelPlayerState* PS = GetPS())
		if (PS->Equipment)
			for (const auto& P : PS->Equipment->GetAll())
			{
				if (!P.Value) continue;
				bWorn = true;
				const FString It = ItemName(P.Value);
				const TArray<FString> Ids = KitList(It);
				M.Lines.Add(FString::Printf(TEXT("%s  %d/%d%s"), *It, Ids.Num(), KitSlots, *KitExamine(It, FString())));
				for (int32 i = 0; i < Ids.Num(); ++i)
				{
					const FKit* K = FindKit(Ids[i]);
					VAct28(M, FString::Printf(TEXT("Remove %s from %s"), K ? K->Name : *Ids[i], *It), [this, It, i] { KitRemove(It, i); });
				}
				if (!KitSel.IsEmpty() && Ids.Num() < KitSlots)
				{
					const FKit* K = FindKit(KitSel);
					VAct28(M, FString::Printf(TEXT("Fit %s to %s"), K ? K->Name : *KitSel, *It), [this, It] { KitAttach(It, KitSel); if (const FKit* KK = FindKit(KitSel)) if (CountItem(KK->Name) < 1) KitSel.Reset(); });
				}
			}
	if (!bWorn) M.Lines.Add(TEXT("Wear something first."));
}
