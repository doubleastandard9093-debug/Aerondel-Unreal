#include "Rules/GielinorRules.h"
#include "Core/GielinorPlayerState.h"
#include "Core/PlayerStatsComponent.h"
#include "Skills/SkillsComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/EquipmentComponent.h"
#include "Quests/QuestComponent.h"
#include "Data/GameDatabase.h"
#include "Data/ContentDefs.h"
#include "Data/ItemDef.h"
#include "Data/MobDef.h"
#include "Data/SkillDef.h"
#include "Data/QuestDef.h"
#include "Data/ResourceNodeDef.h"

#define LOCTEXT_NAMESPACE "GielinorRules"

namespace
{
	const UGameDatabase* Db() { return UGameDatabase::Get(); }
	int32 Lvl(const AGielinorPlayerState* PS, FName Skill) { return PS && PS->Skills ? PS->Skills->GetLevelById(Skill) : 1; }
	UItemDef* Worn(const AGielinorPlayerState* PS, EEquipSlot S) { return PS && PS->Equipment ? PS->Equipment->GetEquipped(S) : nullptr; }
	float Rand() { return FMath::FRand(); }
	bool MeetsReq(const AGielinorPlayerState* PS, const UItemDef* I)
	{
		for (const TPair<FName, int32>& R : I->SkillRequirements) if (Lvl(PS, R.Key) < R.Value) return false;
		return true;
	}
}

// ---------------------------------------------------------------- bonus sources
float UGielinorRules::Perk(const AGielinorPlayerState* PS, FName SkillId, FName Key)
{
	const UGameDatabase* D = Db();
	const USkillDef* S = D ? D->FindSkill(SkillId) : nullptr;
	if (!S) return 0.f;
	const int32 L = Lvl(PS, SkillId);
	float V = 0.f;
	for (const FSkillPerk& P : S->Perks)
	{
		if (P.Level > L) break;
		if (const float* F = P.Effects.Find(Key)) V += *F;
	}
	if (const UItemDef* Cape = Worn(PS, EEquipSlot::Cape)) if (Cape->CapeSkill == SkillId) V *= 1.1f;
	const float Cap = D->GetPerkCap(Key);
	return Cap >= 0.f ? FMath::Min(Cap, V) : V;
}

float UGielinorRules::QuestFx(const AGielinorPlayerState* PS, FName Key)
{
	if (!PS || !PS->Quests) return 0.f;
	float V = 0.f;
	for (const FQuestProgress& P : PS->Quests->GetAll())
		if (P.Quest && P.State == EQuestState::Completed) if (const float* F = P.Quest->PermanentBonus.Find(Key)) V += *F;
	if (const UGameDatabase* D = Db())
	{
		const int32 QP = PS->Quests->QuestPoints();
		for (const FQuestPointMilestone& M : D->QuestPointMilestones)
			if (QP >= M.Points) if (const float* F = M.Bonus.Find(Key)) V += *F;
	}
	return V;
}

float UGielinorRules::GearFx(const AGielinorPlayerState* PS, FName Key)
{
	if (!PS || !PS->Equipment) return 0.f;
	float V = 0.f;
	for (const auto& P : PS->Equipment->GetAll()) if (P.Value) V += P.Value->GetEffect(Key);
	return V;
}

bool UGielinorRules::SetStatus(const AGielinorPlayerState* PS, const FArmourSet*& OutSet, int32& OutHave, int32& OutNeed)
{
	OutSet = nullptr; OutHave = 0; OutNeed = 1;
	const UGameDatabase* D = Db();
	if (!D || !PS) return false;
	for (const FArmourSet& S : D->ArmourSets)
	{
		int32 Have = 0;
		for (EEquipSlot Sl : S.Pieces) if (const UItemDef* I = Worn(PS, Sl)) if (I->SetName == S.Name) ++Have;
		if (!Have || S.Pieces.Num() == 0) continue;
		if (!OutSet || float(Have) / S.Pieces.Num() > float(OutHave) / OutNeed) { OutSet = &S; OutHave = Have; OutNeed = S.Pieces.Num(); }
	}
	return OutSet != nullptr;
}

