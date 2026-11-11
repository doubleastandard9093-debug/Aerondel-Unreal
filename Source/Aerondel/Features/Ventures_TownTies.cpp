#include "Features/VenturesFeature.h"
#include "UI/AerondelPanels.h"
#include "Core/AerondelPlayerState.h"
#include "Inventory/InventoryComponent.h"
#include "Data/GameDatabase.h"
#include "Data/NpcDef.h"
#include "Data/ItemDef.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// Ventures group 13 (web adapt/townties.js): friendship ranks with ordinary townsfolk, not Sworn Companions.
// Talk once a day and give a gift. Loved gifts, and produce you grew, mean more. Ranks cap at Kindred (5).
// Friendly folk (rank 2+) offer a modest 5% shop discount.

namespace VenturesTies
{
	static constexpr int32 TieMax = 5;
	static constexpr int64 GiftMs = 10 * 60 * 1000;
	static const TCHAR* Ranks[] = { TEXT("Stranger"), TEXT("Acquaintance"), TEXT("Friendly"), TEXT("Trusted"), TEXT("Close"), TEXT("Kindred") };
	static bool IsCompanion(const FString& Id)
	{
		return Id == TEXT("tamsin") || Id == TEXT("ysolde") || Id == TEXT("brannoc") || Id == TEXT("mirelle");
	}
	enum class EGiftKind { None, Hate, Ok, Love, LovePlus };
	static bool Match(const FString& Item, const TCHAR* const* Parts)
	{
		for (int32 i = 0; Parts[i]; ++i)
		{
			const FString P(Parts[i]);
			if (P.StartsWith(TEXT("^")) && Item.StartsWith(P.Mid(1))) return true;
			if (P.EndsWith(TEXT("$")) && Item.EndsWith(P.LeftChop(1))) return true;
			if (!P.StartsWith(TEXT("^")) && !P.EndsWith(TEXT("$")) && Item.Contains(P)) return true;
		}
		return false;
	}
	static void LoveHate(const FString& Id, const FString& Name, const TCHAR* const*& Love, const TCHAR* const*& Hate)
	{
		static const TCHAR* Pie[] = { TEXT("Pie"), nullptr };
		static const TCHAR* Bones[] = { TEXT("Bones"), nullptr };
		static const TCHAR* Cooked[] = { TEXT("Bread"), TEXT("Cake"), TEXT("Pie"), TEXT("Stew"), TEXT("^Cooked "), nullptr };
		static const TCHAR* OreBar[] = { TEXT("Bones"), TEXT(" ore$"), TEXT(" bar$"), nullptr };
		static const TCHAR* Farm[] = { TEXT("Potato"), TEXT("Cabbage"), TEXT("Wheat"), TEXT("Tomato"), TEXT("Pumpkin"), TEXT("Winterroot"), TEXT("Egg"), TEXT("Milk"), nullptr };
		static const TCHAR* Fish[] = { TEXT("Cooked shrimp"), TEXT("Cooked lobster"), TEXT("Cooked swordfish"), TEXT("Cooked shark"), nullptr };
		static const TCHAR* Beer[] = { TEXT("Beer"), TEXT("Ale"), nullptr };
		static const TCHAR* BarOre[] = { TEXT(" bar$"), TEXT(" ore$"), TEXT("Coal"), nullptr };
		static const TCHAR* Rune[] = { TEXT(" rune"), TEXT("Feather"), TEXT("Book"), nullptr };
		if (Id == TEXT("hilda") || Name.Contains(TEXT("Hilda")) || Name.Contains(TEXT("Granny"))) { Love = Cooked; Hate = OreBar; return; }
		if (Id == TEXT("bram") || Name.Contains(TEXT("Bram"))) { static const TCHAR* L[] = { TEXT("Wheat"), TEXT("Barley"), TEXT("Logs"), nullptr }; Love = L; Hate = Bones; return; }
		if (Id == TEXT("wydin") || Name.Contains(TEXT("Wendric")) || Name.Contains(TEXT("Grocer"))) { static const TCHAR* L[] = { TEXT("Potato"), TEXT("Cabbage"), TEXT("Wheat"), TEXT("Tomato"), TEXT("^Cooked "), nullptr }; Love = L; Hate = Bones; return; }
		if (Id == TEXT("sarah") || Name.Contains(TEXT("Farmer Sarah"))) { Love = Farm; Hate = Bones; return; }
		if (Id == TEXT("thurgo") || Name.Contains(TEXT("Durgan"))) { Love = Pie; Hate = Bones; return; }
		if (Name.Contains(TEXT("Cook")) || Name.Contains(TEXT("Brewer"))) { static const TCHAR* L[] = { TEXT("Bread"), TEXT("Cake"), TEXT("Pie"), TEXT("Stew"), TEXT("^Cooked "), TEXT("Beer"), TEXT("Ale"), nullptr }; static const TCHAR* H[] = { TEXT("Bones"), TEXT("Grimy "), nullptr }; Love = L; Hate = H; return; }
		if (Name.Contains(TEXT("Smith")) || Name.Contains(TEXT("Miner"))) { Love = BarOre; static const TCHAR* H[] = { TEXT("Cake"), TEXT("Pie"), nullptr }; Hate = H; return; }
		if (Name.Contains(TEXT("Priest")) || Name.Contains(TEXT("Chapel"))) { static const TCHAR* L[] = { TEXT("Bones"), TEXT("Bread"), TEXT("Cake"), nullptr }; Love = L; Hate = Beer; return; }
		if (Name.Contains(TEXT("Ranger")) || Name.Contains(TEXT("Fletcher"))) { static const TCHAR* L[] = { TEXT("arrows"), TEXT("Feather"), TEXT("hide"), TEXT("leather"), nullptr }; Love = L; Hate = Bones; return; }
		if (Name.Contains(TEXT("Sail")) || Name.Contains(TEXT("Fish")) || Name.Contains(TEXT("Captain"))) { Love = Fish; Hate = Bones; return; }
		if (Name.Contains(TEXT("Farmer"))) { Love = Farm; Hate = Bones; return; }
		if (Name.Contains(TEXT("Scholar")) || Name.Contains(TEXT("Wizard")) || Name.Contains(TEXT("Mage"))) { Love = Rune; Hate = Beer; return; }
		static const TCHAR* Work[] = { TEXT("Logs"), TEXT("Flax"), TEXT("Soft leather"), TEXT("Wool"), nullptr };
		Love = Work; Hate = Bones;
	}
	static void VAct13(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
}
using namespace VenturesTies;

void UVenturesFeature::ResetTie()
{
	TieRecs.Reset();
	TieShopId.Reset();
}

void UVenturesFeature::SaveTie(FJsonObject& A) const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	for (const auto& E : TieRecs)
	{
		TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
		R->SetNumberField(TEXT("rank"), E.Value.Rank);
		R->SetNumberField(TEXT("giftAt"), (double)E.Value.GiftAt);
		R->SetStringField(TEXT("talkAt"), E.Value.TalkAt);
		TSharedPtr<FJsonObject> Fl = MakeShared<FJsonObject>();
		Fl->SetBoolField(TEXT("heart"), E.Value.bHeart);
		Fl->SetBoolField(TEXT("errand"), E.Value.bErrand);
		R->SetObjectField(TEXT("flags"), Fl);
		S->SetObjectField(E.Key, R);
	}
	A.SetObjectField(TEXT("ties"), S);
}

