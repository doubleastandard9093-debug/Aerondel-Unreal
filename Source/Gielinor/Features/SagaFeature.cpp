#include "Features/SagaFeature.h"
#include "Features/StandingFeature.h"
#include "Core/GielinorEvents.h"
#include "Core/GielinorPlayerState.h"
#include "Skills/SkillsComponent.h"
#include "Quests/QuestComponent.h"
#include "Data/MobDef.h"
#include "Dom/JsonValue.h"

namespace SagaData
{
	struct FFx { const TCHAR* Label; const TCHAR* Flag; int32 Gold; const TCHAR* RepFac; int32 Rep; const TCHAR* XpSkill; int32 Xp; const TCHAR* Say; };
	struct FChapter { const TCHAR* Title; int32 Total; int32 Qp; const TCHAR* Intro; bool bKill; const TCHAR* Target; int32 N; const TCHAR* Obj; const TCHAR* Done; bool bCompanionTest; int32 VarroHelp; bool bFinale; FFx Wheel[3]; };
	static const FChapter Ch[] = {
		{ TEXT("The Ember Letter"), 0, 0, TEXT("A letter came for you, sealed with an ember. It is from Aldric, the one who taught you to hold a blade. \"If you read this, I am gone. Something stirs under Whitecairn. Find your calling, and follow the road I left.\""),
			true, TEXT("Goblin"), 5, TEXT("Drive off 5 Goblins troubling the Larkhaven farms"), TEXT("One of the goblins you beat is caught alive, clutching a scrap of Aldric's cloak."), false, 0, false,
			{ { TEXT("Let it go. It's only scared."), TEXT("mercy"), 0, nullptr, 0, nullptr, 0, TEXT("It bows and scurries off. Somewhere, someone will remember that.") },
			  { TEXT("Hand it to the Watch."), nullptr, 0, TEXT("watch"), 40, nullptr, 0, TEXT("The Watch thanks you. The goblin will face a fair hearing.") },
			  { TEXT("Tell me where you found that cloak."), TEXT("clue"), 100, nullptr, 0, nullptr, 0, TEXT("It squeaks out \"ash town\" and a purse of stolen coin.") } } },
		{ TEXT("Ashes at Emberfold"), 50, 2, TEXT("Aldric passed through Emberfold before it burned. The refugees there are sleeping in the cold."),
			false, TEXT("Cowhide"), 5, TEXT("Bring 5 Cowhide for the refugees' tents"), TEXT("The refugees stare at the hides. Their elder asks what you want for them."), false, 0, false,
			{ { TEXT("Nothing. Keep warm."), TEXT("ashfordFriend"), 0, nullptr, 0, nullptr, 0, TEXT("Word spreads. Merchants everywhere give you 5% off from now on.") },
			  { TEXT("Rebuild the wall, and stand your own watch."), nullptr, 0, TEXT("watch"), 60, nullptr, 0, TEXT("They raise a palisade that same night.") },
			  { TEXT("Coin, and anything you know about Aldric."), nullptr, 400, nullptr, 0, nullptr, 0, TEXT("They pay, and mention a bandit called Varro who robbed him.") } } },
		{ TEXT("The Bandit Road"), 80, 5, TEXT("Varro's bandits hold the road west. Aldric's satchel is with them."),
			true, TEXT("Bandit"), 6, TEXT("Break Varro's gang: defeat 6 Bandits"), TEXT("Varro kneels in the dirt and hands back Aldric's satchel. Inside: a map of the Saltreach docks."), false, 0, false,
			{ { TEXT("Go, and do better."), TEXT("varro"), 0, nullptr, 0, nullptr, 0, TEXT("\"I owe you my life,\" Varro says. \"I won't forget it.\"") },
			  { TEXT("You'll answer to the Watch."), nullptr, 0, TEXT("watch"), 80, nullptr, 0, TEXT("Captain Brann locks him up with a nod of respect for you.") },
			  { TEXT("Work for me now."), TEXT("varro"), 200, nullptr, 0, nullptr, 0, TEXT("Varro grins and tosses you his share of the loot. \"Boss.\"") } } },
		{ TEXT("Salt and Smoke"), 110, 8, TEXT("The map leads to a wreck off Saltreach, where the drowned dead walk the hold."),
			true, TEXT("Skeleton"), 8, TEXT("Clear 8 Skeletons from the wreck-haunted shore"), TEXT("Among the bones is Aldric's journal. The Mariners want it for their records, but it names their harbour master as a smuggler."), true, 0, false,
			{ { TEXT("Give them the journal. Let them judge their own."), nullptr, 0, TEXT("mariners"), 60, nullptr, 0, TEXT("The harbour master is quietly replaced.") },
			  { TEXT("Arrest the harbour master. Now."), nullptr, 300, TEXT("mariners"), 30, nullptr, 0, TEXT("The docks go quiet as he is led away.") },
			  { TEXT("Sell the page to the harbour master."), TEXT("smuggler"), 600, nullptr, 0, nullptr, 0, TEXT("He pays well, and owes you one.") } } },
		{ TEXT("The Lantern Archive"), 140, 12, TEXT("Aldric's journal speaks of an archive under Highcrest, sealed with rune-light."),
			false, TEXT("Glyph essence"), 20, TEXT("Bring 20 Glyph essence to wake the archive seals"), TEXT("The seals open. Aldric's last page: he went into the Ironveil Knights' keep near Whitecairn and never came out."), false, 5, false,
			{ { TEXT("Share the archive with the Circle."), nullptr, 0, TEXT("circle"), 80, nullptr, 0, TEXT("The Arcane Circle names you a friend of the archive.") },
			  { TEXT("Seal it again. Some things should stay buried."), nullptr, 0, TEXT("lantern"), 60, nullptr, 0, TEXT("The White Lantern approves.") },
			  { TEXT("Copy the best spells for yourself."), nullptr, 0, nullptr, 0, TEXT("Magic"), 1500, TEXT("Your mind fills with old patterns.") } } },
		{ TEXT("Aldric's Ember"), 180, 16, TEXT("Whitecairn. The keep of the Ironveil Knights. Whatever happened to Aldric is inside."),
			true, TEXT("Ironveil Knight"), 5, TEXT("Fight through 5 Ironveil Knights to reach Aldric"), TEXT("Aldric is alive, chained and thin. He smiles. \"You came. Now: who did you become?\""), false, 0, true,
			{ { nullptr, nullptr, 0, nullptr, 0, nullptr, 0, nullptr }, { nullptr, nullptr, 0, nullptr, 0, nullptr, 0, nullptr }, { nullptr, nullptr, 0, nullptr, 0, nullptr, 0, nullptr } } } };
	struct FCalling { const TCHAR* Id; const TCHAR* Name; const TCHAR* Skills[3]; };
	static const FCalling Callings[] = {
		{ TEXT("warden"), TEXT("Warden"), { TEXT("Attack"), TEXT("Strength"), TEXT("Defence") } },
		{ TEXT("arcanist"), TEXT("Arcanist"), { TEXT("Magic"), TEXT("Glyphcraft"), nullptr } },
		{ TEXT("ranger"), TEXT("Ranger"), { TEXT("Ranged"), TEXT("Agility"), nullptr } } };
	static const FCalling* FindCalling(FName Id) { for (const FCalling& C : Callings) if (Id == FName(C.Id)) return &C; return nullptr; }
	static const TCHAR* EndTitle[] = { TEXT("the Hearthbound"), TEXT("the Unbroken"), TEXT("the Silver-Tongued") };
	static const TCHAR* EndSay[] = { TEXT("You became someone people run toward, not away from. Aldric weeps with pride."),
		TEXT("You held the line every time. Aldric says the realm is safer for it."), TEXT("You bent every deal your way. Aldric laughs: \"Better than I ever did.\"") };
}

