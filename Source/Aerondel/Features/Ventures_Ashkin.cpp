#include "Features/VenturesFeature.h"
#include "Features/AshkinSpirit.h"
#include "Engine/World.h"
#include "UI/AerondelPanels.h"
#include "Core/AerondelPlayerState.h"
#include "Core/PlayerStatsComponent.h"
#include "Combat/AerondelMob.h"
#include "Combat/HealthComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// ============================================================================================
// Ventures group 9 (web adapt/ashkin.js): temporary Prayer-cost spirits that fight beside you.
// Not companions (no Bond). Shared follower cap: one companion plus one Ashkin or one Kinbeast,
// never both a spirit and a pet. Kinbeasts (Phase 15) will set bKinOut / RecallKin.
// ============================================================================================

namespace VenturesAsh
{
	struct FAshDef
	{
		const TCHAR* Id; const TCHAR* Item; const TCHAR* Name; const TCHAR* Style;
		int32 Prayer = 8; int32 CdMs = 40000; int32 DurMs = 45000; int32 PrayerLvl = 1; int32 Gold = 50;
		const TCHAR* MatA; int32 MatAN = 0; const TCHAR* MatB; int32 MatBN = 0;
		const TCHAR* Desc;
	};
	static const FAshDef Ashes[] = {
		{ TEXT("shades"), TEXT("Ember ash"),    TEXT("Ashkin shades"), TEXT("pack"),   8,  40000, 45000,  1,  50, TEXT("Bones"), 5, nullptr, 0, TEXT("A brief pack of weaker shades.") },
		{ TEXT("ward"),   TEXT("Bulwark ash"),  TEXT("Ashkin warder"), TEXT("melee"), 14,  50000, 50000, 20, 200, TEXT("Bones"), 8, TEXT("Iron bar"), 1, TEXT("A shield-bearer that blunts incoming blows.") },
		{ TEXT("ranger"), TEXT("Longshot ash"), TEXT("Ashkin ranger"), TEXT("ranged"), 12, 45000, 50000, 30, 200, TEXT("Bones"), 8, TEXT("Bronze arrows"), 15, TEXT("A ranger spirit that looses short volleys.") },
	};
	static const FAshDef* FindAshItem(const FString& Item) { for (const FAshDef& A : Ashes) if (Item == A.Item) return &A; return nullptr; }
	static const FAshDef* FindAshId(const FString& Id) { for (const FAshDef& A : Ashes) if (Id == A.Id) return &A; return nullptr; }
	static void VAct9(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
}
using namespace VenturesAsh;

void UVenturesFeature::ResetAsh()
{
	AshOwned.Reset();
	AshEquipped.Reset();
	AshLastUsedMs = 0;
	AshLive = FAshLive();
	bKinOut = false;
}

void UVenturesFeature::SaveAsh(FJsonObject& A) const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	PutStrArr(*S, TEXT("owned"), AshOwned);
	S->SetStringField(TEXT("equipped"), AshEquipped);
	S->SetNumberField(TEXT("lastUsed"), (double)AshLastUsedMs);
	A.SetObjectField(TEXT("ashkin"), S);
}

void UVenturesFeature::LoadAsh(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> S = JObj(A, TEXT("ashkin"));
	if (!S) return;
	for (const FString& It : JStrArr(*S, TEXT("owned"))) if (FindAshItem(It) && AshOwned.Num() < 20) AshOwned.AddUnique(It);
	AshEquipped = JStr(*S, TEXT("equipped"));
	if (!AshOwned.Contains(AshEquipped)) AshEquipped = AshOwned.Num() ? AshOwned[0] : FString();
	AshLastUsedMs = JInt(*S, TEXT("lastUsed"));
}

bool UVenturesFeature::AshOut() const { return AshLive.bOn && NowMs() < AshLive.UntilMs; }
bool UVenturesFeature::KinOut() const { return bKinOut; }

void UVenturesFeature::RecallKin(const FString& Why)
{
	if (!bKinOut) return;
	bKinOut = false;
	Say(Why.IsEmpty() ? TEXT("Your Kinbeast is recalled.") : Why);
}

bool UVenturesFeature::AshRecall(const FString& Why, bool bSilent)
{
	if (!AshLive.bOn) return false;
	if (AActor* S = AshLive.Spirit.Get()) S->Destroy();
	AshLive = FAshLive();
	if (!bSilent) Say(Why.IsEmpty() ? TEXT("The Ashkin fades.") : Why);
	return true;
}

