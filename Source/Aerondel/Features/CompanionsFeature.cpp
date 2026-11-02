#include "Features/CompanionsFeature.h"
#include "Core/AerondelEvents.h"
#include "Data/MobDef.h"
#include "Dom/JsonValue.h"
#include "Core/AerondelPlayerState.h"
#include "Combat/HealthComponent.h"
#include "Features/PartyFollower.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

const TArray<FCompanionDef>& UCompanionsFeature::Defs()
{
	static TArray<FCompanionDef> D;
	if (D.Num()) return D;
	auto Add = [](FName Id, const TCHAR* Name, const TCHAR* Role, TArray<FString> Skills, EWheelTone Likes, TArray<FString> Gifts, const TCHAR* Style, FVector2D Pos) -> FCompanionDef&
	{
		FCompanionDef& C = D.AddDefaulted_GetRef();
		C.Id = Id; C.Name = Name; C.Role = Role; C.Skills = MoveTemp(Skills); C.Likes = Likes; C.GiftWords = MoveTemp(Gifts); C.Style = Style; C.WebPos = Pos;
		return C;
	};
	FCompanionDef& T = Add(TEXT("tamsin"), TEXT("Tamsin the Squire"), TEXT("Squire"), { TEXT("Attack"), TEXT("Defence") }, EWheelTone::Kind, { TEXT("Cooked "), TEXT("Bread"), TEXT("Cake"), TEXT("Pie"), TEXT("Stew") }, TEXT("melee"), FVector2D(7, -11));
	T.RecruitMob = TEXT("Goblin"); T.RecruitCount = 5; T.Ask = TEXT("Goblins keep raiding the farms east of here. Help me drive off five of them and I'll ride with you.");
	FCompanionDef& Y = Add(TEXT("ysolde"), TEXT("Ysolde the Ranger"), TEXT("Ranger"), { TEXT("Ranged"), TEXT("Agility") }, EWheelTone::Cunning, { TEXT("arrows"), TEXT("Feather"), TEXT("hide"), TEXT("leather") }, TEXT("ranged"), FVector2D(44, -30));
	Y.RecruitItem = TEXT("Bronze arrows"); Y.RecruitCount = 25; Y.ReqSkill = TEXT("Ranged"); Y.ReqLevel = 10; Y.Ask = TEXT("My quiver is empty. Bring me 25 bronze arrows and show me you can shoot, and I'll watch your back.");
	FCompanionDef& B = Add(TEXT("brannoc"), TEXT("Brannoc Ironbeard"), TEXT("Dwarf smith"), { TEXT("Smithing"), TEXT("Mining") }, EWheelTone::Firm, { TEXT(" bar"), TEXT(" ore"), TEXT("Coal") }, TEXT("melee"), FVector2D(-54, -70));
	B.RecruitItem = TEXT("Iron bar"); B.RecruitCount = 3; B.ReqSkill = TEXT("Smithing"); B.ReqLevel = 15; B.Ask = TEXT("You want a dwarf at your side? Prove you know the forge. Three iron bars, smelted by your own hand.");
	FCompanionDef& M = Add(TEXT("mirelle"), TEXT("Mirelle the Hedge-mage"), TEXT("Hedge-mage"), { TEXT("Magic"), TEXT("Herbalism") }, EWheelTone::Cunning, { TEXT(" rune"), TEXT("Grimy "), TEXT("leaf"), TEXT("Marshmint"), TEXT("Tallowroot") }, TEXT("magic"), FVector2D(-74, 54));
	M.RecruitItem = TEXT("Air rune"); M.RecruitCount = 30; M.ReqSkill = TEXT("Magic"); M.ReqLevel = 10; M.Ask = TEXT("The sea air ate my rune pouch. Thirty air runes and a little magic of your own, and I'll travel with you.");
	return D;
}

const FCompanionDef* UCompanionsFeature::Def(FName Id) { return Defs().FindByPredicate([Id](const FCompanionDef& C) { return C.Id == Id; }); }

void UCompanionsFeature::Initialize(FSubsystemCollectionBase& Collection)
{
	UAerondelEvents* Ev = Collection.InitializeDependency<UAerondelEvents>();
	Super::Initialize(Collection);
	if (Ev) Ev->OnMobKilled.AddDynamic(this, &UCompanionsFeature::HandleMobKilled);
}

