#include "Features/GuildFeature.h"
#include "Features/CompanionsFeature.h"
#include "Core/AerondelEvents.h"
#include "Data/MobDef.h"
#include "Data/SkillDef.h"
#include "Dom/JsonObject.h"

namespace GuildData
{
	struct FRoom { const TCHAR* Id; const TCHAR* Name; const TCHAR* What; int32 Cost; int32 Renown; };
	static const FRoom Rooms[] = {
		{ TEXT("hall"), TEXT("Great hall"), TEXT("Seats more members (+3 per level) and +2% renown per level"), 4000, 0 },
		{ TEXT("training"), TEXT("Training yard"), TEXT("+1% combat XP per level (always on)"), 6000, 50 },
		{ TEXT("workshop"), TEXT("Workshop"), TEXT("+1% gathering and artisan XP per level (always on)"), 6000, 50 },
		{ TEXT("kitchen"), TEXT("Kitchen"), TEXT("Boosts last 10% longer per level"), 5000, 100 },
		{ TEXT("vault"), TEXT("Vault"), TEXT("Fortune boost pays 20% more per level"), 8000, 150 },
		{ TEXT("shrine"), TEXT("Shrine"), TEXT("Each level makes boosts 8% cheaper in renown"), 7000, 200 } };
	struct FBoost { const TCHAR* Id; const TCHAR* Name; int32 Minutes; int32 Cost; const TCHAR* Effect; };
	static const FBoost Boosts[] = {
		{ TEXT("wisdom"), TEXT("Banner of Wisdom"), 30, 60, TEXT("+10% XP in every skill") },
		{ TEXT("valor"), TEXT("Banner of Valor"), 30, 60, TEXT("+15% combat XP and 5% damage reduction") },
		{ TEXT("craft"), TEXT("Banner of the Craft"), 30, 60, TEXT("+15% gathering and artisan XP") },
		{ TEXT("fortune"), TEXT("Banner of Fortune"), 30, 80, TEXT("Extra coins and guild crests from kills") },
		{ TEXT("rally"), TEXT("Grand Rally"), 15, 200, TEXT("+25% XP in every skill (guild hall level 3)") } };
	static const TCHAR* Hirelings[] = { TEXT("Brannoc the Steady"), TEXT("Isel Quickhand"), TEXT("Morwen Ashby"), TEXT("Tobin Fell"), TEXT("Kestra Vale"), TEXT("Orrin Blackbrook"),
		TEXT("Sabeth Wren"), TEXT("Dunmar Oakes"), TEXT("Liraen Moss"), TEXT("Corvin Haske"), TEXT("Eda Thornfield"), TEXT("Pell Marrow") };
	static const TCHAR* Names[] = { TEXT("Lantern Oath"), TEXT("Larkhaven Wardens"), TEXT("Silver Compass"), TEXT("Order of the Ember"), TEXT("Hollow Oak Company"), TEXT("Free Blades of Aerondel") };
	static bool IsCombat(FName S)
	{
		static const FName C[] = { TEXT("attack"), TEXT("strength"), TEXT("defence"), TEXT("hitpoints"), TEXT("ranged"), TEXT("magic"), TEXT("prayer") };
		for (const FName& N : C) if (N == S) return true; return false;
	}
}

void UGuildFeature::Initialize(FSubsystemCollectionBase& Collection)
{
	UAerondelEvents* Ev = Collection.InitializeDependency<UAerondelEvents>();
	Super::Initialize(Collection);
	if (Ev) { Ev->OnMobKilled.AddDynamic(this, &UGuildFeature::HandleMobKilled); Ev->OnXpGained.AddDynamic(this, &UGuildFeature::HandleXp); }
}

TArray<FString> UGuildFeature::CharterNames() { TArray<FString> O; for (const TCHAR* N : GuildData::Names) O.Add(N); return O; }
int32 UGuildFeature::NumRooms() { return (int32)UE_ARRAY_COUNT(GuildData::Rooms); }
int32 UGuildFeature::NumBoosts() { return (int32)UE_ARRAY_COUNT(GuildData::Boosts); }

void UGuildFeature::ResetState()
{
	Name.Reset(); Renown = Funds = XpAcc = 0; Rooms.Reset(); Hired.Reset(); BoostId.Reset(); BoostUntil = 0;
	for (const GuildData::FRoom& R : GuildData::Rooms) Rooms.Add(R.Id, 0);
}

void UGuildFeature::SaveState(FJsonObject& Out) const
{
	if (!HasGuild()) return; // web saves null
	Out.SetStringField(TEXT("name"), Name); Out.SetNumberField(TEXT("renown"), Renown); Out.SetNumberField(TEXT("funds"), Funds); Out.SetNumberField(TEXT("xpAcc"), XpAcc);
	PutIntMap(Out, TEXT("rooms"), Rooms); PutStrArr(Out, TEXT("hired"), Hired);
	if (!BoostId.IsEmpty()) { TSharedPtr<FJsonObject> B = MakeShared<FJsonObject>(); B->SetStringField(TEXT("id"), BoostId); B->SetNumberField(TEXT("until"), (double)BoostUntil); Out.SetObjectField(TEXT("boost"), B); }
}

