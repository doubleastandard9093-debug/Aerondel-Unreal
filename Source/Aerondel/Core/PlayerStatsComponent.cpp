#include "Core/PlayerStatsComponent.h"
#include "Core/AerondelPlayerState.h"
#include "Core/AerondelEvents.h"
#include "Rules/AerondelRules.h"
#include "Skills/SkillsComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/EquipmentComponent.h"
#include "Quests/QuestComponent.h"
#include "Combat/HealthComponent.h"
#include "Data/GameDatabase.h"
#include "Features/VenturesFeature.h"
#include "Data/ContentDefs.h"
#include "Data/ItemDef.h"
#include "Data/MobDef.h"
#include "Data/QuestDef.h"
#include "Data/SkillDef.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Features/AerondelFeature.h"
#include "Combat/AerondelMob.h"
#include "Features/PathsFeature.h"
#include "Features/EconomyFeature.h"
#include "Engine/GameInstance.h"

#define LOCTEXT_NAMESPACE "AerondelStats"

int32 FPoisonState::Tick(float Resist)
{
	if (Damage <= 0) return 0;
	if (--Next > 0) return 0;
	Next = UAerondelRules::PoisonEvery;
	const int32 D = FMath::Max(1, FMath::RoundToInt(Damage * (1.f - Resist)));
	Damage -= 2;
	if (Damage <= 0) Damage = 0;
	return D;
}

bool FPoisonState::Apply(int32 Dmg)
{
	if (Dmg <= 0 || Damage >= Dmg) return false;
	Damage = Dmg; Next = UAerondelRules::PoisonEvery;
	return true;
}

UPlayerStatsComponent::UPlayerStatsComponent() { PrimaryComponentTick.bCanEverTick = false; }

AAerondelPlayerState* UPlayerStatsComponent::PS() const { return Cast<AAerondelPlayerState>(GetOwner()); }
void UPlayerStatsComponent::Msg(const FText& T) const { if (UAerondelEvents* Ev = UAerondelEvents::Get(this)) Ev->Message(T); }

void UPlayerStatsComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UWorld* W = GetWorld())
		W->GetTimerManager().SetTimer(TickTimer, this, &UPlayerStatsComponent::GameTickFn, UAerondelRules::TickSeconds, true);
}

void UPlayerStatsComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UWorld* W = GetWorld()) W->GetTimerManager().ClearTimer(TickTimer);
	Super::EndPlay(Reason);
}

int32 UPlayerStatsComponent::BoostOf(FName SkillId) const
{
	int32 B = 0;
	for (const FActiveEffect& E : Effects) if (const int32* V = E.Boost.Find(SkillId)) B = FMath::Max(B, *V);
	return B;
}

float UPlayerStatsComponent::EffectFx(FName Key) const
{
	float V = 0.f;
	for (const FActiveEffect& E : Effects) if (const float* F = E.Fx.Find(Key)) V = FMath::Max(V, *F);
	return V;
}

void UPlayerStatsComponent::RefreshMaxHp(bool bRefill)
{
	if (AAerondelPlayerState* S = PS()) S->Health->SetMax(UAerondelRules::MaxHitpoints(S), bRefill);
}