void UCompanionsFeature::ResetState()
{
	States.Reset(); Active = NAME_None; Party.Reset(); Alignment[0] = Alignment[1] = Alignment[2] = 0;
	for (const FCompanionDef& D : Defs()) States.Add(D.Id, FCompanionState());
}

void UCompanionsFeature::SaveState(FJsonObject& Out) const
{
	for (const TPair<FName, FCompanionState>& P : States)
	{
		TSharedPtr<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetBoolField(TEXT("recruited"), P.Value.bRecruited); J->SetBoolField(TEXT("started"), P.Value.bStarted);
		J->SetNumberField(TEXT("bond"), P.Value.Bond); J->SetNumberField(TEXT("giftAt"), (double)P.Value.GiftAt);
		J->SetNumberField(TEXT("task"), P.Value.Task);
		PutIntMap(*J, TEXT("crew"), P.Value.Crew);
		Out.SetObjectField(P.Key.ToString(), J);
	}
	Out.SetStringField(TEXT("_active"), Active.ToString());
	{ TArray<FString> P; for (FName N : Party) P.Add(N.ToString()); PutStrArr(Out, TEXT("_party"), P); }
	TSharedPtr<FJsonObject> A = MakeShared<FJsonObject>();
	A->SetNumberField(TEXT("kind"), Alignment[0]); A->SetNumberField(TEXT("firm"), Alignment[1]); A->SetNumberField(TEXT("cunning"), Alignment[2]);
	Out.SetObjectField(TEXT("_alignment"), A);
}

void UCompanionsFeature::LoadState(const FJsonObject& In)
{
	for (TPair<FName, FCompanionState>& P : States)
		if (TSharedPtr<FJsonObject> J = JObj(In, *P.Key.ToString()))
		{
			P.Value.bRecruited = JBool(*J, TEXT("recruited")); P.Value.bStarted = JBool(*J, TEXT("started"));
			P.Value.Bond = FMath::Clamp((int32)JInt(*J, TEXT("bond")), 0, BondMax);
			P.Value.GiftAt = JInt(*J, TEXT("giftAt")); P.Value.Task = FMath::Max(0, (int32)JInt(*J, TEXT("task")));
			for (const TPair<FString, int32>& C : JIntMap(*J, TEXT("crew"))) if (C.Value > 0 && C.Key.Len() < 20) P.Value.Crew.Add(C.Key, FMath::Min(C.Value, 1000000));
		}
	const FName A(*JStr(In, TEXT("_active")));
	const FCompanionState* S = States.Find(A);
	Active = S && S->bRecruited ? A : NAME_None;
	Party.Reset();
	for (const FString& P : JStrArr(In, TEXT("_party"))) { const FName N(*P); const FCompanionState* PS = States.Find(N); if (PS && PS->bRecruited && N != Active) Party.AddUnique(N); }
	if (TSharedPtr<FJsonObject> Al = JObj(In, TEXT("_alignment")))
	{
		Alignment[0] = FMath::Max(0, (int32)JInt(*Al, TEXT("kind"))); Alignment[1] = FMath::Max(0, (int32)JInt(*Al, TEXT("firm"))); Alignment[2] = FMath::Max(0, (int32)JInt(*Al, TEXT("cunning")));
	}
}

float UCompanionsFeature::XpBonus(FName SkillId) const
{
	// Party XP bonus: each member's bond bonus, summed, capped at 25% (web partyXpBonus).
	float T = 0.f; for (FName M : GetPartyIds()) T += OneXpBonus(M, SkillId);
	return FMath::Min(0.25f, T);
}

TArray<FName> UCompanionsFeature::GetPartyIds() const
{
	TArray<FName> O; if (!Active.IsNone()) O.Add(Active);
	for (FName N : Party) { const FCompanionState* S = States.Find(N); if (S && S->bRecruited) O.AddUnique(N); }
	return O;
}

void UCompanionsFeature::TogglePartyMember(FName CompId)
{
	const FCompanionState* S = States.Find(CompId); if (!S || !S->bRecruited) return;
	if (InParty(CompId))
	{
		if (Active == CompId) { Active = Party.Num() ? Party[0] : NAME_None; if (Party.Num()) Party.RemoveAt(0); }
		else Party.Remove(CompId);
	}
	else if (Active.IsNone()) Active = CompId;
	else Party.AddUnique(CompId);
}