float UGielinorRules::SetFx(const AGielinorPlayerState* PS, FName Key)
{
	const FArmourSet* S; int32 Have, Need;
	if (!SetStatus(PS, S, Have, Need) || Have < Need) return 0.f;
	const float* F = S->Effects.Find(Key);
	return F ? *F : 0.f;
}

float UGielinorRules::PrayerFx(const AGielinorPlayerState* PS, FName Key)
{
	const UGameDatabase* D = Db();
	if (!PS || !PS->Stats || !D) return 0.f;
	float V = 0.f;
	for (FName Id : PS->Stats->ActivePrayers) if (const UPrayerDef* P = D->FindPrayer(Id)) V += P->GetEffect(Key);
	return V;
}

float UGielinorRules::EffectFx(const AGielinorPlayerState* PS, FName Key) { return PS && PS->Stats ? PS->Stats->EffectFx(Key) : 0.f; }

FItemBonuses UGielinorRules::EquipmentBonuses(const AGielinorPlayerState* PS)
{
	FItemBonuses B;
	if (!PS || !PS->Equipment) return B;
	for (const auto& P : PS->Equipment->GetAll())
	{
		if (!P.Value) continue;
		B += P.Value->Bonuses;
		// Legacy starter-content fields.
		B.Attack += P.Value->AttackBonus; B.Strength += P.Value->StrengthBonus; B.Defence += P.Value->DefenceBonus;
	}
	return B;
}

int32 UGielinorRules::BoostedLevel(const AGielinorPlayerState* PS, FName SkillId)
{
	return Lvl(PS, SkillId) + (PS && PS->Stats ? PS->Stats->BoostOf(SkillId) : 0);
}

float UGielinorRules::XpMultiplier(const AGielinorPlayerState* PS, FName SkillId)
{
	const FName XpKey(*(TEXT("xp.") + SkillId.ToString()));
	return 1.f + QuestFx(PS, XpKey) + QuestFx(PS, TEXT("xp.*")) + GearFx(PS, XpKey) + EffectFx(PS, TEXT("xp"));
}

int32 UGielinorRules::MaxHitpoints(const AGielinorPlayerState* PS)
{
	return Lvl(PS, TEXT("hitpoints")) * 10 + FMath::RoundToInt(Perk(PS, TEXT("hitpoints"), TEXT("maxHp")) + QuestFx(PS, TEXT("maxHp")));
}

int32 UGielinorRules::MaxPrayerPoints(const AGielinorPlayerState* PS) { return Lvl(PS, TEXT("prayer")) * 10; }

// ---------------------------------------------------------------- toolbelt / gathering
UItemDef* UGielinorRules::BestTool(const AGielinorPlayerState* PS, EToolType Type)
{
	if (!PS || Type == EToolType::None) return nullptr;
	UItemDef* Best = nullptr; int32 BestTier = -1;
	auto Consider = [&](UItemDef* I)
	{
		if (!I || I->ToolType != Type || !MeetsReq(PS, I)) return;
		if (I->ToolTier > BestTier) { Best = I; BestTier = I->ToolTier; }
	};
	if (PS->Stats) for (UItemDef* I : PS->Stats->Toolbelt) Consider(I);
	// Tools in the backpack or wielded also count (VR players physically hold them).
	if (PS->Inventory) for (const FItemStack& S : PS->Inventory->GetSlots()) Consider(S.Item);
	if (PS->Equipment) for (const auto& P : PS->Equipment->GetAll()) Consider(P.Value);
	return Best;
}

bool UGielinorRules::OwnsToolType(const AGielinorPlayerState* PS, EToolType Type)
{
	if (!PS) return false;
	if (PS->Stats) for (const UItemDef* I : PS->Stats->Toolbelt) if (I && I->ToolType == Type) return true;
	for (const FItemStack& S : PS->Inventory->GetSlots()) if (S.Item && S.Item->ToolType == Type) return true;
	return PS->Equipment->EquippedTool(Type) != nullptr;
}