int32 USagaFeature::NumChapters() { return UE_ARRAY_COUNT(SagaData::Ch); }

void USagaFeature::Initialize(FSubsystemCollectionBase& Collection)
{
	UGielinorEvents* Ev = Collection.InitializeDependency<UGielinorEvents>();
	Super::Initialize(Collection);
	if (Ev) Ev->OnMobKilled.AddDynamic(this, &USagaFeature::HandleMobKilled);
}

float USagaFeature::XpBonus(FName SkillId) const
{
	if (const SagaData::FCalling* C = SagaData::FindCalling(Calling)) for (const TCHAR* S : C->Skills) if (S && Id(S) == SkillId) return 0.05f;
	return 0.f;
}

bool USagaFeature::ChooseCalling(FName NewCalling)
{
	if (!Calling.IsNone() || !SagaData::FindCalling(NewCalling)) return false;
	Calling = NewCalling; return true;
}

FString USagaFeature::GateText() const
{
	if (Chapter >= NumChapters()) return TEXT("Your story is written.");
	const SagaData::FChapter& C = SagaData::Ch[Chapter];
	const AGielinorPlayerState* PS = GetPS();
	const int32 T = PS && PS->Skills ? PS->Skills->TotalLevel() : 0, Q = PS && PS->Quests ? PS->Quests->QuestPoints() : 0;
	return T >= C.Total && Q >= C.Qp ? FString() : FString::Printf(TEXT("You'll need total level %d and %d quest points (you have %d and %d)."), C.Total, C.Qp, T, Q);
}

