#include "Features/SeasonalEventsFeature.h"
#include "Core/AerondelEvents.h"
#include "Data/MobDef.h"
#include "Data/SkillDef.h"
#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "World/AerondelScalability.h"

namespace EventData
{
	struct FShop { const TCHAR* Item; const TCHAR* Stats; int32 Cost; };
	struct FEvent { const TCHAR* Id; const TCHAR* Name; const TCHAR* Season; bool bSpecial; int32 From; int32 To; const TCHAR* Token; const TCHAR* Deco; const TCHAR* Blurb; FShop Shop[2]; int32 NumShop; };
	static const FEvent Events[] = {
		{ TEXT("bloomtide"), TEXT("Bloomtide"), TEXT("spring"), false, 0, 0, TEXT("Bloom petal"), TEXT("flowers"), TEXT("Larkhaven strews the square with blossoms to wake the fields."),
			{ { TEXT("Bloomcrown"), TEXT("+4 def, +4 rdef, +2 mag"), 60 }, { TEXT("Honeyed bun"), TEXT(""), 4 } }, 2 },
		{ TEXT("midsummer"), TEXT("Midsummer Fair"), TEXT("summer"), false, 0, 0, TEXT("Fair ticket"), TEXT("bunting"), TEXT("Stalls, games and the long light of the year's longest days."),
			{ { TEXT("Fairgoer's straw hat"), TEXT("+3 def, +3 rdef, +2 ranged"), 60 }, { TEXT("Candied apple"), TEXT(""), 4 } }, 2 },
		{ TEXT("harvest"), TEXT("Harvest Moon"), TEXT("autumn"), false, 0, 0, TEXT("Harvest sheaf"), TEXT("pumpkins"), TEXT("The fields are in. Aerondel gives thanks under a fat orange moon."),
			{ { TEXT("Sheaf-crown"), TEXT("+4 def, +4 rdef, +2 str"), 60 }, { TEXT("Harvest pie"), TEXT(""), 5 } }, 2 },
		{ TEXT("frostfall"), TEXT("Frostfall"), TEXT("winter"), false, 0, 0, TEXT("Frost star"), TEXT("snowmen"), TEXT("Snow, lanterns and the longest night; gifts are left on every doorstep."),
			{ { TEXT("Frostfall toque"), TEXT("+4 def, +5 rdef, +2 atk"), 60 }, { TEXT("Spiced cider"), TEXT(""), 4 } }, 2 },
		{ TEXT("lantern"), TEXT("Lantern Night"), TEXT(""), true, 1024, 1107, TEXT("Ghost lantern"), TEXT("lanterns"), TEXT("For two weeks the dead walk lightly; lanterns guide them home. Kills drop twice as many tokens."),
			{ { TEXT("Hollow-eyed mask"), TEXT("+5 def, +5 rdef, +3 mag"), 90 }, { TEXT("Grave-cake"), TEXT(""), 6 } }, 2 },
		{ TEXT("founders"), TEXT("Founders' Week"), TEXT(""), true, 601, 610, TEXT("Founder's seal"), TEXT("bunting"), TEXT("Aerondel remembers the first walls of Larkhaven."),
			{ { TEXT("Founder's circlet"), TEXT("+5 def, +5 rdef, +2 atk, +2 str"), 120 }, { TEXT(""), TEXT(""), 0 } }, 1 } };
	static const TCHAR* DailySkills[] = { TEXT("Woodcutting"), TEXT("Mining"), TEXT("Fishing"), TEXT("Cooking"), TEXT("Smithing"), TEXT("Crafting"), TEXT("Fletching"), TEXT("Herbalism"), TEXT("Agility"), TEXT("Thieving") };
	static const TCHAR* AllSkills[] = { TEXT("Attack"), TEXT("Strength"), TEXT("Defence"), TEXT("Ranged"), TEXT("Prayer"), TEXT("Magic"), TEXT("Woodcutting"), TEXT("Mining"), TEXT("Fishing"),
		TEXT("Cooking"), TEXT("Smithing"), TEXT("Crafting"), TEXT("Fletching"), TEXT("Herbalism"), TEXT("Agility"), TEXT("Thieving"), TEXT("Glyphcraft"), TEXT("Relicry"), TEXT("Delving") };
	static bool Valid(int32 I) { return I >= 0 && I < (int32)UE_ARRAY_COUNT(Events); }
	static FString Season(int32 Month) { return Month >= 3 && Month <= 5 ? TEXT("spring") : Month >= 6 && Month <= 8 ? TEXT("summer") : Month >= 9 && Month <= 11 ? TEXT("autumn") : TEXT("winter"); }
}