float UGielinorRules::SuccessChance(int32 Level, int32 Required) { return FMath::Clamp(0.3f + (Level - Required) * 0.025f, 0.05f, 0.92f); }

FGatherResult UGielinorRules::GatherAttempt(const AGielinorPlayerState* PS, const UResourceNodeDef* Node, float ExtraBonus)
{
	FGatherResult R;
	if (!PS || !Node || !Node->Skill) { R.Error = LOCTEXT("Cant", "You can't do that."); return R; }
	const FName Skill = Node->Skill->Id;
	const int32 L = Lvl(PS, Skill);
	int32 MinLevel = Node->LevelRequired;
	if (Node->Catches.Num() > 0) { MinLevel = Node->Catches.Last().Level; }
	if (L < MinLevel) { R.Error = FText::Format(LOCTEXT("Lvl", "You need a {0} level of {1} to do that."), Node->Skill->DisplayName, MinLevel); return R; }
	const UItemDef* Tool = nullptr;
	if (Node->ToolRequired != EToolType::None)
	{
		Tool = BestTool(PS, Node->ToolRequired);
		if (!Tool)
		{
			R.Error = OwnsToolType(PS, Node->ToolRequired) ? FText::Format(LOCTEXT("ToolLvl", "Your tool needs a higher {0} level."), Node->Skill->DisplayName)
			                                               : LOCTEXT("NoTool", "You need the right tool on your toolbelt. The tool shop sells one.");
			return R;
		}
	}
	const FName GatherKey(*(TEXT("gather.") + Skill.ToString()));
	const float Bonus = (Tool ? Tool->ToolTier : 0) * 0.05f + Perk(PS, Skill, TEXT("speed")) + PrayerFx(PS, TEXT("gather")) + GearFx(PS, GatherKey)
		+ QuestFx(PS, TEXT("gather")) + EffectFx(PS, TEXT("gather")) + ExtraBonus;
	const float XpMult = 1.f + PrayerFx(PS, TEXT("gather")) + Perk(PS, Skill, TEXT("xpBonus"));
	const float Dbl = Perk(PS, Skill, TEXT("double")), Bank = Perk(PS, Skill, TEXT("autoBank"));
	auto Win = [&](UItemDef* Item, float Xp) { R.Item = Item; R.Count = Rand() < Dbl ? 2 : 1; R.Xp = Xp * XpMult; R.bToBank = Rand() < Bank; };
	if (Node->Catches.Num() > 0)
	{
		for (int32 i = 0; i < Node->Catches.Num(); ++i)
		{
			const FResourceCatch& C = Node->Catches[i];
			if (L < C.Level || !C.Item) continue;
			if (!PS->Inventory->CanAdd(C.Item)) { R.Error = LOCTEXT("Full", "Your inventory is too full to hold any more."); return R; }
			const bool bLast = i == Node->Catches.Num() - 1;
			if (Rand() < FMath::Min(0.95f, SuccessChance(L, C.Level) + Bonus) * (bLast ? 1.f : 0.6f)) { Win(C.Item, C.Xp); return R; }
		}
		return R;
	}
	UItemDef* Item = (Node->PureLevel > 0 && Node->PureProduct && L >= Node->PureLevel) ? Node->PureProduct.Get() : Node->Product.Get();
	if (!Item) { R.Error = LOCTEXT("Cant", "You can't do that."); return R; }
	if (!PS->Inventory->CanAdd(Item)) { R.Error = LOCTEXT("Full", "Your inventory is too full to hold any more."); return R; }
	if (Rand() < FMath::Min(0.95f, SuccessChance(L, Node->LevelRequired) + Bonus)) Win(Item, Node->Xp);
	return R;
}