void UGuildFeature::LoadState(const FJsonObject& In)
{
	ResetState();
	Name = JStr(In, TEXT("name")).Left(24); if (Name.IsEmpty()) return;
	Renown = FMath::Max<int32>(0, JInt(In, TEXT("renown"))); Funds = FMath::Max<int32>(0, JInt(In, TEXT("funds"))); XpAcc = FMath::Max<int32>(0, JInt(In, TEXT("xpAcc")));
	const TMap<FString, int32> R = JIntMap(In, TEXT("rooms"));
	for (auto& P : Rooms) P.Value = FMath::Clamp(R.FindRef(P.Key), 0, 5);
	Rooms.Add(TEXT("hall"), FMath::Max(1, Rooms.FindRef(TEXT("hall"))));
	for (const FString& H : JStrArr(In, TEXT("hired"))) for (const TCHAR* N : GuildData::Hirelings) if (H == N) Hired.AddUnique(H);
	if (TSharedPtr<FJsonObject> B = JObj(In, TEXT("boost"))) { BoostId = JStr(*B, TEXT("id")); BoostUntil = JInt(*B, TEXT("until")); }
}

FString UGuildFeature::ActiveBoost() const { return HasGuild() && !BoostId.IsEmpty() && BoostUntil > NowMs() ? BoostId : FString(); }

TArray<FString> UGuildFeature::Members() const
{
	TArray<FString> O; if (!HasGuild()) return O;
	if (const UCompanionsFeature* C = Feature<UCompanionsFeature>()) for (const FCompanionDef& D : UCompanionsFeature::Defs()) if (C->GetState(D.Id).bRecruited) O.Add(D.Name);
	O.Append(Hired); return O;
}

void UGuildFeature::AddRenown(int32 N) { if (HasGuild()) Renown += FMath::RoundToInt(N * (1.f + Rooms.FindRef(TEXT("hall")) * 0.02f)); }

void UGuildFeature::HandleXp(USkillDef* Skill, float Amount, float TotalXp, int32 Level)
{
	if (!HasGuild()) return;
	if (++XpAcc >= 40) { XpAcc = 0; AddRenown(1); }
}

float UGuildFeature::XpBonus(FName SkillId) const
{
	if (!HasGuild()) return 0.f;
	const bool bC = GuildData::IsCombat(SkillId);
	float M = (bC ? Rooms.FindRef(TEXT("training")) : Rooms.FindRef(TEXT("workshop"))) * 0.01f + Members().Num() * 0.005f;
	const FString B = ActiveBoost();
	if (B == TEXT("wisdom")) M += 0.1f; else if (B == TEXT("rally")) M += 0.25f;
	else if (B == TEXT("valor") && bC) M += 0.15f; else if (B == TEXT("craft") && !bC) M += 0.15f;
	return M;
}

float UGuildFeature::DamageTakenMult(const UMobDef* Source, float HpFrac) const { return ActiveBoost() == TEXT("valor") ? 0.95f : 1.f; }

void UGuildFeature::HandleMobKilled(UMobDef* Mob)
{
	if (!HasGuild()) return;
	if (FMath::FRand() < 0.25f) AddRenown(1);
	if (ActiveBoost() == TEXT("fortune"))
	{
		const int32 L = Mob ? FMath::Max(1, Mob->CombatLevel) : 5;
		GiveCoins(FMath::RoundToInt((5 + L * 2) * (1.f + Rooms.FindRef(TEXT("vault")) * 0.2f)));
		if (FMath::FRand() < 0.06f) { GiveItem(TEXT("Guild crest"), 1); Say(TEXT("Fortune smiles: a Guild crest!")); }
	}
}

FString UGuildFeature::Status() const
{
	if (!HasGuild()) return TEXT("Guildmaster Halvard: Adventurers do better together. Want to found a guild? The charter costs 5,000 coins.");
	const FString B = ActiveBoost(); FString BT;
	for (const GuildData::FBoost& X : GuildData::Boosts) if (B == X.Id) BT = FString::Printf(TEXT(" Active: %s, %d min left."), X.Name, (int32)FMath::DivideAndRoundUp<int64>(BoostUntil - NowMs(), 60000));
	const TArray<FString> M = Members();
	return FString::Printf(TEXT("%s: %d renown, %d coins in the treasury, %d/%d members (%s).%s"), *Name, Renown, Funds, M.Num(), Seats(), M.Num() ? *FString::Join(M, TEXT(", ")) : TEXT("just you"), *BT);
}