FString UCompanionsFeature::CallWholeParty()
{
	TArray<FString> Names;
	for (const FCompanionDef& D : Defs()) { const FCompanionState* S = States.Find(D.Id); if (S && S->bRecruited) { if (!InParty(D.Id)) TogglePartyMember(D.Id); Names.Add(D.Name); } }
	if (!Names.Num()) return TEXT("Recruit a Sworn Companion first.");
	return TEXT("Your party gathers: ") + FString::Join(Names, TEXT(", ")) + TEXT(".");
}

float UCompanionsFeature::PartyGuard() const
{
	float R = 0.f;
	for (FName M : GetPartyIds()) { const FCompanionDef* D = Def(M); const FCompanionState* S = States.Find(M); if (D && S && D->Style == TEXT("melee")) R += 0.05f + (S->Bond >= 6 ? 0.03f : 0.f); }
	return FMath::Min(0.2f, R);
}

float UCompanionsFeature::DamageTakenMult(const UMobDef* Source, float HpFrac) const { return 1.f - PartyGuard(); }

FVector2D UCompanionsFeature::FormationSlot(int32 Slot)
{
	static const FVector2D Slots[] = { FVector2D(-1.5f, -1.5f), FVector2D(1.5f, -1.5f), FVector2D(-2.6f, -0.2f), FVector2D(2.6f, -0.2f), FVector2D(0.f, -2.8f) };
	return Slots[FMath::Abs(Slot) % 5];
}

void UCompanionsFeature::OnGameTick()
{
	++PartyTick;
	SyncFollowers();
	// Hedge-mage (magic style) mends you between blows below 60% HP, every 10 ticks.
	AAerondelPlayerState* PS = GetPS(); if (!PS || !PS->Health || PS->Health->IsDead() || PartyTick % 10 != 0) return;
	if (PS->Health->GetCurrent() >= PS->Health->GetMax() * 0.6f) return;
	for (FName M : GetPartyIds())
	{
		const FCompanionDef* D = Def(M); const FCompanionState* S = States.Find(M);
		if (!D || !S || D->Style != TEXT("magic")) continue;
		const int32 H = 2 + (S->Bond / 2) * 2; PS->Health->Heal(H);
		if (PartyTick % 50 == 0) Say(FString::Printf(TEXT("%s mends your wounds (+%d HP)."), *D->Name, H));
	}
}

void UCompanionsFeature::SyncFollowers()
{
	UGameInstance* GI = GetGameInstance(); UWorld* W = GI ? GI->GetWorld() : nullptr;
	APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const TArray<FName> Ids = GetPartyIds();
	TArray<FName> Gone; for (const auto& F : Followers) if (!Ids.Contains(F.Key) || !IsValid(F.Value)) Gone.Add(F.Key);
	for (FName G : Gone) { if (AActor* A = Followers.FindRef(G)) if (IsValid(A)) A->Destroy(); Followers.Remove(G); }
	if (!W || !Pawn) return;
	for (int32 i = 0; i < Ids.Num(); ++i)
	{
		APartyFollower* F = Cast<APartyFollower>(Followers.FindRef(Ids[i]));
		if (!F)
		{
			FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			F = W->SpawnActor<APartyFollower>(APartyFollower::StaticClass(), Pawn->GetActorLocation(), FRotator::ZeroRotator, P);
			if (!F) continue;
			const FCompanionDef* D = Def(Ids[i]); F->Setup(Ids[i], D ? D->Name : Ids[i].ToString(), D ? D->Style : FString());
			Followers.Add(Ids[i], F);
		}
		F->Leader = Pawn; F->Slot = UCompanionsFeature::FormationSlot(i) * 100.f; // web metres -> cm
	}
}

void UCompanionsFeature::Deinitialize()
{
	for (const auto& F : Followers) if (IsValid(F.Value)) F.Value->Destroy();
	Followers.Reset();
	Super::Deinitialize();
}

float UCompanionsFeature::OneXpBonus(FName CompId, FName SkillId) const
{
	const FCompanionDef* D = Def(CompId); const FCompanionState* S = States.Find(CompId);
	if (!D || !S || !S->bRecruited) return 0.f;
	bool bHelps = false; for (const FString& K : D->Skills) if (Id(K) == SkillId) bHelps = true;
	return !bHelps ? 0.f : S->Bond >= 6 ? 0.10f : S->Bond >= 3 ? 0.05f : 0.f;
}