float UGielinorRules::BurnChance(const AGielinorPlayerState* PS, int32 BurnStop, int32 RecipeLevel)
{
	if (BurnStop <= 0) return 0.f;
	const int32 L = Lvl(PS, TEXT("cooking"));
	if (L >= BurnStop) return 0.f;
	const float Base = 0.45f * (BurnStop - L) / float(BurnStop - RecipeLevel + 1);
	const float Less = Perk(PS, TEXT("cooking"), TEXT("burnLess")) + QuestFx(PS, TEXT("burnLess"));
	return FMath::Max(0.f, Base * (1.f - FMath::Min(1.f, Less)));
}

// ---------------------------------------------------------------- combat
bool UGielinorRules::IsRanged(const AGielinorPlayerState* PS)
{
	const UItemDef* W = Worn(PS, EEquipSlot::Weapon);
	return W && W->bRangedWeapon;
}

const FCombatStyle* UGielinorRules::CurrentStyle(const AGielinorPlayerState* PS)
{
	const UGameDatabase* D = Db();
	if (!D) return nullptr;
	const TArray<FCombatStyle>& L = IsRanged(PS) ? D->RangedStyles : D->MeleeStyles;
	if (L.Num() == 0) return nullptr;
	const int32 Idx = PS && PS->Stats ? PS->Stats->AttackStyle : 0;
	return &L[FMath::Clamp(Idx, 0, L.Num() - 1)];
}

int32 UGielinorRules::AttackSpeedTicks(const AGielinorPlayerState* PS)
{
	const UItemDef* W = Worn(PS, EEquipSlot::Weapon);
	const FCombatStyle* S = CurrentStyle(PS);
	return FMath::Max(2, (W ? W->AttackSpeedTicks : 4) + (S ? S->SpeedTicks : 0));
}

float UGielinorRules::AttackRangeTiles(const AGielinorPlayerState* PS)
{
	if (IsRanged(PS)) { const FCombatStyle* S = CurrentStyle(PS); return 7.f + (S ? S->RangeTiles : 0.f); }
	return 1.6f;
}

float UGielinorRules::HitChance(float A, float D)
{
	return A > D ? 1.f - (D + 2.f) / (2.f * (A + 1.f)) : A / (2.f * (D + 1.f));
}

int32 UGielinorRules::PlayerMaxHit(const AGielinorPlayerState* PS, const FSwingMods& Mods)
{
	const FItemBonuses B = EquipmentBonuses(PS);
	const FCombatStyle* S = CurrentStyle(PS);
	const float StyleStr = S ? S->StrengthBonus : 0.f;
	float Max;
	if (IsRanged(PS))
	{
		const float L = BoostedLevel(PS, TEXT("ranged")) * (1.f + PrayerFx(PS, TEXT("rng")));
		Max = 8 + FMath::FloorToFloat((L + StyleStr) * 2.f) + B.RangedStrength * 3 + Perk(PS, TEXT("ranged"), TEXT("rmaxHit")) + QuestFx(PS, TEXT("rmaxHit"));
	}
	else
	{
		const float L = BoostedLevel(PS, TEXT("strength")) * (1.f + PrayerFx(PS, TEXT("str")));
		Max = 10 + FMath::FloorToFloat((L + StyleStr) * 2.f) + B.Strength * 3 + Perk(PS, TEXT("strength"), TEXT("maxHit"));
	}
	return FMath::FloorToInt(Max * (1.f + Mods.Damage));
}

float UGielinorRules::PlayerAccuracy(const AGielinorPlayerState* PS, const UMobDef* Mob, const FSwingMods& Mods)
{
	const FItemBonuses B = EquipmentBonuses(PS);
	const FCombatStyle* S = CurrentStyle(PS);
	const bool bR = IsRanged(PS);
	const float Base = bR ? BoostedLevel(PS, TEXT("ranged")) * (1.f + PrayerFx(PS, TEXT("rng"))) : BoostedLevel(PS, TEXT("attack")) * (1.f + PrayerFx(PS, TEXT("att")));
	const float L = Base + (S ? S->AttackBonus : 0) + 8.f;
	const float Att = L * FMath::Max(16.f, 64.f + (bR ? B.RangedAttack : B.Attack) * 3.f);
	const float Def = ((Mob ? Mob->DefenceLevel : 1) + 8.f) * 64.f;
	const float Bonus = (bR ? Perk(PS, TEXT("ranged"), TEXT("racc")) + SetFx(PS, TEXT("racc")) : Perk(PS, TEXT("attack"), TEXT("acc")) + SetFx(PS, TEXT("acc"))) + Mods.Accuracy;
	return FMath::Min(0.98f, HitChance(Att, Def) * (1.f + Bonus));
}

