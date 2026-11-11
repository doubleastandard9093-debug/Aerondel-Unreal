#include "Features/VenturesFeature.h"
#include "UI/AerondelPanels.h"
#include "Core/AerondelPlayerState.h"
#include "Inventory/EquipmentComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Combat/AerondelMob.h"
#include "Data/GameDatabase.h"
#include "Data/ItemDef.h"
#include "Data/AerondelId.h"
#include "Dom/JsonObject.h"

// Ventures group 32 (web adapt/veilcraft.js): unmake a non-unique kit-bearing item to learn its
// named effect — the item is destroyed, the lesson stays. Bind that lesson onto a plain weapon
// or armour with a Skywyrm soul (or a lesser bright soul) and 2 Clear essence. One binding per
// item. Unbind for 25 coins; the lesson stays. Not a Facet, not a graft, not a kit, and not a
// damage percent. Frontier uniques cannot be unmade. Veilcraft owns the soul pouch (veil.souls);
// wyrms.souls is kept in sync.

namespace VenturesVeil
{
	static constexpr int32 SoulCap = 8, BindEssence = 2, UnbindGold = 25;
	struct FFx { const TCHAR* Id; const TCHAR* Name; const TCHAR* Kit; const TCHAR* Desc; };
	static const FFx Effects[] = {
		{ TEXT("frost"), TEXT("Frost bind"), TEXT("longhaft"), TEXT("A brief chill on a hit that delays the foe a little. Not a damage percent.") },
		{ TEXT("stun"),  TEXT("Longer hitch"), TEXT("collar"), TEXT("A slightly longer stun hitch on a hit, with a cooldown. Bosses feel it less.") },
		{ TEXT("night"), TEXT("Night-sight"), TEXT("sight"), TEXT("You read worn gear and dim ground a little more clearly.") },
		{ TEXT("hush"),  TEXT("Quiet step"), TEXT("hush"), TEXT("A quieter step. Shadework can use this later; it is not a sneak sim.") },
		{ TEXT("lamp"),  TEXT("Veil lamp"), TEXT("lamp"), TEXT("A short cone of light, like a lamp-rim, with no extra mesh on Low.") },
	};
	static const TCHAR* LesserBosses[] = { TEXT("Cinder Warden"), TEXT("Thornback Prowler"), TEXT("Peak Warlord") };
	static const FFx* FindFx(const FString& Id) { for (const FFx& E : Effects) if (Id == E.Id) return &E; return nullptr; }
	static const TCHAR* EffectForKit(const FString& Kit)
	{
		for (const FFx& E : Effects) if (Kit == E.Kit) return E.Id;
		return nullptr;
	}
	static FString ItemName(const UItemDef* I) { return I ? I->DisplayName.ToString() : FString(); }
	static bool IsGear(const UItemDef* I)
	{
		if (!I) return false;
		if (I->EquipSlot == EEquipSlot::Weapon || I->ItemType == FName(TEXT("weapon"))) return true;
		return I->EquipSlot == EEquipSlot::Head || I->EquipSlot == EEquipSlot::Body || I->EquipSlot == EEquipSlot::Legs || I->EquipSlot == EEquipSlot::Shield;
	}
	static bool IsUnique(const FString& Item)
	{
		const UGameDatabase* Db = UGameDatabase::Get();
		const UItemDef* I = Db ? Db->FindItem(AerondelId::FromName(Item)) : nullptr;
		return I && I->bUnique;
	}
	static void VAct32(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
}
using namespace VenturesVeil;

void UVenturesFeature::SyncSoulPouch()
{
	VeilSoulCount = FMath::Clamp(VeilSoulCount, 0, SoulCap);
	WyrmSoulCount = VeilSoulCount;
}

void UVenturesFeature::ResetVeil()
{
	VeilSoulCount = VeilLesser = 0;
	VeilKnown.Reset();
	VeilOn.Reset();
	VeilSelItem.Reset();
	VeilSelFx.Reset();
	VeilFrostTick = VeilStunTick = -999;
	SyncSoulPouch();
}

void UVenturesFeature::SaveVeil(FJsonObject& A) const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	PutStrArr(*S, TEXT("known"), VeilKnown);
	S->SetNumberField(TEXT("souls"), VeilSoulCount);
	S->SetNumberField(TEXT("lesser"), VeilLesser);
	TSharedRef<FJsonObject> On = MakeShared<FJsonObject>();
	for (const auto& P : VeilOn) On->SetStringField(P.Key, P.Value);
	S->SetObjectField(TEXT("on"), On);
	A.SetObjectField(TEXT("veil"), S);
}