void UPlayerStatsComponent::GameTickFn()
{
	AAerondelPlayerState* S = PS();
	if (!S) return;
	++GameTick;
	bool bDirty = false;
	UHealthComponent* H = S->Health;
	// Hitpoint regen: 1 HP per 6 ticks, faster with perks / Swift Mending / gear.
	if (H && H->GetCurrent() < H->GetMax() && H->GetCurrent() > 0)
	{
		float Rate = 1.f + UAerondelRules::Perk(S, TEXT("hitpoints"), TEXT("regen")) + UAerondelRules::PrayerFx(S, TEXT("regen")) + UAerondelRules::GearFx(S, TEXT("regen"));
		if (GameTick < WardUntil) Rate += 2.f; // Warding Hymn
		if (H->GetCurrent() < H->GetMax() * 0.3f) Rate *= 1.f + UAerondelRules::Perk(S, TEXT("hitpoints"), TEXT("lowRegen"));
		RegenAcc += Rate / 6.f;
		int32 Heal = 0;
		while (RegenAcc >= 1.f) { RegenAcc -= 1.f; ++Heal; }
		if (Heal > 0) H->Heal(Heal);
	}
	// Prayer drain and regen.
	if (ActivePrayers.Num() > 0)
	{
		PrayerPoints = FMath::Max(0.f, PrayerPoints - UAerondelRules::PrayerDrainPerTick(S));
		if (PrayerPoints <= 0.f) { ActivePrayers.Reset(); Msg(LOCTEXT("PrayerOut", "You have run out of prayer points. Recharge them at an altar.")); }
		bDirty = true;
	}
	const float PpRegen = UAerondelRules::Perk(S, TEXT("prayer"), TEXT("ppRegen")) + UAerondelRules::QuestFx(S, TEXT("ppRegen"));
	const float PpMax = UAerondelRules::MaxPrayerPoints(S);
	if (PpRegen > 0.f && PrayerPoints < PpMax) { PrayerPoints = FMath::Min(PpMax, PrayerPoints + PpRegen); bDirty = true; }
	// Effect timers.
	for (int32 i = Effects.Num() - 1; i >= 0; --i)
	{
		FActiveEffect& E = Effects[i];
		--E.TicksLeft;
		if (E.TicksLeft == 50) Msg(FText::Format(LOCTEXT("WearingOff", "Your {0} is wearing off."), FText::FromName(E.Id)));
		if (E.TicksLeft <= 0) { Msg(FText::Format(LOCTEXT("WornOff", "Your {0} has worn off."), FText::FromName(E.Id))); Effects.RemoveAt(i); bDirty = true; }
	}
	if (PoisonImmuneTicks > 0 && --PoisonImmuneTicks == 0) Msg(LOCTEXT("ImmuneOff", "Your poison protection has worn off."));
	if (Poison.IsActive() && H && H->GetCurrent() > 0)
	{
		if (const int32 Pd = Poison.Tick())
		{
			ApplyPlayerDamage(Pd, nullptr);
			if (!Poison.IsActive()) Msg(LOCTEXT("PoisonGone", "The poison has worn off."));
		}
	}
	if (!QueuedAbility.IsNone() && GameTick - QueuedAt > 12) QueuedAbility = NAME_None;
	// Run energy regen while not running.
	const APawn* Pawn = S->GetPawn();
	const bool bMoving = Pawn && Pawn->GetVelocity().SizeSquared2D() > 100.f;
	if (bRunning && bMoving) { RunEnergy = FMath::Max(0.f, RunEnergy - 0.67f); if (RunEnergy <= 0.f) bRunning = false; bDirty = true; }
	else if (RunEnergy < 100.f)
	{
		RunEnergy = FMath::Min(100.f, RunEnergy + 0.45f * (1.f + UAerondelRules::QuestFx(S, TEXT("runRegen")) + S->Skills->GetLevelById(TEXT("agility")) / 100.f));
		bDirty = true;
	}
	if (bDirty) OnChanged.Broadcast();
}

FText UPlayerStatsComponent::TogglePrayer(FName PrayerId)
{
	AAerondelPlayerState* S = PS();
	const UGameDatabase* Db = UGameDatabase::Get();
	const UPrayerDef* P = Db ? Db->FindPrayer(PrayerId) : nullptr;
	if (!S || !P) return LOCTEXT("Unknown", "Unknown prayer.");
	if (ActivePrayers.Remove(PrayerId) > 0) { OnChanged.Broadcast(); return FText::GetEmpty(); }
	if (S->Skills->GetLevelById(TEXT("prayer")) < P->Level) return FText::Format(LOCTEXT("PLvl", "You need a Prayer level of {0} to use {1}."), P->Level, P->DisplayName);
	if (!P->RequiredQuest.IsNone())
	{
		const UQuestDef* Q = Db->FindQuest(P->RequiredQuest);
		if (Q && S->Quests->GetState(Q) != EQuestState::Completed) return FText::Format(LOCTEXT("PQuest", "You need to complete {0} to use {1}."), Q->DisplayName, P->DisplayName);
	}
	if (PrayerPoints <= 0.f) return LOCTEXT("PEmpty", "You need to recharge your prayer points at an altar.");
	// Same group is exclusive; ranged clashes with attack and strength.
	TSet<FName> Off = { P->Group };
	if (P->Group == TEXT("rng")) { Off.Add(TEXT("att")); Off.Add(TEXT("str")); }
	if (P->Group == TEXT("att") || P->Group == TEXT("str")) Off.Add(TEXT("rng"));
	ActivePrayers.RemoveAll([&](FName Id) { const UPrayerDef* O = Db->FindPrayer(Id); return !O || Off.Contains(O->Group); });
	ActivePrayers.Add(PrayerId);
	OnChanged.Broadcast();
	return FText::GetEmpty();
}