float UGielinorRules::DamageMult(const AGielinorPlayerState* PS, const UMobDef* Mob, int32 MobHp, int32 MobMaxHp)
{
	float M = 1.f + QuestFx(PS, TEXT("dmgVs.*")) + GearFx(PS, TEXT("dmgPct")) + SetFx(PS, TEXT("dmgPct"));
	if (Mob) M += QuestFx(PS, FName(*(TEXT("dmgVs.") + Mob->Id.ToString())));
	if (MobMaxHp > 0 && MobHp < MobMaxHp * 0.25f && !IsRanged(PS)) M += Perk(PS, TEXT("strength"), TEXT("execute"));
	return M;
}

int32 UGielinorRules::RollPlayerHit(const AGielinorPlayerState* PS, const UMobDef* Mob, int32 MobHp, int32 MobMaxHp, FSwingMods& Mods)
{
	int32 Dmg = 0;
	if (Rand() < PlayerAccuracy(PS, Mob, Mods))
	{
		Dmg = 1 + FMath::FloorToInt(Rand() * PlayerMaxHit(PS, Mods));
		const bool bR = IsRanged(PS);
		const float Crit = FMath::Min(0.75f, (bR ? Perk(PS, TEXT("ranged"), TEXT("rcrit")) : Perk(PS, TEXT("strength"), TEXT("crit"))) + GearFx(PS, TEXT("crit")));
		if (Rand() < Crit) { Dmg = FMath::FloorToInt(Dmg * (1.5f + Perk(PS, TEXT("strength"), TEXT("critMult")))); Mods.bCrit = true; }
		const float Big = GearFx(PS, TEXT("bigHit"));
		if (Big > 0.f && Rand() < Big) { Dmg = FMath::FloorToInt(Dmg * 1.5f); Mods.bBigHit = true; }
		Dmg = FMath::FloorToInt(Dmg * DamageMult(PS, Mob, MobHp, MobMaxHp));
	}
	return FMath::Clamp(Dmg, 0, FMath::Max(0, MobHp));
}

FWeaponProcs UGielinorRules::RollWeaponProcs(const AGielinorPlayerState* PS, int32 Damage)
{
	FWeaponProcs P;
	if (Damage <= 0) return P;
	const float Dbl = GearFx(PS, TEXT("doubleHit")), Ls = GearFx(PS, TEXT("lifesteal")), Pc = GearFx(PS, TEXT("poison")), St = GearFx(PS, TEXT("stun"));
	if (Dbl > 0.f && Rand() < Dbl) P.bAgain = true;
	if (Ls > 0.f) P.Heal = FMath::FloorToInt(Damage * Ls);
	if (Pc > 0.f && Rand() < Pc) P.Poison = FMath::RoundToInt(GearFx(PS, TEXT("poisonDmg")));
	if (St > 0.f && Rand() < St) P.bStun = true;
	return P;
}

TMap<FName, float> UGielinorRules::CombatXp(const AGielinorPlayerState* PS, int32 Damage)
{
	TMap<FName, float> Out;
	if (Damage <= 0) return Out;
	const FCombatStyle* S = CurrentStyle(PS);
	TArray<FName> Skills = S && S->XpSkills.Num() ? S->XpSkills : TArray<FName>{ IsRanged(PS) ? FName(TEXT("ranged")) : FName(TEXT("attack")) };
	const float Each = Damage * 1.2f / Skills.Num();
	for (FName K : Skills) Out.FindOrAdd(K) += Each;
	Out.FindOrAdd(TEXT("hitpoints")) += Damage * 0.4f;
	return Out;
}

