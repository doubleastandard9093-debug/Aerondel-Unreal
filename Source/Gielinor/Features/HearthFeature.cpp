#include "Features/HearthFeature.h"
#include "Features/CompanionsFeature.h"
#include "Core/GielinorEvents.h"
#include "Data/MobDef.h"

namespace HearthData
{
	struct FRoom { const TCHAR* Id; const TCHAR* Name; int32 Gold; const TCHAR* Item1; int32 N1; const TCHAR* Item2; int32 N2; };
	static const FRoom Rooms[] = {
		{ TEXT("workshop"), TEXT("Workshop"), 15000, TEXT("Oak logs"), 20, TEXT("Iron bar"), 10 },
		{ TEXT("garden"), TEXT("Herb garden"), 10000, TEXT("Logs"), 20, nullptr, 0 },
		{ TEXT("trophy"), TEXT("Trophy hall"), 30000, TEXT("Steel bar"), 10, nullptr, 0 },
		{ TEXT("quarters"), TEXT("Companion quarters"), 20000, TEXT("Oak logs"), 30, nullptr, 0 } };
	struct FFurn { const TCHAR* Id; const TCHAR* Room; const TCHAR* Name; const TCHAR* Skill; int32 Level; const TCHAR* Item1; int32 N1; const TCHAR* Item2; int32 N2; int32 Xp; };
	static const FFurn Furn[] = {
		{ TEXT("bench"), TEXT("workshop"), TEXT("Oak workbench"), TEXT("Crafting"), 20, TEXT("Oak logs"), 6, nullptr, 0, 300 },
		{ TEXT("anvil"), TEXT("workshop"), TEXT("Anvil stand"), TEXT("Smithing"), 30, TEXT("Iron bar"), 6, nullptr, 0, 400 },
		{ TEXT("planter"), TEXT("garden"), TEXT("Herb planter"), TEXT("Crafting"), 15, TEXT("Logs"), 10, nullptr, 0, 200 },
		{ TEXT("trough"), TEXT("garden"), TEXT("Watering trough"), TEXT("Smithing"), 20, TEXT("Iron bar"), 4, nullptr, 0, 250 },
		{ TEXT("plinth"), TEXT("trophy"), TEXT("Mounting plinth"), TEXT("Smithing"), 40, TEXT("Steel bar"), 6, nullptr, 0, 600 },
		{ TEXT("banners"), TEXT("trophy"), TEXT("Banner rack"), TEXT("Crafting"), 30, TEXT("Cowhide"), 4, TEXT("Logs"), 4, 350 },
		{ TEXT("bunks"), TEXT("quarters"), TEXT("Bunks"), TEXT("Crafting"), 25, TEXT("Oak logs"), 10, nullptr, 0, 350 },
		{ TEXT("stove"), TEXT("quarters"), TEXT("Hearth stove"), TEXT("Smithing"), 25, TEXT("Iron bar"), 5, nullptr, 0, 350 } };
	static const TCHAR* Bosses[] = { TEXT("Grumsh"), TEXT("Scorchwing"), TEXT("Captain Saltgrave") };
}

void UHearthFeature::Initialize(FSubsystemCollectionBase& Collection)
{
	UGielinorEvents* Ev = Collection.InitializeDependency<UGielinorEvents>();
	Super::Initialize(Collection);
	if (Ev) Ev->OnMobKilled.AddDynamic(this, &UHearthFeature::HandleMobKilled);
}

bool UHearthFeature::BuyDeed()
{
	if (bDeed) return false;
	if (!TakeCoins(DeedCost)) { Say(TEXT("A Hearthstead deed costs 25,000 coins.")); return false; }
	bDeed = true; Say(TEXT("You own a Hearthstead plot behind Larkhaven.")); return true;
}

bool UHearthFeature::BuildRoom(FName Room)
{
	if (!bDeed || HasRoom(Room)) return false;
	for (const HearthData::FRoom& R : HearthData::Rooms)
	{
		if (Room != FName(R.Id)) continue;
		if (CountItem(TEXT("Coins")) < R.Gold || CountItem(R.Item1) < R.N1 || (R.Item2 && CountItem(R.Item2) < R.N2)) { Say(FString::Printf(TEXT("You need %d coins and the materials for the %s."), R.Gold, R.Name)); return false; }
		TakeCoins(R.Gold); TakeItem(R.Item1, R.N1); if (R.Item2) TakeItem(R.Item2, R.N2);
		Rooms.Add(R.Id); Say(FString::Printf(TEXT("Your %s is built."), R.Name)); return true;
	}
	return false;
}