int32 UPlayerStatsComponent::RechargePrayer()
{
	const float Max = UAerondelRules::MaxPrayerPoints(PS());
	const int32 Gained = FMath::RoundToInt(Max - PrayerPoints);
	PrayerPoints = Max;
	OnChanged.Broadcast();
	return Gained;
}

bool UPlayerStatsComponent::Consume(int32 InventorySlot)
{
	AAerondelPlayerState* S = PS();
	if (!S) return false;
	UItemDef* Item = S->Inventory->GetSlot(InventorySlot).Item;
	if (!Item) return false;
	const UGameDatabase* Db = UGameDatabase::Get();
	if (const UPotionDef* P = Db ? Db->FindPotionForItem(Item) : nullptr)
	{
		S->Inventory->RemoveFromSlot(InventorySlot, 1);
		if (UItemDef* Vial = Db->FindItem(TEXT("vial"))) S->Inventory->Add(Vial, 1, false);
		const float PpMax = UAerondelRules::MaxPrayerPoints(S);
		if (P->Instant == TEXT("energy")) { RunEnergy = FMath::Min(100.f, RunEnergy + 30.f); Msg(LOCTEXT("Energy", "You drink the energy potion. You feel refreshed.")); }
		else if (P->Instant == TEXT("prayer") || P->Instant == TEXT("restore"))
		{
			const int32 L = S->Skills->GetLevelById(TEXT("prayer"));
			const int32 Amt = FMath::FloorToInt((P->Instant == TEXT("prayer") ? 7.f : 8.f) + L / 4.f) * 10;
			PrayerPoints = FMath::Min(PpMax, PrayerPoints + Amt);
			if (P->Instant == TEXT("restore")) RunEnergy = FMath::Min(100.f, RunEnergy + 20.f);
			Msg(LOCTEXT("PrayRestore", "Your prayer points are restored."));
		}
		else if (P->Instant == TEXT("antipoison"))
		{
			Poison = FPoisonState();
			PoisonImmuneTicks = UAerondelRules::PotionTicks(S, P->ImmuneTicks > 0 ? P->ImmuneTicks : 150);
			Msg(LOCTEXT("Anti", "You drink the antipoison. You are cured and protected from poison for a while."));
		}
		else
		{
			FActiveEffect E; E.Id = Item->Id; E.TicksLeft = UAerondelRules::PotionTicks(S, UAerondelRules::PotionBaseTicks); E.Fx = P->Fx;
			TSet<FName> Skills; for (const auto& F : P->BoostFlat) Skills.Add(F.Key); for (const auto& F : P->BoostPct) Skills.Add(F.Key);
			for (FName Sk : Skills)
			{
				const int32* Flat = P->BoostFlat.Find(Sk); const float* Pct = P->BoostPct.Find(Sk);
				E.Boost.Add(Sk, (Flat ? *Flat : 0) + FMath::FloorToInt(S->Skills->GetLevelById(Sk) * (Pct ? *Pct : 0.f)));
			}
			Effects.RemoveAll([&](const FActiveEffect& X) { return X.Id == E.Id; });
			Effects.Add(E);
			Msg(FText::Format(LOCTEXT("Drink", "You drink the {0}."), Item->DisplayName));
		}
		OnChanged.Broadcast();
		return true;
	}
	if (Item->IsFood())
	{
		S->Inventory->RemoveFromSlot(InventorySlot, 1);
		const int32 Before = S->Health->GetCurrent();
		S->Health->Heal(UAerondelRules::HealAmount(S, Item));
		Msg(FText::Format(LOCTEXT("Eat", "You eat the {0}. It heals {1} hitpoints."), Item->DisplayName, S->Health->GetCurrent() - Before));
		if (Item->BuffPrayer > 0)
		{
			PrayerPoints = FMath::Min<float>(UAerondelRules::MaxPrayerPoints(S), PrayerPoints + Item->BuffPrayer * 10);
			Msg(LOCTEXT("EatPray", "It restores some prayer points."));
		}
		else if (Item->BuffBoostFlat.Num() > 0 && Item->BuffTicks > 0)
		{
			FActiveEffect E; E.Id = Item->Id; E.TicksLeft = Item->BuffTicks;
			for (const auto& F : Item->BuffBoostFlat)
			{
				const float* Pct = Item->BuffBoostPct.Find(F.Key);
				E.Boost.Add(F.Key, F.Value + FMath::FloorToInt(S->Skills->GetLevelById(F.Key) * (Pct ? *Pct : 0.f)));
			}
			Effects.RemoveAll([&](const FActiveEffect& X) { return X.Id == E.Id; });
			Effects.Add(E);
		}
		OnChanged.Broadcast();
		return true;
	}
	return false;
}