int32 USagaFeature::Need() const
{
	if (Chapter >= NumChapters()) return 0;
	const SagaData::FChapter& C = SagaData::Ch[Chapter];
	return C.VarroHelp && HasFlag(TEXT("varro")) ? C.VarroHelp : C.N;
}

FString USagaFeature::ObjectiveText() const { return Chapter < NumChapters() ? FString(SagaData::Ch[Chapter].Obj) : FString(); }

bool USagaFeature::ReadyToFinish() const
{
	if (!bActive || Chapter >= NumChapters()) return false;
	const SagaData::FChapter& C = SagaData::Ch[Chapter];
	return C.bKill ? Prog >= Need() : CountItem(C.Target) >= Need();
}

TArray<FString> USagaFeature::Talk()
{
	TArray<FString> L;
	if (Chapter >= NumChapters()) { L.Add(Title.IsEmpty() ? TEXT("Your story is written.") : FString::Printf(TEXT("Your story is written, Wanderer %s. I read it often."), *Title)); return L; }
	const SagaData::FChapter& C = SagaData::Ch[Chapter];
	if (!bActive)
	{
		const FString G = GateText();
		if (!G.IsEmpty()) { L.Add(FString::Printf(TEXT("Chapter %d, \"%s\", waits for you. %s"), Chapter + 1, C.Title, *G)); return L; }
		if (Chapter == 0 && Calling.IsNone()) { L.Add(TEXT("I am Corwin, and I write down the stories worth keeping. Yours begins today.")); L.Add(C.Intro); L.Add(TEXT("First: who are you, on the road? (Choose a calling)")); return L; }
		bActive = true; Prog = 0;
		L.Add(C.Intro); L.Add(FString(C.Obj) + TEXT("."));
		return L;
	}
	if (!ReadyToFinish()) { L.Add(FString::Printf(TEXT("%s: %d/%d."), C.Obj, C.bKill ? Prog : CountItem(C.Target), Need())); return L; }
	L.Add(C.Done);
	if (!C.bFinale) for (const SagaData::FFx& F : C.Wheel) L.Add(FString::Printf(TEXT("> %s"), F.Label));
	return L;
}

EWheelTone USagaFeature::EndingTone() const
{
	int32 Count[3] = { 0, 0, 0 }; EWheelTone Last = EWheelTone::Kind;
	for (const TPair<int32, EWheelTone>& X : Choices) { Count[(int32)X.Value]++; Last = X.Value; }
	const int32 Max = FMath::Max3(Count[0], Count[1], Count[2]);
	if (Count[(int32)Last] == Max) return Last;
	for (int32 i = 0; i < 3; ++i) if (Count[i] == Max) return (EWheelTone)i;
	return EWheelTone::Kind;
}