float UGielinorRules::MonsterAccuracy(const UMobDef* Mob, const AGielinorPlayerState* PS)
{
	const FItemBonuses B = EquipmentBonuses(PS);
	const FCombatStyle* S = CurrentStyle(PS);
	const float Att = ((Mob ? Mob->AttackLevel : 1) + 8.f) * 64.f;
	const int32 Armour = Mob && Mob->bRanged ? B.RangedDefence : B.Defence;
	const float Def = (BoostedLevel(PS, TEXT("defence")) * (1.f + PrayerFx(PS, TEXT("def"))) + (S ? S->DefenceBonus : 0) + 8.f) * FMath::Max(16.f, 64.f + Armour * 3.f);
	return HitChance(Att, Def);
}

int32 UGielinorRules::RollMonsterHit(const UMobDef* Mob, const AGielinorPlayerState* PS)
{
	if (!Mob || Rand() >= MonsterAccuracy(Mob, PS)) return 0;
	return FMath::FloorToInt(Rand() * (Mob->MaxHit + 1));
}

int32 UGielinorRules::ReduceIncoming(const AGielinorPlayerState* PS, int32 Damage, bool bFortify, bool bRage)
{
	if (Damage <= 0) return 0;
	const float Cap = Db() ? Db()->DamageReductionCap : 0.6f;
	float M = 1.f - FMath::Min(Cap, Perk(PS, TEXT("defence"), TEXT("dr")) + QuestFx(PS, TEXT("dr")) + SetFx(PS, TEXT("dr")));
	M *= 1.f - PrayerFx(PS, TEXT("protect"));
	if (bFortify) M *= 0.5f;
	if (bRage) M *= 1.1f;
	return FMath::Max(0, FMath::RoundToInt(Damage * M));
}

// ---------------------------------------------------------------- magic
TArray<TPair<UItemDef*, int32>> UGielinorRules::RuneCost(const AGielinorPlayerState* PS, const USpellDef* Spell)
{
	TArray<TPair<UItemDef*, int32>> Out;
	if (!Spell) return Out;
	const UItemDef* W = Worn(PS, EEquipSlot::Weapon);
	const FName Free = W ? W->RuneSource : NAME_None;
	for (const FRuneCost& R : Spell->Runes) if (R.Rune && R.Rune->Id != Free) Out.Emplace(R.Rune.Get(), R.Amount);
	return Out;
}

bool UGielinorRules::CanCast(const AGielinorPlayerState* PS, const USpellDef* Spell, FText& OutReason)
{
	if (!Spell || !PS) { OutReason = LOCTEXT("Choose", "Choose a spell first."); return false; }
	if (Lvl(PS, TEXT("magic")) < Spell->Level) { OutReason = FText::Format(LOCTEXT("MagLvl", "You need a Magic level of {0} to cast {1}."), Spell->Level, Spell->DisplayName); return false; }
	for (const TPair<UItemDef*, int32>& R : RuneCost(PS, Spell))
		if (PS->Inventory->CountOf(R.Key) < R.Value) { OutReason = FText::Format(LOCTEXT("Runes", "You don't have enough runes to cast {0}."), Spell->DisplayName); return false; }
	return true;
}

USpellDef* UGielinorRules::BestSpell(const AGielinorPlayerState* PS)
{
	USpellDef* Best = nullptr; FText Dummy;
	if (const UGameDatabase* D = Db()) for (USpellDef* S : D->Spells) if (CanCast(PS, S, Dummy)) Best = S;
	return Best;
}

int32 UGielinorRules::MagicMaxHit(const AGielinorPlayerState* PS, const USpellDef* Spell)
{
	if (!Spell) return 0;
	return FMath::FloorToInt(Spell->MaxHit * (1.f + Perk(PS, TEXT("magic"), TEXT("magicDmg")) + EquipmentBonuses(PS).Magic * 0.01f));
}