bool UPlayerStatsComponent::BuryBones(int32 InventorySlot)
{
	AAerondelPlayerState* S = PS();
	UItemDef* Item = S ? S->Inventory->GetSlot(InventorySlot).Item.Get() : nullptr;
	if (!Item || Item->PrayerXp <= 0.f) return false;
	const UGameDatabase* Db = UGameDatabase::Get();
	S->Inventory->RemoveFromSlot(InventorySlot, 1);
	S->Skills->AddXp(Db ? Db->FindSkill(TEXT("prayer")) : nullptr, UAerondelRules::BuryXp(S, Item) * UAerondelRules::XpMultiplier(S, TEXT("prayer")));
	Msg(LOCTEXT("Bury", "You bury the bones."));
	if (UAerondelEvents* Ev = UAerondelEvents::Get(this)) Ev->OnBonesBuried.Broadcast(Item);
	return true;
}

bool UPlayerStatsComponent::CleanHerb(int32 InventorySlot)
{
	AAerondelPlayerState* S = PS();
	UItemDef* Item = S ? S->Inventory->GetSlot(InventorySlot).Item.Get() : nullptr;
	const UGameDatabase* Db = UGameDatabase::Get();
	if (!Item || Item->CleansInto.IsNone() || !Db) return false;
	UItemDef* Clean = Db->FindItem(Item->CleansInto);
	if (!Clean) return false;
	if (S->Skills->GetLevelById(TEXT("herbalism")) < Item->HerbLevel)
	{
		Msg(FText::Format(LOCTEXT("HerbLvl", "You need a Herbalism level of {0} to clean this herb."), Item->HerbLevel)); return false;
	}
	S->Inventory->RemoveFromSlot(InventorySlot, 1);
	S->Inventory->Add(Clean, 1);
	S->Skills->AddXp(Db->FindSkill(TEXT("herbalism")), FMath::Max(1.f, Item->HerbLevel * 0.25f + 2.5f) * UAerondelRules::XpMultiplier(S, TEXT("herbalism")));
	return true;
}

bool UPlayerStatsComponent::AddTool(UItemDef* Tool)
{
	if (!Tool || Tool->ToolType == EToolType::None || Toolbelt.Contains(Tool)) return false;
	Toolbelt.Add(Tool);
	OnChanged.Broadcast();
	return true;
}

bool UPlayerStatsComponent::ApplyPoison(int32 Damage)
{
	if (PoisonImmuneTicks > 0) return false;
	return Poison.Apply(Damage);
}