void USeasonalEventsFeature::Initialize(FSubsystemCollectionBase& Collection)
{
	UAerondelEvents* Ev = Collection.InitializeDependency<UAerondelEvents>();
	Super::Initialize(Collection);
	if (Ev) { Ev->OnMobKilled.AddDynamic(this, &USeasonalEventsFeature::HandleMobKilled); Ev->OnXpGained.AddDynamic(this, &USeasonalEventsFeature::HandleXp); }
}

int32 USeasonalEventsFeature::NumEvents() { return (int32)UE_ARRAY_COUNT(EventData::Events); }
FString USeasonalEventsFeature::EventName(int32 I) { return EventData::Valid(I) ? EventData::Events[I].Name : TEXT(""); }
FString USeasonalEventsFeature::EventToken(int32 I) { return EventData::Valid(I) ? EventData::Events[I].Token : TEXT(""); }
FString USeasonalEventsFeature::EventDeco(int32 I) { return EventData::Valid(I) ? EventData::Events[I].Deco : TEXT(""); }
bool USeasonalEventsFeature::IsSpecial(int32 I) { return EventData::Valid(I) && EventData::Events[I].bSpecial; }
int32 USeasonalEventsFeature::NumShopItems(int32 I) { return EventData::Valid(I) ? EventData::Events[I].NumShop : 0; }
FString USeasonalEventsFeature::Today() { return FDateTime::Now().ToString(TEXT("%Y-%m-%d")); }

TArray<int32> USeasonalEventsFeature::ActiveIndices()
{
	const FDateTime D = FDateTime::Now();
	const int32 V = D.GetMonth() * 100 + D.GetDay();
	const FString S = EventData::Season(D.GetMonth());
	TArray<int32> O;
	for (int32 I = 0; I < NumEvents(); ++I)
	{
		const EventData::FEvent& E = EventData::Events[I];
		const bool bIn = E.bSpecial ? (E.From <= E.To ? (V >= E.From && V <= E.To) : (V >= E.From || V <= E.To)) : S == E.Season;
		if (bIn) O.Add(I);
	}
	return O;
}

TArray<FString> USeasonalEventsFeature::ActiveEventIds() const { TArray<FString> O; for (int32 I : ActiveIndices()) O.Add(EventData::Events[I].Id); return O; }

void USeasonalEventsFeature::ResetState() { Day.Reset(); Kind.Reset(); TaskSkill.Reset(); N = C = 0; Base = 0.0; bDone = false; Seen.Reset(); Total = 0; bAnnounced = false; }

void USeasonalEventsFeature::SaveState(FJsonObject& Out) const
{
	if (!Day.IsEmpty())
	{
		TSharedPtr<FJsonObject> D = MakeShared<FJsonObject>();
		D->SetStringField(TEXT("day"), Day); D->SetStringField(TEXT("kind"), Kind);
		if (!TaskSkill.IsEmpty()) D->SetStringField(TEXT("skill"), TaskSkill);
		D->SetNumberField(TEXT("n"), N); D->SetNumberField(TEXT("base"), Base); D->SetNumberField(TEXT("c"), C); D->SetBoolField(TEXT("done"), bDone);
		Out.SetObjectField(TEXT("daily"), D);
	}
	PutStrArr(Out, TEXT("seen"), Seen); Out.SetNumberField(TEXT("total"), Total);
}

void USeasonalEventsFeature::LoadState(const FJsonObject& In)
{
	ResetState();
	if (TSharedPtr<FJsonObject> D = JObj(In, TEXT("daily")))
	{
		Day = JStr(*D, TEXT("day")); Kind = JStr(*D, TEXT("kind")); if (Kind.IsEmpty()) Kind = TEXT("kill");
		TaskSkill = JStr(*D, TEXT("skill")); N = FMath::Max<int32>(1, JInt(*D, TEXT("n"))); Base = JNum(*D, TEXT("base"));
		C = FMath::Max<int32>(0, JInt(*D, TEXT("c"))); bDone = JBool(*D, TEXT("done"));
	}
	Seen = JStrArr(In, TEXT("seen")); if (Seen.Num() > 20) Seen.RemoveAt(0, Seen.Num() - 20);
	Total = FMath::Max<int32>(0, JInt(In, TEXT("total")));
}

