#include "Features/PathsFeature.h"
#include "Core/AerondelEvents.h"
#include "Core/AerondelPlayerState.h"
#include "Combat/HealthComponent.h"
#include "Data/MobDef.h"
#include "Dom/JsonValue.h"

namespace PathData
{
	struct FT { const TCHAR* Name; float Red, Dmg, Acc, Regen, KillHeal; };
	static const FT Rows[7][3] = {
		{ { TEXT("Thick Hide"), .04f, 0, 0, 0, 0 }, { TEXT("Keen Edge"), 0, .04f, 0, 0, 0 }, { TEXT("Steady Hand"), 0, 0, .05f, 0, 0 } },
		{ { TEXT("Second Breath"), 0, 0, 0, .5f, 0 }, { TEXT("Butcher"), 0, 0, 0, 0, .04f }, { TEXT("Iron Will"), .04f, 0, 0, 0, 0 } },
		{ { TEXT("Precision"), 0, 0, .06f, 0, 0 }, { TEXT("Brutality"), 0, .05f, 0, 0, 0 }, { TEXT("Bulwark Stance"), .05f, 0, 0, 0, 0 } },
		{ { TEXT("Field Medic"), 0, 0, 0, .8f, 0 }, { TEXT("Vampiric"), 0, 0, 0, 0, .06f }, { TEXT("Sharpshooter"), 0, 0, .07f, 0, 0 } },
		{ { TEXT("Juggernaut"), .06f, 0, 0, 0, 0 }, { TEXT("Executioner"), 0, .06f, 0, 0, 0 }, { TEXT("Lifeblood"), 0, 0, 0, 1.f, 0 } },
		{ { TEXT("Falcon Sight"), 0, 0, .08f, 0, 0 }, { TEXT("Unbroken"), .06f, 0, 0, 0, 0 }, { TEXT("Reaver"), 0, .07f, 0, 0, .03f } },
		{ { TEXT("Avatar of War"), 0, .1f, 0, 0, 0 }, { TEXT("Living Fortress"), .1f, 0, 0, 0, 0 }, { TEXT("Font of Life"), 0, 0, 0, 1.5f, .05f } } };
	static bool Valid(FName P) { return P == FName(TEXT("bulwark")) || P == FName(TEXT("mender")) || P == FName(TEXT("striker")); }
}

void UPathsFeature::Initialize(FSubsystemCollectionBase& Collection)
{
	UAerondelEvents* Ev = Collection.InitializeDependency<UAerondelEvents>();
	Super::Initialize(Collection);
	if (Ev) Ev->OnMobKilled.AddDynamic(this, &UPathsFeature::HandleMobKilled);
}

int32 UPathsFeature::RowsOpen() const { const int32 C = CombatLevel(); return C < PathLevel ? 0 : FMath::Clamp((C - PathLevel) / 10 + 1, 0, NumRows); }

FString UPathsFeature::Choose(FName NewPath)
{
	if (!PathData::Valid(NewPath)) return TEXT("Unknown path.");
	if (CombatLevel() < PathLevel) return TEXT("Paths open at combat level 30.");
	if (PathId == NewPath) return TEXT("You already walk that path.");
	if (!PathId.IsNone()) ++Switches;
	PathId = NewPath; Talents.Init(-1, NumRows);
	return FString::Printf(TEXT("You take up the %s path."), *NewPath.ToString());
}

FString UPathsFeature::PickTalent(int32 Row, int32 Choice)
{
	if (PathId.IsNone()) return TEXT("Choose a path first.");
	if (Row < 0 || Row >= RowsOpen() || Choice < 0 || Choice > 2) return TEXT("That talent row isn't open yet.");
	if (Talents.Num() != NumRows) Talents.Init(-1, NumRows);
	Talents[Row] = Choice;
	return FString::Printf(TEXT("Talent: %s."), PathData::Rows[Row][Choice].Name);
}

FString UPathsFeature::TalentName(int32 Row, int32 Choice) { return Row >= 0 && Row < NumRows && Choice >= 0 && Choice < 3 ? FString(PathData::Rows[Row][Choice].Name) : FString(); }

FPathFx UPathsFeature::Fx() const
{
	FPathFx O;
	if (PathId.IsNone()) return O;
	if (PathId == FName(TEXT("bulwark"))) O.Red += 0.10f;
	else if (PathId == FName(TEXT("mender"))) O.Regen += 1.f;
	else O.Dmg += 0.08f;
	for (int32 i = 0; i < Talents.Num() && i < NumRows; ++i)
		if (Talents[i] >= 0 && Talents[i] < 3)
		{ const PathData::FT& T = PathData::Rows[i][Talents[i]]; O.Red += T.Red; O.Dmg += T.Dmg; O.Acc += T.Acc; O.Regen += T.Regen; O.KillHeal += T.KillHeal; }
	O.Red = FMath::Min(0.45f, O.Red);
	return O;
}

TArray<FName> UPathsFeature::PathSkills() const
{
	TArray<FName> S;
	if (PathId == FName(TEXT("bulwark"))) { S.Add(TEXT("challenge")); S.Add(TEXT("wall")); }
	else if (PathId == FName(TEXT("mender"))) { S.Add(TEXT("mend")); S.Add(TEXT("ward")); }
	else if (PathId == FName(TEXT("striker"))) { S.Add(TEXT("rend")); S.Add(TEXT("flurry")); }
	return S;
}

void UPathsFeature::OnGameTick()
{
	const float R = Fx().Regen; if (R <= 0.f) return;
	AAerondelPlayerState* PS = GetPS(); if (!PS || !PS->Health || PS->Health->IsDead()) return;
	if (PS->Health->GetCurrent() >= PS->Health->GetMax()) return;
	RegenAcc += R / 6.f;
	if (RegenAcc >= 1.f) { const int32 N = FMath::FloorToInt(RegenAcc); RegenAcc -= N; PS->Health->Heal(N); }
}

void UPathsFeature::HandleMobKilled(UMobDef* Mob)
{
	const float K = Fx().KillHeal; AAerondelPlayerState* PS = GetPS();
	if (K > 0.f && PS && PS->Health && !PS->Health->IsDead()) PS->Health->Heal(FMath::Max(1, FMath::RoundToInt(PS->Health->GetMax() * K)));
}

void UPathsFeature::SaveState(FJsonObject& Out) const
{
	if (!PathId.IsNone()) Out.SetStringField(TEXT("id"), PathId.ToString());
	TArray<TSharedPtr<FJsonValue>> A;
	for (int32 V : Talents) A.Add(V >= 0 ? StaticCastSharedRef<FJsonValue>(MakeShared<FJsonValueNumber>(V)) : StaticCastSharedRef<FJsonValue>(MakeShared<FJsonValueNull>()));
	Out.SetArrayField(TEXT("talents"), A); Out.SetNumberField(TEXT("switches"), Switches);
}

void UPathsFeature::LoadState(const FJsonObject& In)
{
	const FName P(*JStr(In, TEXT("id"))); PathId = PathData::Valid(P) ? P : NAME_None;
	Talents.Init(-1, NumRows);
	const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
	if (In.TryGetArrayField(TEXT("talents"), A) && A)
		for (int32 i = 0; i < A->Num() && i < NumRows; ++i)
		{ double V = -1; if ((*A)[i].IsValid() && (*A)[i]->TryGetNumber(V) && V >= 0 && V <= 2) Talents[i] = (int32)V; }
	Switches = FMath::Max(0, (int32)JInt(In, TEXT("switches")));
}