FText UPlayerStatsComponent::UseAbility(FName AbilityId, bool bHasTarget)
{
	AAerondelPlayerState* S = PS();
	const UGameDatabase* Db = UGameDatabase::Get();
	const UAbilityDef* A = Db ? Db->FindAbility(AbilityId) : nullptr;
	if (!S || !A) return LOCTEXT("NoAb", "Unknown ability.");
	if (S->Skills->GetLevelById(A->Skill) < A->Level) return FText::Format(LOCTEXT("AbLvl", "You need {0} level {1} to use {2}."), FText::FromName(A->Skill), A->Level, A->DisplayName);
	if (!A->Path.IsNone())
	{
		const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
		const UPathsFeature* Paths = GI ? GI->GetSubsystem<UPathsFeature>() : nullptr;
		if (!Paths || Paths->GetPath() != A->Path) return FText::Format(LOCTEXT("AbPath", "{0} belongs to another Combat Path."), A->DisplayName);
	}
	if (CooldownSecondsLeft(AbilityId) > 0.f) return FText::Format(LOCTEXT("AbCd", "{0} is recharging."), A->DisplayName);
	const bool bRanged = UAerondelRules::IsRanged(S);
	if (A->Style == TEXT("melee") && bRanged) return FText::Format(LOCTEXT("AbMelee", "{0} needs a melee weapon."), A->DisplayName);
	if (A->Style == TEXT("ranged") && !bRanged) return FText::Format(LOCTEXT("AbBow", "{0} needs a bow."), A->DisplayName);
	const float Cost = A->Kind == EAbilityKind::Threshold ? 50.f : A->Kind == EAbilityKind::Ultimate ? 100.f : 0.f;
	if (Cost > 0.f && Adrenaline + 1e-4f < Cost) return FText::Format(LOCTEXT("AbAdr", "{0} needs {1}% adrenaline."), A->DisplayName, FMath::RoundToInt(Cost));
	if (A->Style == TEXT("magic") && AutocastSpell.IsNone()) return FText::Format(LOCTEXT("AbMagic", "{0} rides on a spell: set an autocast spell first."), A->DisplayName);
	const bool bSelf = A->Special == TEXT("fortify") || A->Special == TEXT("rage") || A->Special == TEXT("rejuv")
		|| A->Special == TEXT("wall") || A->Special == TEXT("ward") || A->Special == TEXT("mend");
	if (!bSelf && !bHasTarget) return LOCTEXT("AbTarget", "Attack something first, then use that.");
	if (Cost > 0.f) Adrenaline -= Cost; else GainAdrenaline(9.f);
	const int32 Cd = FMath::Max(0, FMath::RoundToInt(A->Cooldown * (1.f - UAerondelRules::Perk(S, TEXT("attack"), TEXT("abilityCd")))));
	if (A->Special == TEXT("fortify")) { FortifyUntil = GameTick + 10; CooldownReady.Add(AbilityId, GameTick + Cd); }
	else if (A->Special == TEXT("rage")) { RageUntil = GameTick + 15; CooldownReady.Add(AbilityId, GameTick + Cd); }
	else if (A->Special == TEXT("rejuv"))
	{
		S->Health->Heal(FMath::RoundToInt(S->Health->GetMax() * 0.2f));
		CooldownReady.Add(AbilityId, GameTick + Cd);
	}
	else if (A->Special == TEXT("wall")) { WallUntil = GameTick + 7; CooldownReady.Add(AbilityId, GameTick + Cd); }
	else if (A->Special == TEXT("ward")) { WardUntil = GameTick + 10; CooldownReady.Add(AbilityId, GameTick + Cd); }
	else if (A->Special == TEXT("mend"))
	{
		S->Health->Heal(FMath::RoundToInt(S->Health->GetMax() * 0.12f));
		CooldownReady.Add(AbilityId, GameTick + Cd);
	}
	else { QueuedAbility = AbilityId; QueuedAt = GameTick; }
	Msg(FText::Format(LOCTEXT("AbUse", "{0}!"), A->DisplayName));
	OnChanged.Broadcast();
	return FText::GetEmpty();
}

float UPlayerStatsComponent::CooldownSecondsLeft(FName AbilityId) const
{
	const int64* R = CooldownReady.Find(AbilityId);
	return R && *R > GameTick ? (*R - GameTick) * UAerondelRules::TickSeconds : 0.f;
}

FName UPlayerStatsComponent::ConsumeQueuedAbility()
{
	const FName Q = QueuedAbility;
	QueuedAbility = NAME_None;
	if (!Q.IsNone())
		if (const UGameDatabase* Db = UGameDatabase::Get())
			if (const UAbilityDef* A = Db->FindAbility(Q))
				CooldownReady.Add(Q, GameTick + FMath::Max(0, FMath::RoundToInt(A->Cooldown * (1.f - UAerondelRules::Perk(PS(), TEXT("attack"), TEXT("abilityCd"))))));
	return Q;
}

bool UPlayerStatsComponent::UseAmmo(FText& OutError)
{
	AAerondelPlayerState* S = PS();
	const UItemDef* W = S ? S->Equipment->GetEquipped(EEquipSlot::Weapon) : nullptr;
	if (W && W->bThrown) return true;
	UItemDef* Ammo = S ? S->Equipment->GetEquipped(EEquipSlot::Ammo) : nullptr;
	if (!Ammo || AmmoCount <= 0) { OutError = LOCTEXT("NoAmmo", "There is no ammo left in your quiver."); return false; }
	const bool bXbow = W && W->Id.ToString().Contains(TEXT("crossbow"));
	const bool bBolt = Ammo->Id.ToString().EndsWith(TEXT("bolts"));
	if (bXbow != bBolt) { OutError = bXbow ? LOCTEXT("NeedBolts", "Your crossbow needs bolts.") : LOCTEXT("NeedArrows", "Your bow needs arrows."); return false; }
	if (FMath::FRand() < UAerondelRules::Perk(S, TEXT("ranged"), TEXT("ammoSave"))) return true;
	if (--AmmoCount <= 0) { AmmoCount = 0; S->Equipment->SetRaw(EEquipSlot::Ammo, nullptr); S->Equipment->OnChanged.Broadcast(); }
	return true;
}