void UVenturesFeature::LoadVeil(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> S = JObj(A, TEXT("veil"));
	if (S)
	{
		VeilKnown = JStrArr(*S, TEXT("known"));
		VeilSoulCount = FMath::Clamp((int32)JInt(*S, TEXT("souls")), 0, SoulCap);
		VeilLesser = FMath::Clamp((int32)JInt(*S, TEXT("lesser")), 0, SoulCap);
		if (const TSharedPtr<FJsonObject> On = JObj(*S, TEXT("on")))
			for (const auto& P : On->Values)
				if (P.Value.IsValid() && P.Value->Type == EJson::String)
					if (FindFx(P.Value->AsString())) VeilOn.Add(P.Key, P.Value->AsString());
	}
	// Skywyrms may already have loaded a pouch; Veilcraft owns it and takes the higher count.
	VeilSoulCount = FMath::Clamp(FMath::Max(VeilSoulCount, WyrmSoulCount), 0, SoulCap);
	SyncSoulPouch();
}

FString UVenturesFeature::VeilOnItem(const FString& Item) const
{
	if (const FString* Id = VeilOn.Find(Item)) return *Id;
	return FString();
}

bool UVenturesFeature::VeilWorn(const FString& EffectId) const
{
	const AAerondelPlayerState* PS = GetPS();
	if (!PS || !PS->Equipment) return false;
	for (const auto& P : PS->Equipment->GetAll())
		if (P.Value && VeilOnItem(ItemName(P.Value)) == EffectId) return true;
	return false;
}

FString UVenturesFeature::VeilLessonFrom(const FString& Item) const
{
	if (const FString Bound = VeilOnItem(Item); !Bound.IsEmpty()) return Bound;
	for (const FString& Kit : KitList(Item))
		if (const TCHAR* Id = EffectForKit(Kit)) return Id;
	return FString();
}

bool UVenturesFeature::VeilUnmake(const FString& Item)
{
	if (Item.IsEmpty()) { Say(TEXT("There is nothing to unmake.")); return false; }
	if (IsUnique(Item)) { Say(TEXT("Frontier uniques and relics keep their identity and cannot be unmade.")); return false; }
	bool bWorn = false;
	EEquipSlot WornSlot = EEquipSlot::None;
	AAerondelPlayerState* PS = GetPS();
	if (PS && PS->Equipment)
		for (const auto& P : PS->Equipment->GetAll())
			if (P.Value && ItemName(P.Value) == Item) { bWorn = true; WornSlot = P.Key; break; }
	if (CountItem(Item) < 1 && !bWorn) { Say(TEXT("You do not have that item.")); return false; }
	const FString Lesson = VeilLessonFrom(Item);
	if (Lesson.IsEmpty()) { Say(TEXT("That item has no kit and no bound veil to learn from.")); return false; }
	if (bWorn && PS && PS->Equipment) PS->Equipment->Unequip(WornSlot);
	if (CountItem(Item) >= 1) TakeItem(Item, 1);
	KitOn.Remove(Item);
	VeilOn.Remove(Item);
	VeilKnown.AddUnique(Lesson);
	const FFx* E = FindFx(Lesson);
	Say(FString::Printf(TEXT("The %s comes apart. You keep the lesson: %s. The item is gone."), *Item, E ? E->Name : *Lesson));
	return true;
}