bool UVenturesFeature::AshSummon(const FString& InItem)
{
	if (AshOut()) { AshRecall(TEXT("You recall the Ashkin.")); return true; }
	for (const FAshDef& D : Ashes) if (CountItem(D.Item) > 0) AshOwned.AddUnique(D.Item);
	FString Item = InItem;
	if (Item.IsEmpty()) Item = AshEquipped;
	if (Item.IsEmpty() && AshOwned.Num()) Item = AshOwned[0];
	if (Item.IsEmpty() || !AshOwned.Contains(Item)) { Say(TEXT("Choose an Ashkin ash first (Ventures -> Ashkin).")); return false; }
	const FAshDef* Def = FindAshItem(Item);
	if (!Def) return false;
	if (SkillLevel(TEXT("Prayer")) < Def->PrayerLvl)
	{
		Say(FString::Printf(TEXT("You need Prayer %d to call %s."), Def->PrayerLvl, Def->Name));
		return false;
	}
	const int64 Wait = (AshLastUsedMs + Def->CdMs) - NowMs();
	if (Wait > 0) { Say(FString::Printf(TEXT("The ashes are still cooling (%llds)."), (Wait + 999) / 1000)); return false; }
	AAerondelPlayerState* PS = GetPS();
	if (!PS || !PS->Stats || PS->Stats->PrayerPoints < Def->Prayer)
	{
		Say(FString::Printf(TEXT("You need %d Prayer points."), Def->Prayer));
		return false;
	}
	if (KinOut()) RecallKin(TEXT("Your Kinbeast is recalled so the Ashkin can come forth."));
	PS->Stats->PrayerPoints -= Def->Prayer;
	AshEquipped = Item;
	AshLastUsedMs = NowMs();
	AshLive.Item = Item;
	AshLive.Style = Def->Style;
	AshLive.Name = Def->Name;
	AshLive.UntilMs = NowMs() + Def->DurMs;
	AshLive.NextTick = TickNow();
	AshLive.bOn = true;
	if (UWorld* World = GetWorld())
	{
		APawn* Pawn = PS->GetPawn();
		FActorSpawnParameters Sp; Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector At = Pawn ? (Pawn->GetActorLocation() + Pawn->GetActorRightVector() * -80.f + FVector(0.f, 0.f, 110.f)) : FVector::ZeroVector;
		if (AAshkinSpirit* Spirit = World->SpawnActor<AAshkinSpirit>(At, FRotator::ZeroRotator, Sp))
		{
			Spirit->Setup(Pawn, Def->Style);
			AshLive.Spirit = Spirit;
		}
	}
	Say(FString::Printf(TEXT("You call forth %s. They will fade in %ds. One Ashkin or Kinbeast at a time."), Def->Name, Def->DurMs / 1000));
	return true;
}

bool UVenturesFeature::AshCraft(const FString& Id)
{
	const FAshDef* Def = FindAshId(Id);
	if (!Def) return false;
	if (SkillLevel(TEXT("Prayer")) < Def->PrayerLvl) { Say(FString::Printf(TEXT("You need Prayer %d."), Def->PrayerLvl)); return false; }
	if (Coins() < Def->Gold) { Say(FString::Printf(TEXT("That costs %d coins."), Def->Gold)); return false; }
	if (CountItem(Def->MatA) < Def->MatAN || (Def->MatB && CountItem(Def->MatB) < Def->MatBN))
	{
		FString Need = FString::Printf(TEXT("%d %s"), Def->MatAN, Def->MatA);
		if (Def->MatB) Need += FString::Printf(TEXT(", %d %s"), Def->MatBN, Def->MatB);
		Say(FString::Printf(TEXT("You need %s."), *Need));
		return false;
	}
	TakeItem(Def->MatA, Def->MatAN);
	if (Def->MatB) TakeItem(Def->MatB, Def->MatBN);
	if (!TakeCoins(Def->Gold)) return false;
	GiveItem(Def->Item, 1);
	AshOwned.AddUnique(Def->Item);
	if (AshEquipped.IsEmpty()) AshEquipped = Def->Item;
	Say(FString::Printf(TEXT("You bind %s."), Def->Item));
	return true;
}