float UGielinorRules::MagicAccuracy(const AGielinorPlayerState* PS, const UMobDef* Mob)
{
	const float Att = (BoostedLevel(PS, TEXT("magic")) + 8.f) * FMath::Max(16.f, 64.f + EquipmentBonuses(PS).Magic * 3.f);
	const float Def = ((Mob ? Mob->DefenceLevel : 1) + 8.f) * 64.f;
	return FMath::Min(0.98f, HitChance(Att, Def) * (1.f + Perk(PS, TEXT("magic"), TEXT("magicAcc"))));
}

// ---------------------------------------------------------------- activities
float UGielinorRules::AgilitySuccess(int32 Level, int32 Required, float FailLess)
{
	const float Fail = FMath::Clamp(0.25f - (Level - Required) * 0.012f, 0.02f, 0.5f);
	return 1.f - Fail * (1.f - FMath::Min(0.9f, FailLess));
}

float UGielinorRules::ThieveChance(int32 Level, int32 Required, float Bonus) { return FMath::Clamp(0.6f + (Level - Required) * 0.012f + Bonus, 0.1f, 0.95f); }

int32 UGielinorRules::RunesPerEssence(const URuneAltarDef* Altar, int32 Level)
{
	return Altar && Altar->MultiplierEvery > 0 ? 1 + Level / Altar->MultiplierEvery : 1;
}

int32 UGielinorRules::SellPrice(const AGielinorPlayerState* PS, const UItemDef* Item)
{
	const int32 Base = FMath::Max(1, FMath::FloorToInt((Item ? Item->Value : 1) * 0.6f));
	return PS ? FMath::Max(1, FMath::FloorToInt(Base * (1.f + QuestFx(PS, TEXT("sellBonus"))))) : Base;
}

int32 UGielinorRules::BuyPrice(const AGielinorPlayerState* PS, int32 Price)
{
	return PS ? FMath::Max(1, FMath::CeilToInt(Price * (1.f - QuestFx(PS, TEXT("shopDiscount"))))) : Price;
}

int32 UGielinorRules::HealAmount(const AGielinorPlayerState* PS, const UItemDef* Food)
{
	if (!Food || Food->HealAmount <= 0) return 0;
	const float Bonus = FMath::Min(1.f, Perk(PS, TEXT("hitpoints"), TEXT("healBonus")) + Perk(PS, TEXT("cooking"), TEXT("healBonus")));
	return FMath::RoundToInt(Food->HealAmount * (1.f + Bonus)); // HealAmount is already x HEAL_SCALE
}

float UGielinorRules::BuryXp(const AGielinorPlayerState* PS, const UItemDef* Bones)
{
	return Bones && Bones->PrayerXp > 0.f ? Bones->PrayerXp * (1.f + Perk(PS, TEXT("prayer"), TEXT("buryXp"))) : 0.f;
}

int32 UGielinorRules::PotionTicks(const AGielinorPlayerState* PS, int32 BaseTicks)
{
	const float More = Perk(PS, TEXT("herblore"), TEXT("potionTime")) + QuestFx(PS, TEXT("potionTime")) + GearFx(PS, TEXT("potionTime"));
	return FMath::RoundToInt(BaseTicks * (1.f + More));
}

float UGielinorRules::PrayerDrainPerTick(const AGielinorPlayerState* PS)
{
	const UGameDatabase* D = Db();
	if (!PS || !PS->Stats || !D) return 0.f;
	float Drain = 0.f;
	for (FName Id : PS->Stats->ActivePrayers) if (const UPrayerDef* P = D->FindPrayer(Id)) Drain += P->DrainPerTick;
	const float Less = FMath::Min(0.6f, Perk(PS, TEXT("prayer"), TEXT("drainLess")) + QuestFx(PS, TEXT("drainLess")));
	return Drain * (1.f - Less) / (1.f + FMath::Max(0, EquipmentBonuses(PS).Prayer) / 30.f);
}

#undef LOCTEXT_NAMESPACE