void UVenturesFeature::LoadTie(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> S = JObj(A, TEXT("ties"));
	if (!S) return;
	for (const auto& E : S->Values)
	{
		if (IsCompanion(FString(*E.Key)) || !E.Value.IsValid() || E.Value->Type != EJson::Object) continue;
		const TSharedPtr<FJsonObject> R = E.Value->AsObject();
		FTieRec T;
		T.Rank = FMath::Clamp((int32)JInt(*R, TEXT("rank")), 0, TieMax);
		T.GiftAt = JInt(*R, TEXT("giftAt"));
		T.TalkAt = JStr(*R, TEXT("talkAt"));
		if (TSharedPtr<FJsonObject> Fl = JObj(*R, TEXT("flags"))) { T.bHeart = JBool(*Fl, TEXT("heart")); T.bErrand = JBool(*Fl, TEXT("errand")); }
		TieRecs.Add(FString(*E.Key), T);
	}
}

FTieRec& UVenturesFeature::TieOf(const FString& Id)
{
	if (FTieRec* R = TieRecs.Find(Id)) return *R;
	return TieRecs.Add(Id, FTieRec());
}

const UNpcDef* UVenturesFeature::TieNpc(const FString& Id) const
{
	const UGameDatabase* Db = UGameDatabase::Get();
	if (!Db || IsCompanion(Id)) return nullptr;
	return Db->FindNpc(FName(*Id));
}