void UVenturesFeature::AshEquip(const FString& Item)
{
	if (!AshOwned.Contains(Item) && CountItem(Item) <= 0) return;
	AshOwned.AddUnique(Item);
	AshEquipped = Item;
}

int32 UVenturesFeature::WardAsh(int32 Amount) const
{
	if (Amount <= 0 || !AshOut() || AshLive.Style != TEXT("melee")) return Amount;
	return FMath::Max(1, Amount - 1);
}

int32 UVenturesFeature::FilterIncoming(const UObject* WorldContext, int32 Amount)
{
	const UVenturesFeature* V = Get(WorldContext);
	return V ? V->WardAsh(Amount) : Amount;
}

void UVenturesFeature::TickAsh()
{
	if (!AshLive.bOn) return;
	if (NowMs() >= AshLive.UntilMs) { AshRecall(TEXT("The Ashkin fades.")); return; }
	AAerondelMob* Mon = CurrentTarget();
	if (!Mon || Mon->GetMaxHp() <= 0) return;
	FVector2D Pos;
	if (!PlayerWebPos(Pos)) return;
	const FVector MLoc = Mon->GetActorLocation();
	const float Dist = FVector2D::Distance(Pos, FVector2D(MLoc.X / 100.0, MLoc.Y / 100.0));
	const float Reach = AshLive.Style == TEXT("ranged") ? 16.f : 12.f;
	if (Dist > Reach) return;
	if (TickNow() < AshLive.NextTick) return;
	AshLive.NextTick = TickNow() + (AshLive.Style == TEXT("pack") ? 2 : 3);
	const int32 Cap = AshLive.Style == TEXT("ranged") ? 4 : 3;
	const int32 Dmg = FMath::Min(Mon->GetHp(), 1 + FMath::RandRange(0, Cap));
	if (Dmg > 0 && Mon->Health)
	{
		const AAerondelPlayerState* PS = GetPS();
		Mon->Health->ApplyDamage(Dmg, PS ? PS->GetPawn() : nullptr); // raw assist; not a player swing (no Crestbreak/Overflare/Hunt fill)
	}
}

void UVenturesFeature::TabAsh(FGPanelModel& M)
{
	M.Title = TEXT("Ashkin");
	M.Lines.Add(TEXT("Craft or bind a spirit ash, then summon it for a short time at a Prayer cost (H). Ashkin are not companions — no Bond, no errands."));
	M.Lines.Add(TEXT("Follower cap: one companion plus one Ashkin or one Kinbeast, never a spirit and a pet together."));
	if (KinOut()) M.Lines.Add(TEXT("A Kinbeast is out — summoning an Ashkin will recall it."));
	if (AshOut()) M.Lines.Add(FString::Printf(TEXT("%s is out."), *AshLive.Name));
	for (const FAshDef& D : Ashes)
	{
		if (CountItem(D.Item) > 0) AshOwned.AddUnique(D.Item);
		const bool bHave = AshOwned.Contains(D.Item);
		const bool bEq = AshEquipped == D.Item;
		const bool bLock = SkillLevel(TEXT("Prayer")) < D.PrayerLvl;
		M.Lines.Add(FString::Printf(TEXT("%s — %s · %d Prayer · %ds. %s Craft: %d %s%s and %d coins."),
			D.Name,
			bHave ? (bEq ? TEXT("equipped") : TEXT("owned")) : *FString::Printf(TEXT("Prayer %d%s"), D.PrayerLvl, bLock ? TEXT(" (locked)") : TEXT("")),
			D.Prayer, D.DurMs / 1000, D.Desc, D.MatAN, D.MatA,
			D.MatB ? *FString::Printf(TEXT(", %d %s"), D.MatBN, D.MatB) : TEXT(""),
			D.Gold));
		const FString Id = D.Id, Item = D.Item;
		if (bHave) VAct9(M, bEq ? FString::Printf(TEXT("%s (equipped)"), D.Item) : FString::Printf(TEXT("Equip %s"), D.Item), [this, Item] { AshEquip(Item); });
		else VAct9(M, FString::Printf(TEXT("Bind %s"), D.Item), [this, Id] { AshCraft(Id); }, !bLock);
	}
	VAct9(M, AshOut() ? TEXT("Recall (H)") : TEXT("Summon (H)"), [this] { AshSummon(); }, true, true);
}