bool UVenturesFeature::VeilBind(const FString& Item, const FString& EffectId)
{
	const FFx* E = FindFx(EffectId); if (!E) return false;
	if (!VeilKnown.Contains(EffectId)) { Say(TEXT("You have not learned that lesson yet.")); return false; }
	const UGameDatabase* Db = UGameDatabase::Get();
	const UItemDef* I = Db ? Db->FindItem(AerondelId::FromName(Item)) : nullptr;
	if (!I || !IsGear(I)) { Say(TEXT("Bind onto a plain weapon or armour piece.")); return false; }
	if (I->bUnique) { Say(TEXT("Frontier uniques cannot take a veil binding.")); return false; }
	if (!VeilOnItem(Item).IsEmpty()) { Say(TEXT("That item already has a bound effect. Unbind it first.")); return false; }
	bool bWorn = false;
	if (const AAerondelPlayerState* PS = GetPS())
		if (PS->Equipment)
			for (const auto& P : PS->Equipment->GetAll()) if (P.Value && ItemName(P.Value) == Item) { bWorn = true; break; }
	if (CountItem(Item) < 1 && !bWorn) { Say(TEXT("You do not have that item.")); return false; }
	if (CountItem(TEXT("Clear essence")) < BindEssence) { Say(FString::Printf(TEXT("You need %d Clear essence to bind a veil."), BindEssence)); return false; }
	FString Paid;
	if (WyrmSpendSoul(1)) Paid = TEXT("soul");
	else if (VeilLesser >= 1) { --VeilLesser; Paid = TEXT("lesser"); }
	if (Paid.IsEmpty()) { Say(TEXT("You need a Skywyrm soul, or a lesser bright soul, to bind.")); return false; }
	if (!TakeItem(TEXT("Clear essence"), BindEssence))
	{
		if (Paid == TEXT("soul")) WyrmGrantSoul(1); else ++VeilLesser;
		return false;
	}
	VeilOn.Add(Item, EffectId);
	Say(FString::Printf(TEXT("You bind %s onto your %s. %s"), E->Name, *Item, E->Desc));
	return true;
}

bool UVenturesFeature::VeilUnbind(const FString& Item)
{
	const FString Id = VeilOnItem(Item);
	if (Id.IsEmpty()) { Say(TEXT("Nothing is bound to that item.")); return false; }
	if (!TakeCoins(UnbindGold)) { Say(FString::Printf(TEXT("Unbinding costs %d coins. The lesson stays known."), UnbindGold)); return false; }
	VeilOn.Remove(Item);
	const FFx* E = FindFx(Id);
	Say(FString::Printf(TEXT("You unbind %s from the %s. The lesson stays with you."), E ? E->Name : *Id, *Item));
	return true;
}

int32 UVenturesFeature::HitVeil(AAerondelMob* Mob, int32 Damage)
{
	if (!Mob || Damage <= 0) return Damage; // never a damage percent
	const bool bBoss = IsBossy(Mob);
	if (VeilWorn(TEXT("frost")) && TickN - VeilFrostTick >= 8)
	{
		VeilFrostTick = TickN;
		Mob->Challenge(bBoss ? 1 : 2, 0);
	}
	if (VeilWorn(TEXT("stun")) && TickN - VeilStunTick >= 20)
	{
		VeilStunTick = TickN;
		Mob->Stun(bBoss ? 3 : 7);
	}
	return Damage;
}

void UVenturesFeature::DiedVeil(AAerondelMob* Mob, bool bByPlayer)
{
	if (!bByPlayer || !Mob) return;
	const FString Name = Mob->GetDisplayName().ToString();
	bool bLesser = false;
	for (const TCHAR* B : LesserBosses) if (Name == B) { bLesser = true; break; }
	if (!bLesser) return;
	if (FMath::FRand() >= 0.25f || VeilLesser >= SoulCap) return;
	++VeilLesser;
	Say(FString::Printf(TEXT("A lesser bright soul settles in your pouch (%d/%d)."), VeilLesser, SoulCap));
}