FString USagaFeature::Finish(EWheelTone Tone)
{
	if (!ReadyToFinish()) return FString();
	const SagaData::FChapter& C = SagaData::Ch[Chapter];
	if (!C.bKill) TakeItem(C.Target, Need());
	if (C.bFinale)
	{
		const EWheelTone T = EndingTone();
		Title = SagaData::EndTitle[(int32)T];
		Choices.Add(TPair<int32, EWheelTone>(Chapter + 1, T)); ++Chapter; bActive = false; Prog = 0;
		GiveCoins(2000);
		if (const SagaData::FCalling* Cl = SagaData::FindCalling(Calling)) GiveXp(Cl->Skills[0], 5000.f);
		FString Out;
		if (HasFlag(TEXT("mercy"))) Out += TEXT("The goblin you once let go slips through the bars and picks Aldric's lock. ");
		if (HasFlag(TEXT("varro"))) Out += TEXT("Varro is there too, holding the door. \"I said I owed you.\" ");
		if (HasFlag(TEXT("ashfordFriend"))) Out += TEXT("Emberfold sends blankets and bread for the road home. ");
		Out += SagaData::EndSay[(int32)T];
		Say(FString::Printf(TEXT("The Wanderer's Saga is complete. You are now known as %s."), *Title));
		return Out;
	}
	const SagaData::FFx& F = C.Wheel[(int32)Tone];
	Choices.Add(TPair<int32, EWheelTone>(Chapter + 1, Tone)); ++Chapter; bActive = false; Prog = 0;
	if (F.Flag) SetFlag(F.Flag);
	if (F.Gold) GiveCoins(F.Gold);
	if (F.RepFac) if (UStandingFeature* St = Feature<UStandingFeature>()) St->AddRep(FName(F.RepFac), F.Rep);
	if (F.XpSkill) GiveXp(F.XpSkill, (float)F.Xp);
	FString Out = F.Say;
	if (C.bCompanionTest)
		if (UCompanionsFeature* Co = Feature<UCompanionsFeature>())
		{
			const FName A = Co->GetActive(); const FCompanionDef* D = UCompanionsFeature::Def(A);
			if (D)
			{
				if (Tone == D->Likes) { Co->AddBond(A, 2); Out += FString::Printf(TEXT(" %s nods firmly."), *D->Name); }
				else if (Co->AddBond(A, -2) <= 0)
				{
					if (FCompanionState* S = Co->Find(A)) { S->bRecruited = false; S->bStarted = false; S->Task = 0; }
					Co->SendHome(); SetFlag(TEXT("left_") + A.ToString());
					Out += FString::Printf(TEXT(" %s shakes their head and leaves your side."), *D->Name);
				}
				else Out += FString::Printf(TEXT(" %s frowns. They didn't like that."), *D->Name);
			}
		}
	Say(FString::Printf(TEXT("Saga: chapter %d, \"%s\", complete."), Chapter, C.Title));
	return Out;
}

void USagaFeature::HandleMobKilled(UMobDef* Mob)
{
	if (!Mob || !bActive || Chapter >= NumChapters()) return;
	const SagaData::FChapter& C = SagaData::Ch[Chapter];
	if (C.bKill && Mob->Id == Id(C.Target) && Prog < Need()) ++Prog;
}

void USagaFeature::SaveState(FJsonObject& Out) const
{
	Out.SetStringField(TEXT("calling"), Calling.IsNone() ? FString() : Calling.ToString());
	Out.SetNumberField(TEXT("ch"), Chapter); Out.SetBoolField(TEXT("active"), bActive); Out.SetNumberField(TEXT("prog"), Prog);
	TSharedPtr<FJsonObject> F = MakeShared<FJsonObject>(); for (const FString& K : Flags) F->SetBoolField(K, true);
	Out.SetObjectField(TEXT("flags"), F);
	TArray<TSharedPtr<FJsonValue>> A;
	for (const TPair<int32, EWheelTone>& X : Choices)
	{ TSharedPtr<FJsonObject> J = MakeShared<FJsonObject>(); J->SetNumberField(TEXT("ch"), X.Key); J->SetStringField(TEXT("tone"), UCompanionsFeature::ToneName(X.Value)); A.Add(MakeShared<FJsonValueObject>(J)); }
	Out.SetArrayField(TEXT("choices"), A);
	if (!Title.IsEmpty()) Out.SetStringField(TEXT("title"), Title);
}

void USagaFeature::LoadState(const FJsonObject& In)
{
	const FName C(*JStr(In, TEXT("calling"))); Calling = SagaData::FindCalling(C) ? C : NAME_None;
	Chapter = FMath::Clamp((int32)JInt(In, TEXT("ch")), 0, NumChapters()); bActive = JBool(In, TEXT("active")); Prog = FMath::Max(0, (int32)JInt(In, TEXT("prog")));
	if (TSharedPtr<FJsonObject> F = JObj(In, TEXT("flags"))) for (const auto& P : F->Values) if (P.Value.IsValid() && P.Value->Type == EJson::Boolean && P.Value->AsBool()) Flags.Add(FString(*P.Key));
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (In.TryGetArrayField(TEXT("choices"), A) && A)
		for (const TSharedPtr<FJsonValue>& V : *A)
		{
			const TSharedPtr<FJsonObject> J = V.IsValid() ? V->AsObject() : nullptr; if (!J.IsValid() || Choices.Num() >= 12) continue;
			const FString T = JStr(*J, TEXT("tone"));
			const EWheelTone Tone = T == TEXT("firm") ? EWheelTone::Firm : T == TEXT("cunning") ? EWheelTone::Cunning : EWheelTone::Kind;
			Choices.Add(TPair<int32, EWheelTone>((int32)JInt(*J, TEXT("ch")), Tone));
		}
	Title = JStr(In, TEXT("title")).Left(40);
}