FString UGuildFeature::Found(int32 NameIndex)
{
	if (HasGuild()) return TEXT("You already lead a guild.");
	if (NameIndex < 0 || NameIndex >= (int32)UE_ARRAY_COUNT(GuildData::Names)) return FString();
	if (!TakeCoins(5000)) return TEXT("The charter is 5,000 coins.");
	ResetState(); Name = GuildData::Names[NameIndex]; Rooms.Add(TEXT("hall"), 1);
	return FString::Printf(TEXT("You found the guild %s! Its hall stands west of Larkhaven."), *Name);
}

FString UGuildFeature::Deposit(int32 Coins)
{
	if (!HasGuild() || Coins <= 0) return FString();
	if (!TakeCoins(Coins)) return FString::Printf(TEXT("You need %d coins."), Coins);
	Funds += Coins; AddRenown(Coins / 500);
	return FString::Printf(TEXT("Treasury: %d coins."), Funds);
}

FString UGuildFeature::RoomLabel(int32 i) const
{
	if (i < 0 || i >= NumRooms()) return FString();
	const GuildData::FRoom& R = GuildData::Rooms[i]; const int32 Lv = Rooms.FindRef(R.Id);
	if (Lv >= 5) return FString::Printf(TEXT("%s (max)"), R.Name);
	return FString::Printf(TEXT("%s %d->%d: %d coins%s"), R.Name, Lv, Lv + 1, R.Cost * (Lv + 1), R.Renown ? *FString::Printf(TEXT(", %d renown"), R.Renown * (Lv + 1)) : TEXT(""));
}

FString UGuildFeature::UpgradeRoom(int32 i)
{
	if (!HasGuild() || i < 0 || i >= NumRooms()) return FString();
	const GuildData::FRoom& R = GuildData::Rooms[i]; int32& Lv = Rooms.FindOrAdd(R.Id);
	if (Lv >= 5) return FString::Printf(TEXT("%s is as grand as it gets. %s."), R.Name, R.What);
	const int32 C = R.Cost * (Lv + 1), Rn = R.Renown * (Lv + 1);
	if (Funds < C) return FString::Printf(TEXT("The treasury holds %d coins; that needs %d."), Funds, C);
	if (Renown < Rn) return FString::Printf(TEXT("The masons want a guild of %d renown first."), Rn);
	Funds -= C; ++Lv;
	return FString::Printf(TEXT("%s upgraded to level %d. %s."), R.Name, Lv, R.What);
}

FString UGuildFeature::BoostLabel(int32 i) const
{
	if (i < 0 || i >= NumBoosts()) return FString();
	const GuildData::FBoost& B = GuildData::Boosts[i];
	return FString::Printf(TEXT("%s (%d min, %d renown): %s"), B.Name, FMath::RoundToInt(B.Minutes * (1.f + Rooms.FindRef(TEXT("kitchen")) * 0.1f)),
		FMath::RoundToInt(B.Cost * (1.f - Rooms.FindRef(TEXT("shrine")) * 0.08f)), B.Effect);
}

FString UGuildFeature::RaiseBoost(int32 i)
{
	if (!HasGuild() || i < 0 || i >= NumBoosts()) return FString();
	const GuildData::FBoost& B = GuildData::Boosts[i];
	const int32 Cost = FMath::RoundToInt(B.Cost * (1.f - Rooms.FindRef(TEXT("shrine")) * 0.08f));
	const int32 Min = FMath::RoundToInt(B.Minutes * (1.f + Rooms.FindRef(TEXT("kitchen")) * 0.1f));
	if (FString(B.Id) == TEXT("rally") && Rooms.FindRef(TEXT("hall")) < 3) return TEXT("A Grand Rally needs a level 3 great hall.");
	if (Renown < Cost) return FString::Printf(TEXT("That costs %d renown; the guild has %d."), Cost, Renown);
	Renown -= Cost; BoostId = B.Id; BoostUntil = NowMs() + (int64)Min * 60000;
	return FString::Printf(TEXT("%s flies over %s for %d minutes: %s."), B.Name, *Name, Min, B.Effect);
}

FString UGuildFeature::Hire()
{
	if (!HasGuild()) return FString();
	TArray<FString> Free; for (const TCHAR* H : GuildData::Hirelings) if (!Hired.Contains(H)) Free.Add(H);
	if (Members().Num() >= Seats()) return TEXT("No seats left. Upgrade the great hall.");
	if (!Free.Num()) return TEXT("Every adventurer in Aerondel already wears your crest!");
	if (Funds < 2000) return TEXT("Hiring is paid from the treasury: 2,000 coins.");
	Funds -= 2000; const FString H = Free[FMath::RandHelper(Free.Num())]; Hired.Add(H);
	return FString::Printf(TEXT("%s joins %s. Each member adds a little guild XP (up to +10%%)."), *H, *Name);
}

FString UGuildFeature::TurnInCrests()
{
	const int32 N = CountItem(TEXT("Guild crest")); if (!HasGuild() || N <= 0) return TEXT("You have no crests.");
	TakeItem(TEXT("Guild crest"), N); AddRenown(N * 15);
	return FString::Printf(TEXT("+%d renown."), N * 15);
}