bool UHearthFeature::BuildFurniture(FName F)
{
	for (const HearthData::FFurn& D : HearthData::Furn)
	{
		if (F != FName(D.Id)) continue;
		if (HasFurniture(F) || !HasRoom(FName(D.Room))) return false;
		if (SkillLevel(D.Skill) < D.Level) { Say(FString::Printf(TEXT("You need %s %d."), D.Skill, D.Level)); return false; }
		if (CountItem(D.Item1) < D.N1 || (D.Item2 && CountItem(D.Item2) < D.N2)) { Say(TEXT("You don't have the materials.")); return false; }
		TakeItem(D.Item1, D.N1); if (D.Item2) TakeItem(D.Item2, D.N2);
		GiveXp(D.Skill, (float)D.Xp); Furniture.Add(D.Id);
		Say(FString::Printf(TEXT("You build the %s."), D.Name)); return true;
	}
	return false;
}

float UHearthFeature::XpBonus(FName SkillId) const
{
	if (HasRoom(TEXT("workshop")) && (SkillId == Id(TEXT("Smithing")) || SkillId == Id(TEXT("Crafting")) || SkillId == Id(TEXT("Fletching"))))
		return 0.05f + (HasFurniture(TEXT("bench")) ? 0.02f : 0.f) + (HasFurniture(TEXT("anvil")) ? 0.02f : 0.f);
	if (HasRoom(TEXT("garden")) && SkillId == Id(TEXT("Herbalism"))) return 0.05f;
	return 0.f;
}

float UHearthFeature::BossDamageBonus() const
{
	if (!HasRoom(TEXT("trophy"))) return 0.f;
	return Trophies.Num() * (0.02f + (HasFurniture(TEXT("plinth")) ? 0.01f : 0.f)) + (HasFurniture(TEXT("banners")) ? 0.02f : 0.f);
}

float UHearthFeature::ErrandBonus() const { return HasRoom(TEXT("quarters")) ? 0.1f + (HasFurniture(TEXT("bunks")) ? 0.05f : 0.f) : 0.f; }

int32 UHearthFeature::TakeHarvest()
{
	const int64 Now = NowMs();
	if (!HasRoom(TEXT("garden")) || Now - HarvestAt < HarvestMs) return 0;
	HarvestAt = Now;
	const int32 N = 4 + (HasFurniture(TEXT("planter")) ? 2 : 0) + (HasFurniture(TEXT("trough")) ? 2 : 0);
	GiveItem(TEXT("Grimy greenwort"), N);
	return N;
}

int32 UHearthFeature::TakeBondDay()
{
	const int64 Now = NowMs();
	if (!HasRoom(TEXT("quarters")) || Now - BondAt < BondDayMs) return 0;
	BondAt = Now;
	const int32 N = 1 + (HasFurniture(TEXT("stove")) ? 1 : 0);
	if (UCompanionsFeature* C = Feature<UCompanionsFeature>()) for (const FCompanionDef& D : UCompanionsFeature::Defs()) C->AddBond(D.Id, N);
	return N;
}

void UHearthFeature::HandleMobKilled(UMobDef* Mob)
{
	if (!Mob || !bDeed) return;
	for (const TCHAR* B : HearthData::Bosses)
		if (Mob->Id == Id(B) && !Trophies.Contains(B)) { Trophies.Add(B); Say(FString::Printf(TEXT("A trophy from %s is sent to your Hearthstead."), B)); }
}

void UHearthFeature::SaveState(FJsonObject& Out) const
{
	Out.SetBoolField(TEXT("deed"), bDeed);
	PutStrArr(Out, TEXT("rooms"), Rooms); PutStrArr(Out, TEXT("furniture"), Furniture); PutStrArr(Out, TEXT("trophies"), Trophies);
	Out.SetNumberField(TEXT("harvestAt"), (double)HarvestAt); Out.SetNumberField(TEXT("bondAt"), (double)BondAt);
}

void UHearthFeature::LoadState(const FJsonObject& In)
{
	bDeed = JBool(In, TEXT("deed"));
	for (const FString& S : JStrArr(In, TEXT("rooms"))) for (const HearthData::FRoom& R : HearthData::Rooms) if (S == R.Id) Rooms.AddUnique(S);
	for (const FString& S : JStrArr(In, TEXT("furniture"))) for (const HearthData::FFurn& F : HearthData::Furn) if (S == F.Id) Furniture.AddUnique(S);
	for (const FString& S : JStrArr(In, TEXT("trophies"))) for (const TCHAR* B : HearthData::Bosses) if (S == B) Trophies.AddUnique(S);
	HarvestAt = JInt(In, TEXT("harvestAt")); BondAt = JInt(In, TEXT("bondAt"));
}