bool UVenturesFeature::TieNear(const FString& Id, float& Dist) const
{
	const UNpcDef* N = TieNpc(Id);
	FVector2D P; if (!N || !N->bHasHome || !PlayerWebPos(P)) return false;
	Dist = FVector2D::Distance(P, N->Home);
	return Dist < 3.4f;
}

int32 UVenturesFeature::TieBump(const FString& Id, int32 N)
{
	FTieRec& R = TieOf(Id);
	const int32 Was = R.Rank;
	R.Rank = FMath::Clamp(R.Rank + N, 0, TieMax);
	const UNpcDef* Npc = TieNpc(Id);
	const FString Name = Npc ? Npc->DisplayName.ToString() : Id;
	if (R.Rank >= 2 && Was < 2) Say(FString::Printf(TEXT("%s will give you a modest shop discount when you trade with them."), *Name));
	if (R.Rank >= 4 && !R.bHeart) { R.bHeart = true; Say(FString::Printf(TEXT("%s shares a short, private word with you. You have become close."), *Name)); }
	if (R.Rank >= 3) R.bErrand = true;
	return R.Rank;
}

bool UVenturesFeature::TieDiscount(const FString& Id) const
{
	const FTieRec* R = TieRecs.Find(Id);
	return TieNpc(Id) && R && R->Rank >= 2;
}

float UVenturesFeature::TieBuyDiscount(const UObject* WorldContext)
{
	const UVenturesFeature* V = Get(WorldContext);
	if (!V || V->TieShopId.IsEmpty() || !V->TieDiscount(V->TieShopId)) return 0.f;
	return 0.05f;
}

bool UVenturesFeature::TieTalk(const FString& Id)
{
	const UNpcDef* N = TieNpc(Id); if (!N) return false;
	float Dist = 0.f;
	if (!TieNear(Id, Dist)) { Say(FString::Printf(TEXT("Stand closer to %s to talk."), *N->DisplayName.ToString())); return false; }
	FTieRec& R = TieOf(Id);
	const FString Day = TodayUtc();
	if (R.TalkAt == Day) { Say(FString::Printf(TEXT("%s already had a proper chat with you today."), *N->DisplayName.ToString())); return false; }
	R.TalkAt = Day;
	const int32 Rank = TieBump(Id, 1);
	Say(FString::Printf(TEXT("You talk with %s. Town Tie: %s (%d/%d)."), *N->DisplayName.ToString(), Ranks[Rank], Rank, TieMax));
	if (R.Rank >= 2) TieShopId = Id;
	return true;
}

bool UVenturesFeature::TieGift(const FString& Id, const FString& Item)
{
	const UNpcDef* N = TieNpc(Id); if (!N) return false;
	float Dist = 0.f;
	if (!TieNear(Id, Dist)) { Say(FString::Printf(TEXT("Stand closer to %s to give a gift."), *N->DisplayName.ToString())); return false; }
	if (CountItem(Item) < 1) { Say(FString::Printf(TEXT("You do not have a %s."), *Item)); return false; }
	FTieRec& R = TieOf(Id);
	if (NowMs() - R.GiftAt < GiftMs) { Say(FString::Printf(TEXT("%s already received a gift recently. Try again in a little while."), *N->DisplayName.ToString())); return false; }
	const TCHAR* const* Love = nullptr; const TCHAR* const* Hate = nullptr;
	LoveHate(Id, N->DisplayName.ToString(), Love, Hate);
	const bool bGrown = GreenGrewOf(Item) > 0;
	static const TCHAR* Produce[] = { TEXT("Potato"), TEXT("Cabbage"), TEXT("Wheat"), TEXT("Tomato"), TEXT("Pumpkin"), TEXT("Winterroot"), TEXT("Egg"), TEXT("Milk"), TEXT("Wool"), TEXT("Farm preserves"), nullptr };
	const bool bProduce = Match(Item, Produce);
	EGiftKind Kind = EGiftKind::Ok;
	if (Hate && Match(Item, Hate) && !bGrown) Kind = EGiftKind::Hate;
	else if ((Love && Match(Item, Love)) || bGrown) Kind = (bGrown || bProduce) ? EGiftKind::LovePlus : EGiftKind::Love;
	if (!TakeItem(Item, 1)) return false;
	R.GiftAt = NowMs();
	const FString Name = N->DisplayName.ToString();
	if (Kind == EGiftKind::Hate) { Say(FString::Printf(TEXT("%s does not care for the %s. (Town Tie unchanged.)"), *Name, *Item)); return true; }
	if (Kind == EGiftKind::Love || Kind == EGiftKind::LovePlus)
	{
		const int32 Rank = TieBump(Id, 1);
		FString Extra;
		if (bGrown) Extra = TEXT(" — especially since you grew it");
		else if (Kind == EGiftKind::LovePlus && bProduce) Extra = TEXT(" — they like farm produce");
		FString Msg = FString::Printf(TEXT("%s %s the %s%s. Town Tie: %s (%d/%d)."), *Name, Kind == EGiftKind::LovePlus ? TEXT("is delighted with") : TEXT("loves"), *Item, *Extra, Ranks[Rank], Rank, TieMax);
		if (Kind == EGiftKind::LovePlus) { GiveXp(TEXT("Farming"), 15.f); Msg += TEXT(" (+15 Farming)"); }
		Say(Msg);
	}
	else Say(FString::Printf(TEXT("%s accepts the %s politely. Town Tie stays at %s (%d/%d)."), *Name, *Item, Ranks[R.Rank], R.Rank, TieMax));
	if (R.Rank >= 2) TieShopId = Id;
	return true;
}