void USeasonalEventsFeature::RefreshDecor()
{
	UWorld* W = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr; if (!W) return;
	const TArray<FString> Act = ActiveEventIds();
	const EAerondelTier T = UAerondelScalability::GetSavedTier();
	const bool bLight = T == EAerondelTier::Low || T == EAerondelTier::Quest;
	static const FString Prefix = TEXT("festival:");
	for (TActorIterator<AActor> It(W); It; ++It)
	{
		AActor* A = *It; if (A->Tags.Num() == 0) continue;
		FString Id; bool bFull = false;
		for (const FName& Tag : A->Tags) { const FString S = Tag.ToString(); if (S.StartsWith(Prefix)) Id = S.Mid(Prefix.Len()); else if (S == TEXT("festival_full")) bFull = true; }
		if (Id.IsEmpty()) continue;
		const bool bHide = !Act.Contains(Id) || (bFull && bLight);
		if (A->IsHidden() != bHide) { A->SetActorHiddenInGame(bHide); A->SetActorEnableCollision(!bHide); }
	}
}

void USeasonalEventsFeature::OnGameTick()
{
	if (--DecorTicks <= 0) { DecorTicks = 25; RefreshDecor(); } // ~15 s: catches World Partition cells streaming in
	if (bAnnounced) return;
	FVector2D P; if (!PlayerWebPos(P)) return; // wait for a pawn so the message is seen
	bAnnounced = true;
	const FString Year = FString::FromInt(FDateTime::Now().GetYear());
	for (int32 I : ActiveIndices())
	{
		const EventData::FEvent& E = EventData::Events[I];
		const FString Key = FString(E.Id) + Year;
		if (Seen.Contains(Key)) continue;
		Seen.Add(Key); if (Seen.Num() > 20) Seen.RemoveAt(0);
		Say(FString::Printf(TEXT("%s has begun! %s Visit Festival Herald Odelyn in Larkhaven square."), E.Name, E.Blurb));
	}
}

void USeasonalEventsFeature::EnsureDaily()
{
	const FString T = Today();
	if (Day == T) return;
	Day = T; C = 0; bDone = false; Base = 0.0;
	if (FMath::FRand() < 0.5f) { Kind = TEXT("kill"); TaskSkill.Reset(); N = 12; }
	else
	{
		Kind = TEXT("xp");
		TaskSkill = EventData::DailySkills[FMath::RandRange(0, (int32)UE_ARRAY_COUNT(EventData::DailySkills) - 1)];
		N = 1500 + SkillLevel(TaskSkill) * 40;
	}
}

// Unreal counts task XP through OnXpGained into C (the web diffs the skill's total XP against base).
void USeasonalEventsFeature::HandleXp(USkillDef* Skill, float Amount, float TotalXp, int32 Level)
{
	if (bDone || Kind != TEXT("xp") || Day != Today() || !Skill) return;
	if (Skill->Id == Id(TaskSkill)) C += FMath::FloorToInt(Amount);
}

void USeasonalEventsFeature::HandleMobKilled(UMobDef* Mob)
{
	const TArray<int32> L = ActiveIndices(); if (L.Num() == 0) return;
	if (!bDone && Kind == TEXT("kill") && Day == Today()) ++C;
	for (int32 I : L)
	{
		const EventData::FEvent& E = EventData::Events[I];
		if (FMath::FRand() < (E.bSpecial ? 0.3f : 0.15f))
		{
			GiveItem(E.Token, E.bSpecial ? 2 : 1);
			Say(E.bSpecial ? FString::Printf(TEXT("You find two %ss."), *FString(E.Token).ToLower()) : FString::Printf(TEXT("You find a %s."), *FString(E.Token).ToLower()));
		}
	}
}