void UCompanionsFeature::StartRecruit(FName CompId)
{
	FCompanionState* S = States.Find(CompId); const FCompanionDef* D = Def(CompId);
	if (!S || !D || S->bRecruited) return;
	S->bStarted = true;
	Say(D->Name + TEXT(": ") + D->Ask);
}

bool UCompanionsFeature::TryRecruit(FName CompId)
{
	FCompanionState* S = States.Find(CompId); const FCompanionDef* D = Def(CompId);
	if (!S || !D || S->bRecruited || !S->bStarted) return false;
	if (!D->RecruitMob.IsEmpty()) { if (S->Task < D->RecruitCount) return false; }
	else
	{
		if (!D->ReqSkill.IsEmpty() && SkillLevel(D->ReqSkill) < D->ReqLevel) { Say(FString::Printf(TEXT("You need %s %d first."), *D->ReqSkill, D->ReqLevel)); return false; }
		if (!TakeItem(D->RecruitItem, D->RecruitCount)) return false;
	}
	S->bRecruited = true; S->Bond = FMath::Max(S->Bond, 1);
	Say(D->Name + TEXT(" swears to ride with you. Open Bonds (B) to have them follow you."));
	return true;
}

bool UCompanionsFeature::SetActive(FName CompId)
{
	const FCompanionState* S = States.Find(CompId);
	if (!S || !S->bRecruited) return false;
	Active = CompId; return true;
}

bool UCompanionsFeature::Gift(FName CompId, const FString& ItemName)
{
	FCompanionState* S = States.Find(CompId); const FCompanionDef* D = Def(CompId);
	if (!S || !D || !S->bRecruited) return false;
	const int64 Now = NowMs();
	if (Now - S->GiftAt < GiftCooldownMs) { Say(D->Name + TEXT(" has had a gift recently.")); return false; }
	if (!TakeItem(ItemName, 1)) return false;
	bool bLiked = false; for (const FString& W : D->GiftWords) if (ItemName.Contains(W, ESearchCase::IgnoreCase)) bLiked = true;
	S->GiftAt = Now;
	AddBond(CompId, bLiked ? 2 : 1);
	Say(FString::Printf(TEXT("%s %s the gift. (Bond %d/%d)"), *D->Name, bLiked ? TEXT("loves") : TEXT("accepts"), S->Bond, BondMax));
	return true;
}

int32 UCompanionsFeature::AddBond(FName CompId, int32 N)
{
	FCompanionState* S = States.Find(CompId);
	if (!S || !S->bRecruited) return 0;
	S->Bond = FMath::Clamp(S->Bond + N, 0, BondMax);
	return S->Bond;
}

void UCompanionsFeature::RecordTone(EWheelTone Tone)
{
	Alignment[(int32)Tone]++;
	const FCompanionDef* D = Def(Active); FCompanionState* S = States.Find(Active);
	if (!D || !S || !S->bRecruited) return;
	const int64 Now = NowMs();
	if (Tone == D->Likes && Now - S->ToneAt > 60000) { S->ToneAt = Now; AddBond(Active, 1); Say(FString::Printf(TEXT("%s approves. (Bond %d/%d)"), *D->Name, S->Bond, BondMax)); }
}

int32 UCompanionsFeature::ActiveBond() const { const FCompanionState* S = States.Find(Active); return S && S->bRecruited ? S->Bond : 0; }
int32 UCompanionsFeature::AssistMax() const { const int32 B = ActiveBond(); return 2 + CombatLevel() / 12 + (B >= 6 ? 3 : 0); }
int32 UCompanionsFeature::AssistEvery() const { return ActiveBond() >= 10 ? 3 : 6; }

void UCompanionsFeature::HandleMobKilled(UMobDef* Mob)
{
	if (!Mob) return;
	for (const FCompanionDef& D : Defs())
		if (!D.RecruitMob.IsEmpty() && Mob->Id == Id(D.RecruitMob))
			if (FCompanionState* S = States.Find(D.Id)) if (S->bStarted && !S->bRecruited && S->Task < D.RecruitCount)
			{
				S->Task++;
				if (S->Task >= D.RecruitCount) Say(D.Name + TEXT(": That's enough of them. Come and talk to me."));
			}
}