void UVenturesFeature::TabTie(FGPanelModel& M)
{
	M.Title = TEXT("Town Ties");
	M.Lines.Add(TEXT("Friendships with ordinary townsfolk and shopkeepers — not your Sworn Companions. Talk once a day and give a gift. Loved gifts, and produce you grew, mean more. Ranks cap at Kindred (5). Friendly folk offer a modest 5% shop discount."));
	const UGameDatabase* Db = UGameDatabase::Get();
	FVector2D P; const bool bPos = PlayerWebPos(P);
	TArray<const UNpcDef*> Near, Met;
	if (Db) for (const UNpcDef* N : Db->Npcs)
	{
		if (!N) continue;
		const FString Id = N->Id.IsNone() ? N->GetFName().ToString() : N->Id.ToString();
		if (IsCompanion(Id)) continue;
		float Dist = 0.f;
		if (TieNear(Id, Dist)) Near.Add(N);
		else if (TieRecs.Contains(Id)) Met.Add(N);
	}
	if (Near.Num() == 0 && Met.Num() == 0) M.Lines.Add(TEXT("Talk to townsfolk in Larkhaven, Emberfold, Saltreach, Highcrest or Whitecairn to start a Tie."));
	auto Row = [&](const UNpcDef* N, bool bHere)
	{
		const FString Id = N->Id.IsNone() ? N->GetFName().ToString() : N->Id.ToString();
		const FTieRec* R = TieRecs.Find(Id);
		const int32 Rank = R ? R->Rank : 0;
		M.Lines.Add(FString::Printf(TEXT("%s — %s (%d/%d)%s%s%s"), *N->DisplayName.ToString(), Ranks[Rank], Rank, TieMax,
			(R && R->Rank >= 2) ? TEXT(" · shop discount") : TEXT(""),
			(R && R->bHeart) ? TEXT(" · close") : TEXT(""),
			bHere ? TEXT(" · you are here") : TEXT("")));
		if (!bHere) return;
		VAct13(M, FString::Printf(TEXT("Chat with %s"), *N->DisplayName.ToString()), [this, Id] { TieTalk(Id); });
		if (const AAerondelPlayerState* PS = GetPS()) if (PS->Inventory)
		{
			int32 Shown = 0;
			for (const FItemStack& St : PS->Inventory->GetSlots())
			{
				if (St.IsEmpty() || Shown >= 8) continue;
				const FString Item = St.Item->DisplayName.ToString();
				VAct13(M, FString::Printf(TEXT("Give %s to %s"), *Item, *N->DisplayName.ToString()), [this, Id, Item] { TieGift(Id, Item); });
				++Shown;
			}
		}
		if (R && R->Rank >= 2) VAct13(M, FString::Printf(TEXT("Trade with %s (Friendly discount)"), *N->DisplayName.ToString()), [this, Id] { TieShopId = Id; Say(TEXT("Your Friendly discount will apply the next time you buy from a shop.")); });
	};
	for (const UNpcDef* N : Near) Row(N, true);
	for (const UNpcDef* N : Met) Row(N, false);
}
