#include "Activities/ActivityActors.h"
#include "Core/GielinorPlayerState.h"
#include "Core/PlayerStatsComponent.h"
#include "Core/GielinorEvents.h"
#include "Rules/GielinorRules.h"
#include "Data/GameDatabase.h"
#include "Data/ContentDefs.h"
#include "Data/ItemDef.h"
#include "Data/SkillDef.h"
#include "Data/QuestDef.h"
#include "Skills/SkillsComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Quests/QuestComponent.h"
#include "Combat/HealthComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"

#define LOCTEXT_NAMESPACE "GielinorActivities"

namespace
{
	const UGameDatabase* ActDb() { return UGameDatabase::Get(); }
	USkillDef* ActSkill(FName Id) { return ActDb() ? ActDb()->FindSkill(Id) : nullptr; }
	UItemDef* ActCoins() { return ActDb() ? ActDb()->FindItem(TEXT("coins")) : nullptr; }
	void ActGiveXp(AGielinorPlayerState* S, FName Id, float Xp) { S->Skills->AddXp(ActSkill(Id), Xp * UGielinorRules::XpMultiplier(S, Id)); }
	bool ActStunned(AGielinorPlayerState* S) { return S->Stats && S->Stats->IsStunned(); }
}

// ------------------------------------------------------------------ agility
const FAgilityObstacle* AAgilityObstacleActor::Obstacle(const UAgilityCourseDef*& OutCourse) const
{
	OutCourse = ActDb() ? ActDb()->FindAgilityCourse(CourseId) : nullptr;
	return OutCourse && OutCourse->Obstacles.IsValidIndex(Index) ? &OutCourse->Obstacles[Index] : nullptr;
}

FText AAgilityObstacleActor::GetInteractLabel() const
{
	const UAgilityCourseDef* C = nullptr;
	const FAgilityObstacle* O = Obstacle(C);
	return O ? FText::Format(LOCTEXT("Obs", "{0} {1}"), O->Verb, O->DisplayName) : DisplayName;
}

void AAgilityObstacleActor::Interact(AActor* InInstigator)
{
	AGielinorPlayerState* S = PS(InInstigator);
	const UAgilityCourseDef* C = nullptr;
	const FAgilityObstacle* O = Obstacle(C);
	if (!S || !O || !S->Stats || ActStunned(S)) return;
	const int32 L = S->Skills->GetLevelById(TEXT("agility"));
	if (L < O->Level) { Msg(FText::Format(LOCTEXT("AgLvl", "You need an Agility level of {0} to use this course."), O->Level)); return; }
	UPlayerStatsComponent* St = S->Stats;
	const float XpMult = 1.f + UGielinorRules::Perk(S, TEXT("agility"), TEXT("agilityXp"));
	APawn* Pawn = Cast<APawn>(InInstigator);
	if (FMath::FRand() >= UGielinorRules::AgilitySuccess(L, O->Level, UGielinorRules::Perk(S, TEXT("agility"), TEXT("failLess"))))
	{
		St->AgilityCourse = NAME_None;
		Msg(FText::Format(LOCTEXT("Slip", "You slip and fall off the {0}."), O->DisplayName));
		ActGiveXp(S, TEXT("agility"), FMath::RoundToFloat(O->Xp * 0.25f * XpMult * 10.f) / 10.f);
		St->ApplyPlayerDamage(FMath::Max(1, FMath::FloorToInt(O->FailDamage * (0.5f + FMath::FRand() * 0.5f))), this);
		OnCrossed(InInstigator, false);
		return;
	}
	if (St->AgilityCourse != CourseId) { St->AgilityCourse = CourseId; St->AgilityNext = 0; }
	if (Index == St->AgilityNext) ++St->AgilityNext; else St->AgilityNext = Index == 0 ? 1 : -1;
	float LapXp = 0.f;
	if (Index == C->Obstacles.Num() - 1 && St->AgilityNext == C->Obstacles.Num()) { LapXp = C->LapXp; St->AgilityNext = 0; }
	if (Pawn) Pawn->TeleportTo(GetActorTransform().TransformPosition(End), Pawn->GetActorRotation());
	Msg(FText::Format(LOCTEXT("Cross", "You {0} the {1}."), O->Verb, O->DisplayName));
	if (LapXp > 0.f) Msg(FText::Format(LOCTEXT("Lap", "Lap complete! You finish the {0} for bonus Agility XP."), C->DisplayName));
	ActGiveXp(S, TEXT("agility"), (O->Xp + LapXp) * XpMult);
	OnCrossed(InInstigator, true);
}

// ------------------------------------------------------------------ thieving
FText AThieveTarget::GetInteractLabel() const
{
	const UThieveTargetDef* T = ActDb() ? ActDb()->FindThieveTarget(TargetId) : nullptr;
	return FText::Format(LOCTEXT("Pick", "Pickpocket {0}"), T ? T->DisplayName : DisplayName);
}

