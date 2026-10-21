#include "Skills/SkillsComponent.h"
#include "Data/SkillDef.h"
#include "Data/GameDatabase.h"
#include "Core/GielinorEvents.h"

USkillsComponent::USkillsComponent() { bWantsInitializeComponent = true; }

void USkillsComponent::InitializeComponent()
{
	Super::InitializeComponent();
	ResetToDatabase();
}

void USkillsComponent::ResetToDatabase()
{
	Entries.Reset();
	if (const UGameDatabase* Db = UGameDatabase::Get())
	{
		for (USkillDef* S : Db->Skills)
		{
			if (!S) continue;
			FSkillProgress P; P.Skill = S; P.Level = S->StartLevel; P.Xp = UXpCurve::XpForLevel(S->StartLevel);
			Entries.Add(P);
		}
	}
}

FSkillProgress* USkillsComponent::Find(const USkillDef* Skill)
{
	return Entries.FindByPredicate([Skill](const FSkillProgress& P) { return P.Skill == Skill; });
}
const FSkillProgress* USkillsComponent::Find(const USkillDef* Skill) const
{
	return Entries.FindByPredicate([Skill](const FSkillProgress& P) { return P.Skill == Skill; });
}

void USkillsComponent::AddXp(USkillDef* Skill, float Amount)
{
	if (!Skill || Amount <= 0.f) return;
	FSkillProgress* P = Find(Skill);
	if (!P) { FSkillProgress N; N.Skill = Skill; N.Level = Skill->StartLevel; N.Xp = UXpCurve::XpForLevel(Skill->StartLevel); P = &Entries.Add_GetRef(N); }
	P->Xp = FMath::Min(P->Xp + Amount, 200000000.f);
	const int32 Old = P->Level;
	P->Level = UXpCurve::LevelForXp(P->Xp, Skill->MaxLevel);
	if (UGielinorEvents* Ev = UGielinorEvents::Get(this))
	{
		Ev->OnXpGained.Broadcast(Skill, Amount, P->Xp, P->Level);
		if (P->Level > Old) Ev->OnLevelUp.Broadcast(Skill, P->Level);
	}
}

int32 USkillsComponent::GetLevel(const USkillDef* Skill) const { const FSkillProgress* P = Find(Skill); return P ? P->Level : 1; }
float USkillsComponent::GetXp(const USkillDef* Skill) const { const FSkillProgress* P = Find(Skill); return P ? P->Xp : 0.f; }

int32 USkillsComponent::GetLevelById(FName SkillId) const
{
	for (const FSkillProgress& P : Entries) if (P.Skill && P.Skill->Id == SkillId) return P.Level;
	return SkillId == TEXT("hitpoints") ? 10 : 1;
}

int32 USkillsComponent::TotalLevel() const
{
	int32 T = 0; for (const FSkillProgress& P : Entries) T += P.Level; return T;
}

int32 USkillsComponent::CombatLevel() const
{
	// Classic melee-only combat level formula.
	const float Base = 0.25f * (GetLevelById(TEXT("defence")) + GetLevelById(TEXT("hitpoints")));
	const float Melee = 0.325f * (GetLevelById(TEXT("attack")) + GetLevelById(TEXT("strength")));
	return FMath::FloorToInt(Base + Melee);
}

void USkillsComponent::SetXpRaw(FName SkillId, float Xp)
{
	for (FSkillProgress& P : Entries)
	{
		if (P.Skill && P.Skill->Id == SkillId) { P.Xp = Xp; P.Level = UXpCurve::LevelForXp(Xp, P.Skill->MaxLevel); return; }
	}
}