FString USeasonalEventsFeature::WeakestSkill() const
{
	FString Best = TEXT("Attack"); int32 BestL = MAX_int32;
	for (const TCHAR* S : EventData::AllSkills) { const int32 L = SkillLevel(S); if (L < BestL) { BestL = L; Best = S; } }
	return Best;
}

FString USeasonalEventsFeature::Greeting() const
{
	const TArray<int32> L = ActiveIndices();
	if (L.Num() == 0) return TEXT("Festival Herald Odelyn: No festival right now. Come back when the season turns!");
	TArray<FString> Names, Blurbs;
	for (int32 I : L) { Names.Add(EventData::Events[I].Name); Blurbs.Add(FString::Printf(TEXT("%s: %s"), EventData::Events[I].Name, EventData::Events[I].Blurb)); }
	return FString::Printf(TEXT("Festival Herald Odelyn: Welcome to %s! %s"), *FString::Join(Names, TEXT(" and ")), *FString::Join(Blurbs, TEXT(" ")));
}

FString USeasonalEventsFeature::TaskText()
{
	if (ActiveIndices().Num() == 0) return TEXT("No festival running.");
	EnsureDaily();
	if (bDone) return TEXT("Today's task is done. A new one tomorrow, friend.");
	return Kind == TEXT("xp") ? FString::Printf(TEXT("Earn %d %s XP (%d/%d)"), N, *TaskSkill, FMath::Min(C, N), N)
		: FString::Printf(TEXT("Defeat %d monsters (%d/%d)"), N, FMath::Min(C, N), N);
}

FString USeasonalEventsFeature::TurnInTask()
{
	const TArray<int32> L = ActiveIndices();
	if (L.Num() == 0) return TEXT("No festival right now.");
	EnsureDaily();
	if (bDone) return TEXT("A new task tomorrow, friend.");
	if (C < N) return TEXT("Not yet: ") + TaskText() + TEXT(".");
	bDone = true; ++Total;
	for (int32 I : L) GiveItem(EventData::Events[I].Token, EventData::Events[I].bSpecial ? 15 : 10);
	const FString S = WeakestSkill(); GiveXp(S, 400.f + SkillLevel(S) * 30.f);
	return FString::Printf(TEXT("Splendid! Tokens for every festival running, and a blessing on your %s. New task tomorrow."), *S);
}

FString USeasonalEventsFeature::ShopLabel(int32 EI, int32 II) const
{
	if (!EventData::Valid(EI) || II < 0 || II >= EventData::Events[EI].NumShop) return FString();
	const EventData::FEvent& E = EventData::Events[EI]; const EventData::FShop& S = E.Shop[II];
	const FString St = FString(S.Stats).IsEmpty() ? FString() : FString::Printf(TEXT(" (%s)"), S.Stats);
	return FString::Printf(TEXT("%s - %d %ss%s"), S.Item, S.Cost, *FString(E.Token).ToLower(), *St);
}

FString USeasonalEventsFeature::Buy(int32 EI, int32 II)
{
	if (!EventData::Valid(EI) || II < 0 || II >= EventData::Events[EI].NumShop || !ActiveIndices().Contains(EI)) return TEXT("That stall is closed.");
	const EventData::FEvent& E = EventData::Events[EI]; const EventData::FShop& S = E.Shop[II];
	const int32 Have = CountItem(E.Token);
	if (Have < S.Cost) return FString::Printf(TEXT("That's %d %ss; you have %d."), S.Cost, *FString(E.Token).ToLower(), Have);
	TakeItem(E.Token, S.Cost); GiveItem(S.Item, 1);
	return FString::Printf(TEXT("The %s, yours to keep!"), S.Item);
}

FString USeasonalEventsFeature::Blessing(int32 EI)
{
	if (!EventData::Valid(EI) || !ActiveIndices().Contains(EI)) return TEXT("That stall is closed.");
	const EventData::FEvent& E = EventData::Events[EI];
	if (CountItem(E.Token) < 25) return TEXT("You need 25 for a blessing.");
	const FString S = WeakestSkill(); TakeItem(E.Token, 25);
	const int32 Xp = 1000 + SkillLevel(S) * 60; GiveXp(S, (float)Xp);
	return FString::Printf(TEXT("%s blessed (+%d XP)."), *S, Xp);
}