void UVenturesFeature::TabVeil(FGPanelModel& M)
{
	M.Title = TEXT("Veilcraft");
	M.Lines.Add(TEXT("Unmake a non-unique kit-bearing item to learn its named effect — the item is destroyed, you keep the lesson. Bind a lesson onto a plain piece with a soul and 2 Clear essence. One effect per item. Not a Facet, not a graft, not a kit, and not a damage percent. Uniques cannot be unmade."));
	M.Lines.Add(FString::Printf(TEXT("Souls: %d/%d · Lesser bright souls: %d/%d"), WyrmSouls(), SoulCap, VeilLesser, SoulCap));
	M.Lines.Add(TEXT("Known lessons"));
	if (!VeilKnown.Num()) M.Lines.Add(TEXT("Unmake a kit-bearing item to learn a lesson."));
	for (const FString& Id : VeilKnown)
	{
		const FFx* E = FindFx(Id); if (!E) continue;
		const bool bOn = VeilSelFx == Id;
		VAct32(M, FString::Printf(TEXT("%s%s"), bOn ? TEXT("● ") : TEXT(""), E->Name), [this, Id] { VeilSelFx = (VeilSelFx == Id) ? FString() : Id; });
	}
	if (const FFx* E = FindFx(VeilSelFx)) M.Lines.Add(E->Desc);
	M.Lines.Add(TEXT("Your gear"));
	TSet<FString> Seen;
	auto AddGear = [&](const FString& Name, const UItemDef* I)
	{
		if (Name.IsEmpty() || !I || !IsGear(I) || Seen.Contains(Name)) return;
		Seen.Add(Name);
		const FString Bound = VeilOnItem(Name);
		const TArray<FString> Kits = KitList(Name);
		FString Mark = Bound.IsEmpty() ? (Kits.Num() ? TEXT("kits") : TEXT("plain")) : FString::Printf(TEXT("veil: %s"), FindFx(Bound) ? FindFx(Bound)->Name : *Bound);
		M.Lines.Add(FString::Printf(TEXT("%s — %s"), *Name, *Mark));
		const bool bSel = VeilSelItem == Name;
		VAct32(M, FString::Printf(TEXT("%sSelect %s"), bSel ? TEXT("● ") : TEXT(""), *Name), [this, Name] { VeilSelItem = (VeilSelItem == Name) ? FString() : Name; });
		if (Kits.Num() || !Bound.IsEmpty()) VAct32(M, FString::Printf(TEXT("Unmake %s (destroyed)"), *Name), [this, Name] { VeilUnmake(Name); });
		if (!Bound.IsEmpty()) VAct32(M, FString::Printf(TEXT("Unbind %s (%dc)"), *Name, UnbindGold), [this, Name] { VeilUnbind(Name); });
	};
	if (const AAerondelPlayerState* PS = GetPS())
	{
		if (PS->Equipment) for (const auto& P : PS->Equipment->GetAll()) if (P.Value) AddGear(ItemName(P.Value), P.Value);
		if (PS->Inventory) for (const FItemStack& Sl : PS->Inventory->GetSlots()) if (!Sl.IsEmpty() && Sl.Item) AddGear(ItemName(Sl.Item), Sl.Item);
	}
	if (!Seen.Num()) M.Lines.Add(TEXT("Wear or carry a weapon or armour piece."));
	if (!VeilSelItem.IsEmpty() && !VeilSelFx.IsEmpty() && VeilOnItem(VeilSelItem).IsEmpty())
	{
		const FFx* E = FindFx(VeilSelFx);
		VAct32(M, FString::Printf(TEXT("Bind %s onto %s"), E ? E->Name : *VeilSelFx, *VeilSelItem),
			[this] { VeilBind(VeilSelItem, VeilSelFx); });
	}
	M.Lines.Add(TEXT("This item will be destroyed if you Unmake it."));
}