void AThieveTarget::Interact(AActor* InInstigator)
{
	AGielinorPlayerState* S = PS(InInstigator);
	const UThieveTargetDef* T = ActDb() ? ActDb()->FindThieveTarget(TargetId) : nullptr;
	if (!S || !T || !S->Stats) return;
	if (ActStunned(S)) { Msg(LOCTEXT("Stunned", "You're stunned!")); return; }
	const int32 L = S->Skills->GetLevelById(TEXT("thieving"));
	if (L < T->Level) { Msg(FText::Format(LOCTEXT("ThLvl", "You need a Thieving level of {0} to pickpocket that."), T->Level)); return; }
	const float LootBonus = UGielinorRules::Perk(S, TEXT("thieving"), TEXT("lootChance"));
	if (FMath::FRand() >= UGielinorRules::ThieveChance(L, T->Level, LootBonus * 0.2f))
	{
		Msg(FText::Format(LOCTEXT("Caught", "You fail to pick the {0}'s pocket. You've been stunned!"), T->DisplayName));
		S->Stats->StunUntil = S->Stats->GameTick + T->StunTicks;
		S->Stats->ApplyPlayerDamage(FMath::Max(1, FMath::FloorToInt(T->FailDamage * (0.5f + FMath::FRand() * 0.5f))), this);
		return;
	}
	for (const FDropEntry& D : T->Loot)
	{
		if (D.Chance < 1.f && FMath::FRand() >= D.Chance * (1.f + LootBonus)) continue;
		UItemDef* Got = D.OneOf.Num() ? D.OneOf[FMath::RandHelper(D.OneOf.Num())].Get() : D.Item.Get();
		if (!Got) continue;
		int32 Qty = FMath::RandRange(D.Min, D.Max);
		if (Got == ActCoins() && FMath::FRand() < LootBonus) Qty *= 2;
		S->Inventory->Add(Got, Qty);
	}
	Msg(FText::Format(LOCTEXT("Picked", "You pick the {0}'s pocket."), T->DisplayName));
	ActGiveXp(S, TEXT("thieving"), T->Xp * (1.f + UGielinorRules::Perk(S, TEXT("thieving"), TEXT("thieveXp"))));
}

// ------------------------------------------------------------------ runecrafting
FText ARuneAltar::GetInteractLabel() const
{
	const URuneAltarDef* A = ActDb() ? ActDb()->FindRuneAltar(AltarId) : nullptr;
	return FText::Format(LOCTEXT("Craft", "Craft-rune {0}"), A ? A->DisplayName : DisplayName);
}

void ARuneAltar::Interact(AActor* InInstigator)
{
	AGielinorPlayerState* S = PS(InInstigator);
	const URuneAltarDef* A = ActDb() ? ActDb()->FindRuneAltar(AltarId) : nullptr;
	if (!S || !A || !A->Rune) { Msg(LOCTEXT("Nothing", "Nothing happens.")); return; }
	const int32 L = S->Skills->GetLevelById(TEXT("runecrafting"));
	if (L < A->Level) { Msg(FText::Format(LOCTEXT("RcLvl", "You need a Runecrafting level of {0} to craft {1}s."), A->Level, A->Rune->DisplayName)); return; }
	int32 Used = 0;
	for (const TCHAR* K : { TEXT("pure_essence"), TEXT("rune_essence") })
	{
		if (A->bPureEssenceOnly && FCString::Strcmp(K, TEXT("rune_essence")) == 0) continue;
		if (UItemDef* E = ActDb()->FindItem(K)) { const int32 N = S->Inventory->CountOf(E); if (N > 0) Used += S->Inventory->Remove(E, N); }
	}
	if (Used == 0)
	{
		Msg(A->bPureEssenceOnly ? LOCTEXT("NeedPure", "You need pure essence to craft these runes. Mine it at Mining 30 in the essence mine.")
		                        : LOCTEXT("NeedEss", "You don't have any rune essence. Buy some from a magic shop or mine it."));
		return;
	}
	int32 Qty = Used * UGielinorRules::RunesPerEssence(A, L);
	const float Dbl = UGielinorRules::Perk(S, TEXT("runecrafting"), TEXT("runeDouble"));
	if (Dbl > 0.f) for (int32 i = 0; i < Used; ++i) if (FMath::FRand() < Dbl) ++Qty;
	S->Inventory->Add(A->Rune, Qty);
	Msg(FText::Format(LOCTEXT("Crafted", "You bind the temple's power into {0} x {1}."), Qty, A->Rune->DisplayName));
	ActGiveXp(S, TEXT("runecrafting"), Used * A->XpPerEssence * (1.f + UGielinorRules::Perk(S, TEXT("runecrafting"), TEXT("rcXp"))));
}

// ------------------------------------------------------------------ prayer altar
FText APrayerAltar::GetInteractLabel() const { return LOCTEXT("Pray", "Pray-at Altar"); }

void APrayerAltar::Interact(AActor* InInstigator)
{
	AGielinorPlayerState* S = PS(InInstigator);
	if (!S || !S->Stats) return;
	const int32 Got = S->Stats->RechargePrayer();
	Msg(Got > 0 ? LOCTEXT("Recharged", "You recharge your prayer points.") : LOCTEXT("Full", "You already have full prayer points."));
}