void UPlayerStatsComponent::ApplyPlayerDamage(int32 Amount, AActor* Source)
{
	AAerondelPlayerState* S = PS();
	if (!S || Amount <= 0 || S->Health->IsDead()) return;
	const AAerondelMob* Mob = Cast<AAerondelMob>(Source);
	const float HpFrac = S->Health->GetMax() > 0 ? (float)S->Health->GetCurrent() / S->Health->GetMax() : 1.f;
	const float Mult = UAerondelFeature::TotalDamageTakenMult(this, Mob ? Mob->Definition.Get() : nullptr, HpFrac);
	if (!FMath::IsNearlyEqual(Mult, 1.f)) Amount = FMath::Max(0, FMath::RoundToInt(Amount * Mult));
	if (Mob && Mob->IsWithered()) Amount = FMath::Max(1, FMath::FloorToInt(Amount * 0.7f));
	const float Red = (GameTick < WallUntil ? 0.6f : 0.f) + (GameTick < WardUntil ? 0.4f : 0.f); // uncapped (web v21)
	if (Red > 0.f) Amount = FMath::Max(0, FMath::RoundToInt(Amount * FMath::Max(0.f, 1.f - Red)));
	S->Health->ApplyDamage(Amount, Source);
	UVenturesFeature::HookPlayerHurt(this, Amount);
	if (UEconomyFeature* Econ = UEconomyFeature::Get(this)) Econ->OnPlayerHit();
	if (S->Health->IsDead()) { HandleDeath(); UAerondelFeature::NotifyPlayerDied(this); }
}

void UPlayerStatsComponent::HandleDeath()
{
	AAerondelPlayerState* S = PS();
	if (!S) return;
	Msg(LOCTEXT("Dead", "Oh dear, you are dead! You wake up back in town. You keep all of your items."));
	ActivePrayers.Reset();
	QueuedAbility = NAME_None;
	FortifyUntil = RageUntil = StunUntil = WallUntil = WardUntil = 0;
	SurgeLeft = 0;
	Poison = FPoisonState();
	RunEnergy = 100.f;
	RefreshMaxHp(true);
	if (APawn* Pawn = S->GetPawn())
	{
		AActor* Start = nullptr;
		if (AGameModeBase* GM = GetWorld()->GetAuthGameMode()) Start = GM->FindPlayerStart(Pawn->GetController());
		if (Start) Pawn->TeleportTo(Start->GetActorLocation(), Start->GetActorRotation());
	}
	OnDied.Broadcast();
	OnChanged.Broadcast();
}

bool UPlayerStatsComponent::StartBounty(FName Id)
{
	const UGameDatabase* Db = UGameDatabase::Get();
	if (!Db || !Db->Bounties.ContainsByPredicate([&](const FBountyDef& B) { return B.Id == Id; })) return false;
	BountyId = Id; BountyKills = 0;
	OnChanged.Broadcast();
	return true;
}

void UPlayerStatsComponent::OnMobKilledForBounty(const UMobDef* Mob)
{
	const UGameDatabase* Db = UGameDatabase::Get();
	AAerondelPlayerState* S = PS();
	if (BountyId.IsNone() || !Db || !Mob || !S) return;
	const FBountyDef* B = Db->Bounties.FindByPredicate([&](const FBountyDef& X) { return X.Id == BountyId; });
	if (!B || B->MobId != Mob->Id) return;
	if (++BountyKills < B->Count) return;
	if (B->Coins > 0) S->Inventory->Add(Db->FindItem(TEXT("coins")), B->Coins, true);
	for (const auto& X : B->Xp) S->Skills->AddXp(Db->FindSkill(X.Key), X.Value);
	Msg(LOCTEXT("Bounty", "Bounty complete!"));
	BountyId = NAME_None; BountyKills = 0;
	OnChanged.Broadcast();
}

void UPlayerStatsComponent::ResetRuntime()
{
	ActivePrayers.Reset(); Effects.Reset(); Poison = FPoisonState(); PoisonImmuneTicks = 0;
	RunEnergy = 100.f; Adrenaline = 0.f; Toolbelt.Reset(); AmmoCount = 0; AttackStyle = 0;
	AutocastSpell = NAME_None; AgilityCourse = NAME_None; AgilityNext = 0; BountyId = NAME_None; BountyKills = 0;
	PrayerPoints = UAerondelRules::MaxPrayerPoints(PS());
}

#undef LOCTEXT_NAMESPACE
