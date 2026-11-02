#include "Features/SteedsFeature.h"
#include "Core/AerondelEvents.h"
#include "Data/SkillDef.h"

namespace SteedData
{
	struct FSteed { const TCHAR* Id; const TCHAR* Name; int32 Agility; float Speed; bool bShift; };
	static const FSteed All[] = { { TEXT("stag"), TEXT("Fernback Stag"), 10, 1.45f, false }, { TEXT("boar"), TEXT("Ironbristle Boar"), 30, 1.3f, true } };
	static const FSteed* Find(FName Id) { for (const FSteed& S : All) if (Id == FName(S.Id)) return &S; return nullptr; }
}

void USteedsFeature::Initialize(FSubsystemCollectionBase& Collection)
{
	UAerondelEvents* Ev = Collection.InitializeDependency<UAerondelEvents>();
	Super::Initialize(Collection);
	if (Ev) Ev->OnLevelUp.AddDynamic(this, &USteedsFeature::HandleLevelUp);
}

void USteedsFeature::HandleLevelUp(USkillDef* Skill, int32 NewLevel) { if (Skill && Skill->Id == Id(TEXT("Agility"))) GrantForAgility(NewLevel); }

int32 USteedsFeature::GrantForAgility(int32 AgilityLevel)
{
	int32 N = 0;
	for (const SteedData::FSteed& S : SteedData::All)
		if (AgilityLevel >= S.Agility && !Owned.Contains(FName(S.Id))) { Owned.Add(FName(S.Id)); ++N; Say(FString::Printf(TEXT("A %s answers your call. Summon it from the Stable Codex."), S.Name)); }
	return N;
}

bool USteedsFeature::Summon(FName Steed)
{
	if (!Owned.Contains(Steed)) return false;
	Active = Steed; bMounted = true; return true;
}

float USteedsFeature::SpeedMultiplier(bool bOnRoad) const
{
	const SteedData::FSteed* S = bMounted ? SteedData::Find(Active) : nullptr;
	return S ? S->Speed * (bOnRoad ? RoadBonus : 1.f) : 1.f;
}

bool USteedsFeature::ShiftBulwark()
{
	const SteedData::FSteed* S = bMounted ? SteedData::Find(Active) : nullptr;
	const int64 Now = NowMs();
	if (!S || !S->bShift || Now < BulwarkReadyAt) return false;
	BulwarkUntil = Now + BulwarkMs; BulwarkReadyAt = Now + BulwarkCdMs;
	Say(TEXT("Your boar shifts into Bulwark form and charges!"));
	return true;
}

void USteedsFeature::SaveState(FJsonObject& Out) const
{
	TArray<FString> O; for (const FName& N : Owned) O.Add(N.ToString());
	PutStrArr(Out, TEXT("owned"), O);
	Out.SetStringField(TEXT("active"), Active.IsNone() ? FString() : Active.ToString());
}


void USteedsFeature::LoadState(const FJsonObject& In)
{
	for (const FString& S : JStrArr(In, TEXT("owned"))) if (SteedData::Find(FName(*S))) Owned.AddUnique(FName(*S));
	const FName A(*JStr(In, TEXT("active")));
	Active = Owned.Contains(A) ? A : NAME_None;
}