// ------------------------------------------------------------------ shops
UShopDef* AShopCounter::GetShop() const { return ActDb() ? ActDb()->FindShop(ShopId) : nullptr; }

FText AShopCounter::GetInteractLabel() const
{
	const UShopDef* Sh = GetShop();
	return FText::Format(LOCTEXT("Trade", "Trade {0}"), Sh ? Sh->DisplayName : DisplayName);
}

void AShopCounter::Interact(AActor* InInstigator)
{
	if (!GetShop()) { Msg(LOCTEXT("Closed", "The shop is closed.")); return; }
	OnShopOpened.Broadcast(this, InInstigator);
}

TArray<int32> AShopCounter::VisibleStock(AActor* Player) const
{
	TArray<int32> Out;
	const UShopDef* Sh = GetShop();
	AGielinorPlayerState* S = PS(Player);
	if (!Sh || !S) return Out;
	const bool bQuestCape = ActDb() && S->Quests->QuestPoints() >= ActDb()->TotalQuestPoints();
	for (int32 i = 0; i < Sh->Stock.Num(); ++i)
	{
		const FShopStock& E = Sh->Stock[i];
		if (!E.Item) continue;
		if (!E.UnlockQuest.IsNone())
		{
			const UQuestDef* Q = ActDb()->FindQuest(E.UnlockQuest);
			if (!Q || S->Quests->GetState(Q) != EQuestState::Completed) continue;
		}
		if (!E.RequiresSkill99.IsNone() && S->Skills->GetLevelById(E.RequiresSkill99) < 99) continue;
		if (E.bRequiresQuestCape && !bQuestCape) continue;
		Out.Add(i);
	}
	return Out;
}

int32 AShopCounter::PriceOf(AActor* Player, int32 StockIndex) const
{
	const UShopDef* Sh = GetShop();
	if (!Sh || !Sh->Stock.IsValidIndex(StockIndex)) return 0;
	const FShopStock& E = Sh->Stock[StockIndex];
	const int32 Base = E.Price > 0 ? E.Price : (E.Item ? FMath::Max(1, E.Item->Value) : 1);
	return UGielinorRules::BuyPrice(PS(Player), Base);
}

bool AShopCounter::Buy(AActor* Player, int32 StockIndex, int32 Amount)
{
	AGielinorPlayerState* S = PS(Player);
	const UShopDef* Sh = GetShop();
	if (!S || !Sh || Amount <= 0 || !VisibleStock(Player).Contains(StockIndex)) return false;
	UItemDef* It = Sh->Stock[StockIndex].Item;
	const int32 Each = PriceOf(Player, StockIndex);
	UItemDef* Gold = ActCoins();
	const int32 Afford = Each > 0 ? S->Inventory->CountOf(Gold) / Each : Amount;
	const int32 N = FMath::Min(Amount, Afford);
	if (N <= 0) { Msg(LOCTEXT("Poor", "You don't have enough coins.")); return false; }
	if (!S->Inventory->CanAdd(It, N)) { Msg(LOCTEXT("NoRoom", "You don't have enough inventory space.")); return false; }
	S->Inventory->Remove(Gold, N * Each);
	if (It->ToolType != EToolType::None && S->Stats && S->Stats->AddTool(It))
		Msg(FText::Format(LOCTEXT("ToolBelt", "The {0} goes on your toolbelt."), It->DisplayName));
	else
		S->Inventory->Add(It, N, false);
	return true;
}

bool AShopCounter::Sell(AActor* Player, UItemDef* SoldItem, int32 Amount)
{
	AGielinorPlayerState* S = PS(Player);
	UItemDef* Gold = ActCoins();
	if (!S || !SoldItem || !Gold || SoldItem == Gold || Amount <= 0) return false;
	const int32 N = S->Inventory->Remove(SoldItem, FMath::Min(Amount, S->Inventory->CountOf(SoldItem)));
	if (N <= 0) return false;
	S->Inventory->Add(Gold, N * UGielinorRules::SellPrice(S, SoldItem), false);
	return true;
}

// ------------------------------------------------------------------ places
AQuestPlaceVolume::AQuestPlaceVolume()
{
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	RootComponent = Box;
	Box->SetBoxExtent(FVector(400, 400, 200));
	Box->SetCollisionProfileName(TEXT("Trigger"));
	Box->SetGenerateOverlapEvents(true);
}

void AQuestPlaceVolume::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsPlayerControlled() || PlaceId.IsNone()) return;
	if (UGielinorEvents* Ev = UGielinorEvents::Get(this))
	{
		if (!DisplayName.IsEmpty()) Ev->Message(FText::Format(LOCTEXT("Enter", "You arrive at {0}."), DisplayName));
		Ev->OnPlaceVisited.Broadcast(PlaceId);
	}
}

#undef LOCTEXT_NAMESPACE
