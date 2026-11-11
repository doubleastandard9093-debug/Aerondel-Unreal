#include "Features/VenturesFeature.h"
#include "Features/KinbeastPet.h"
#include "UI/AerondelPanels.h"
#include "Core/AerondelPlayerState.h"
#include "Inventory/InventoryComponent.h"
#include "Data/ItemDef.h"
#include "Rules/AerondelRules.h"
#include "Combat/AerondelMob.h"
#include "Data/MobDef.h"
#include "Combat/HealthComponent.h"
#include "Engine/World.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// Ventures group 20 (web adapt/kinbeasts.js): catchable, trainable pets that fight beside you.
// Hunter gates the catch and the training. One Kinbeast or one Ashkin, never both.
// Companions (Bond) are unchanged and can still walk with you.

namespace VenturesKin
{
	struct FSpecies
	{
		const TCHAR* Id; const TCHAR* Name; const TCHAR* Type; int32 Hun; int32 Vessel;
		float R, G, B;
		const TCHAR* Snap; const TCHAR* Pin;
	};
	static const FSpecies Species[] = {
		{ TEXT("wolf"),   TEXT("Wolf"),            TEXT("Wolf"),            1,  1, 0.48f, 0.48f, 0.50f, TEXT("Snap"),  TEXT("Pin") },
		{ TEXT("spider"), TEXT("Giant Spider"),    TEXT("Giant Spider"),    15, 1, 0.16f, 0.16f, 0.16f, TEXT("Bite"),  TEXT("Web") },
		{ TEXT("fen"),    TEXT("Fen Crawler"),     TEXT("Fen Crawler"),     30, 2, 0.23f, 0.29f, 0.16f, TEXT("Lunge"), TEXT("Latch") },
		{ TEXT("raider"), TEXT("Ashen raider"),    TEXT("Ashen raider"),    40, 2, 0.23f, 0.23f, 0.23f, TEXT("Cut"),   TEXT("Shove") },
		{ TEXT("reaver"), TEXT("Sand Reaver"),     TEXT("Sand Reaver"),     55, 2, 0.54f, 0.42f, 0.23f, TEXT("Claw"),  TEXT("Dust") },
		{ TEXT("rime"),   TEXT("Rime Wraith"),     TEXT("Rime Wraith"),     70, 3, 0.66f, 0.82f, 0.91f, TEXT("Chill"), TEXT("Frost") },
		{ TEXT("voidk"),  TEXT("Voidborn Knight"), TEXT("Voidborn Knight"), 90, 3, 0.16f, 0.10f, 0.29f, TEXT("Edge"),  TEXT("Bind") },
	};
	struct FVessel { const TCHAR* Name; int32 Tier; float Power; int32 Hun; int32 Price; const TCHAR* Desc; };
	static const FVessel Vessels[] = {
		{ TEXT("Bind vessel"),   1, 1.f,    1,  20,  TEXT("A simple vessel for common beasts.") },
		{ TEXT("Sturdy vessel"), 2, 1.45f, 30,  80,  TEXT("A firmer vessel for harder hunts.") },
		{ TEXT("Fine vessel"),   3, 1.9f,  75,  220, TEXT("A fine vessel for rare frontier beasts.") },
	};
	static const FSpecies* FindSp(const FString& Id)
	{
		for (const FSpecies& S : Species) if (Id == S.Id) return &S;
		return nullptr;
	}
	static const FSpecies* ByType(const FString& Name)
	{
		for (const FSpecies& S : Species) if (Name == S.Type || Name == S.Name) return &S;
		return nullptr;
	}
	static const FVessel* FindVessel(const FString& Name)
	{
		for (const FVessel& V : Vessels) if (Name == V.Name) return &V;
		return nullptr;
	}
	static int32 NeedFor(int32 Lv) { return 40 + Lv * 14; }
	static int32 MaxHp(int32 Level) { return 10 + Level * 4; }
	static void VAct20(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
}
using namespace VenturesKin;

FKinRec* UVenturesFeature::KinRecOf(const FString& Id)
{
	for (FKinRec& R : KinRoster) if (R.Id == Id) return &R;
	return nullptr;
}

float UVenturesFeature::KinKennelMul() const
{
	const int32* Lv = HoldLv.Find(TEXT("kennel"));
	const int32 Kennel = Lv ? *Lv : 0;
	return 1.f + ((HoldHome() || GreenHome()) ? 0.15f : 0.f) + Kennel * 0.25f;
}

FString UVenturesFeature::KinBestVessel(int32 NeedTier) const
{
	const int32 Hun = SkillLevel(TEXT("Hunter"));
	for (const FVessel& V : Vessels)
		if (V.Tier >= NeedTier && Hun >= V.Hun && CountItem(V.Name) > 0) return V.Name;
	return FString();
}

void UVenturesFeature::ResetKin()
{
	RecallKin(FString(), true);
	KinRoster.Reset();
	KinSeen.Reset();
	KinActive.Reset();
	KinNextId = 1;
	KinSwapAt = 0;
	bKinOut = false;
	KinLive = FKinLive();
}

void UVenturesFeature::SaveKin(FJsonObject& A) const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Rost;
	for (const FKinRec& R : KinRoster)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("id"), R.Id);
		O->SetStringField(TEXT("species"), R.Species);
		O->SetStringField(TEXT("name"), R.Name);
		O->SetNumberField(TEXT("level"), R.Level);
		O->SetNumberField(TEXT("xp"), R.Xp);
		O->SetNumberField(TEXT("loyalty"), R.Loyalty);
		O->SetNumberField(TEXT("hp"), R.Hp);
		O->SetNumberField(TEXT("faintUntil"), (double)R.FaintUntil);
		Rost.Add(MakeShared<FJsonValueObject>(O));
	}
	S->SetArrayField(TEXT("roster"), Rost);
	TArray<TSharedPtr<FJsonValue>> Seen;
	for (const FString& Id : KinSeen) Seen.Add(MakeShared<FJsonValueString>(Id));
	S->SetArrayField(TEXT("seen"), Seen);
	S->SetStringField(TEXT("active"), KinActive);
	S->SetNumberField(TEXT("nextId"), KinNextId);
	A.SetObjectField(TEXT("kin"), S);
}

void UVenturesFeature::LoadKin(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> S = JObj(A, TEXT("kin"));
	if (!S) return;
	KinRoster.Reset();
	KinSeen.Reset();
	if (const TArray<TSharedPtr<FJsonValue>>* Rost = nullptr; S->TryGetArrayField(TEXT("roster"), Rost) && Rost)
	{
		for (const TSharedPtr<FJsonValue>& V : *Rost)
		{
			if (!V.IsValid() || V->Type != EJson::Object) continue;
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid()) continue;
			const FString Sp = JStr(*O, TEXT("species"));
			if (!FindSp(Sp) || KinRoster.Num() >= 40) continue;
			FKinRec R;
			R.Species = Sp;
			R.Id = JStr(*O, TEXT("id"));
			if (!R.Id.StartsWith(TEXT("k"))) R.Id = FString::Printf(TEXT("k%d"), KinRoster.Num() + 1);
			R.Name = JStr(*O, TEXT("name"));
			if (R.Name.IsEmpty()) R.Name = FindSp(Sp)->Name;
			R.Name = R.Name.Left(18);
			R.Level = FMath::Clamp((int32)JInt(*O, TEXT("level"), 1), 1, 1000);
			R.Xp = FMath::Max(0, (int32)JInt(*O, TEXT("xp")));
			R.Loyalty = FMath::Clamp((int32)JInt(*O, TEXT("loyalty"), 20), 0, 100);
			R.Hp = FMath::Max(0, (int32)JInt(*O, TEXT("hp"), MaxHp(R.Level)));
			R.FaintUntil = FMath::Max((int64)0, JInt(*O, TEXT("faintUntil")));
			KinRoster.Add(R);
		}
	}
	if (const TArray<TSharedPtr<FJsonValue>>* Seen = nullptr; S->TryGetArrayField(TEXT("seen"), Seen) && Seen)
		for (const TSharedPtr<FJsonValue>& V : *Seen)
			if (V.IsValid() && V->Type == EJson::String && FindSp(V->AsString())) KinSeen.AddUnique(V->AsString());
	KinActive = JStr(*S, TEXT("active"));
	if (!KinRecOf(KinActive)) KinActive = KinRoster.Num() ? KinRoster[0].Id : FString();
	KinNextId = FMath::Max(1, (int32)JInt(*S, TEXT("nextId"), 1));
	for (const FKinRec& R : KinRoster)
	{
		int32 N = 0;
		if (R.Id.StartsWith(TEXT("k"))) N = FCString::Atoi(*R.Id.Mid(1));
		KinNextId = FMath::Max(KinNextId, N + 1);
	}
}

void UVenturesFeature::RecallKin(const FString& Why, bool bSilent)
{
	if (!bKinOut && !KinLive.bOn) return;
	if (AActor* P = KinLive.Pet.Get()) P->Destroy();
	if (FKinRec* R = KinRecOf(KinLive.Id)) R->Hp = FMath::Max(0, KinLive.Hp);
	KinLive = FKinLive();
	bKinOut = false;
	if (!bSilent) Say(Why.IsEmpty() ? TEXT("You recall your Kinbeast.") : Why);
}

void UVenturesFeature::KinSpawnPet(FKinRec& R)
{
	const FSpecies* Sp = FindSp(R.Species);
	if (UWorld* World = GetWorld())
	{
		AAerondelPlayerState* PS = GetPS();
		APawn* Pawn = PS ? PS->GetPawn() : nullptr;
		FActorSpawnParameters P; P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector At = Pawn ? (Pawn->GetActorLocation() + Pawn->GetActorRightVector() * 80.f + Pawn->GetActorForwardVector() * -60.f + FVector(0.f, 0.f, 30.f)) : FVector::ZeroVector;
		if (AKinbeastPet* Pet = World->SpawnActor<AKinbeastPet>(At, FRotator::ZeroRotator, P))
		{
			const FLinearColor Col = Sp ? FLinearColor(Sp->R, Sp->G, Sp->B) : FLinearColor(0.5f, 0.5f, 0.5f);
			Pet->Setup(Pawn, Col);
			KinLive.Pet = Pet;
		}
	}
}

float UVenturesFeature::KinChance(const AAerondelMob* Mob, const FString& Vessel) const
{
	if (!Mob || Mob->GetMaxHp() <= 0) return 0.f;
	const UMobDef* D = Mob->Definition.Get();
	const FString Name = D ? D->DisplayName.ToString() : FString();
	const FSpecies* Sp = ByType(Name);
	const FVessel* V = FindVessel(Vessel);
	if (!Sp || !V) return 0.f;
	const float HpR = (float)Mob->GetHp() / (float)Mob->GetMaxHp();
	if (HpR > 0.35f) return 0.f;
	const int32 Hun = SkillLevel(TEXT("Hunter"));
	if (Hun < Sp->Hun || V->Tier < Sp->Vessel) return 0.f;
	const float Low = HpR <= 0.2f ? 1.2f : 1.f;
	const float Skill = 0.5f + FMath::Min(0.5f, Hun / 200.f);
	const float PerkB = 1.f + UAerondelRules::Perk(GetPS(), FName(TEXT("Hunter")), FName(TEXT("catchChance")));
	const float Ev = 1.f - FMath::Min(0.45f, (D && D->CombatLevel > 0 ? D->CombatLevel : Sp->Hun) / 900.f);
	return FMath::Min(0.92f, 0.22f * V->Power * Skill * PerkB * Low * Ev);
}

bool UVenturesFeature::KinThrow()
{
	AAerondelMob* Mob = CurrentTarget();
	if (!Mob || Mob->IsDead() || Mob->GetHp() <= 0) { Say(TEXT("There is nothing to bind.")); return false; }
	const UMobDef* D = Mob->Definition.Get();
	const FString Name = D ? D->DisplayName.ToString() : FString();
	const FSpecies* Sp = ByType(Name);
	if (!Sp) { Say(TEXT("This creature cannot be bound.")); return false; }
	const int32 Hun = SkillLevel(TEXT("Hunter"));
	if (Hun < Sp->Hun) { Say(FString::Printf(TEXT("You need Hunter %d to bind a %s."), Sp->Hun, Sp->Name)); return false; }
	const FString Vessel = KinBestVessel(Sp->Vessel);
	if (Vessel.IsEmpty()) { Say(TEXT("You need a vessel of this height. Craft one, or buy one from Beastbinder Cal.")); return false; }
	const FVessel* V = FindVessel(Vessel);
	if (!V || V->Tier < Sp->Vessel) { Say(FString::Printf(TEXT("A %s needs a stronger vessel."), Sp->Name)); return false; }
	if (Mob->GetMaxHp() > 0 && (float)Mob->GetHp() / (float)Mob->GetMaxHp() > 0.35f)
	{
		Say(TEXT("Weaken it first. A Kinbeast is bound when it is low, not at full strength."));
		return false;
	}
	const float Chance = KinChance(Mob, Vessel);
	TakeItem(Vessel, 1);
	KinSeen.AddUnique(Sp->Id);
	if (FMath::FRand() > Chance)
	{
		GiveXp(TEXT("Hunter"), 8.f + Sp->Hun / 5.f);
		Say(FString::Printf(TEXT("The %s shrugs off the %s."), Sp->Name, *Vessel));
		return false;
	}
	if (AActor* A = Mob) A->Destroy();
	FKinRec R;
	R.Id = FString::Printf(TEXT("k%d"), KinNextId++);
	R.Species = Sp->Id;
	R.Name = Sp->Name;
	R.Level = 1;
	R.Xp = 0;
	R.Loyalty = 20;
	R.Hp = MaxHp(1);
	if (KinRoster.Num() < 40) KinRoster.Add(R);
	if (KinActive.IsEmpty()) KinActive = R.Id;
	GiveXp(TEXT("Hunter"), 28.f + Sp->Hun);
	Say(FString::Printf(TEXT("You bind a %s. It is yours to train. (+Hunter)"), Sp->Name));
	return true;
}

bool UVenturesFeature::KinSend(const FString& InId)
{
	FString Id = InId.IsEmpty() ? KinActive : InId;
	FKinRec* R = KinRecOf(Id);
	if (!R) { Say(TEXT("Choose a Kinbeast first.")); return false; }
	if (bKinOut && KinLive.Id == R->Id) { RecallKin(TEXT("You recall your Kinbeast.")); return true; }
	if (bKinOut) RecallKin(TEXT("You recall the Kinbeast that was out."));
	const bool bFight = CurrentTarget() && !CurrentTarget()->IsDead();
	if (NowMs() < KinSwapAt && bFight) { Say(TEXT("You can only swap out of a fight, or after a short wait.")); return false; }
	if (R->FaintUntil && NowMs() < R->FaintUntil) { Say(FString::Printf(TEXT("%s is still gathering itself."), *R->Name)); return false; }
	if (AshOut()) AshRecall(TEXT("The Ashkin fades so your Kinbeast can come forth."));
	if (R->Hp <= 0) R->Hp = MaxHp(R->Level);
	KinActive = R->Id;
	KinSwapAt = NowMs() + 15000;
	KinLive = FKinLive();
	KinLive.bOn = true;
	KinLive.Id = R->Id;
	KinLive.Hp = R->Hp;
	bKinOut = true;
	KinSpawnPet(*R);
	Say(FString::Printf(TEXT("%s comes to your side."), *R->Name));
	return true;
}

bool UVenturesFeature::KinFeed(const FString& InId)
{
	FString Id = InId;
	if (Id.IsEmpty() && bKinOut) Id = KinLive.Id;
	if (Id.IsEmpty()) Id = KinActive;
	FKinRec* R = KinRecOf(Id);
	if (!R) return false;
	FString Food;
	if (AAerondelPlayerState* PS = GetPS()) if (PS->Inventory)
	{
		for (const FItemStack& Slot : PS->Inventory->GetSlots())
		{
			if (Slot.Item && Slot.Count > 0 && Slot.Item->IsFood())
			{
				Food = Slot.Item->DisplayName.ToString();
				if (Food.IsEmpty()) Food = Slot.Item->GetFName().ToString();
				break;
			}
		}
	}
	if (Food.IsEmpty() || !TakeItem(Food, 1)) { Say(TEXT("You have no food to offer.")); return false; }
	const float PerkB = 1.f + UAerondelRules::Perk(GetPS(), FName(TEXT("Hunter")), FName(TEXT("kinXp")));
	const int32 Gain = FMath::RoundToInt((6 + R->Level / 20) * PerkB * KinKennelMul());
	R->Xp += Gain;
	R->Loyalty = FMath::Min(100, R->Loyalty + 4);
	R->Hp = FMath::Min(MaxHp(R->Level), R->Hp + FMath::Max(2, FMath::RoundToInt(MaxHp(R->Level) * 0.15f)));
	if (bKinOut && KinLive.Id == R->Id) KinLive.Hp = R->Hp;
	int32 Up = 0;
	while (R->Level < 1000 && R->Xp >= NeedFor(R->Level))
	{
		R->Xp -= NeedFor(R->Level);
		R->Level++;
		R->Hp = MaxHp(R->Level);
		if (bKinOut && KinLive.Id == R->Id) KinLive.Hp = R->Hp;
		Up++;
	}
	GiveXp(TEXT("Hunter"), 6.f + Gain / 3.f);
	Say(Up ? FString::Printf(TEXT("%s grows to level %d."), *R->Name, R->Level) : FString::Printf(TEXT("You tend %s."), *R->Name));
	return true;
}

bool UVenturesFeature::KinSetActive(const FString& Id)
{
	if (!KinRecOf(Id)) return false;
	KinActive = Id;
	return true;
}

bool UVenturesFeature::KinBuy(const FString& Name)
{
	const FVessel* V = FindVessel(Name);
	if (!V) return false;
	if (SkillLevel(TEXT("Hunter")) < V->Hun) { Say(FString::Printf(TEXT("You need Hunter %d for a %s."), V->Hun, V->Name)); return false; }
	const int32 Price = UAerondelRules::BuyPrice(GetPS(), V->Price);
	if (Coins() < Price) { Say(FString::Printf(TEXT("That costs %d coins."), Price)); return false; }
	TakeCoins(Price);
	GiveItem(V->Name, 1);
	Say(FString::Printf(TEXT("You buy a %s."), V->Name));
	return true;
}

void UVenturesFeature::TickKin()
{
	if (!bKinOut || !KinLive.bOn) return;
	FKinRec* R = KinRecOf(KinLive.Id);
	if (!R) { RecallKin(TEXT("Your Kinbeast is gone."), true); return; }
	if (KinLive.Hp <= 0)
	{
		R->Hp = 0;
		R->FaintUntil = NowMs() + 20000;
		RecallKin(FString::Printf(TEXT("%s faints and returns to you."), *R->Name));
		return;
	}
	KinLive.Acc += 0.6f * KinKennelMul();
	if (KinLive.Acc > 8.f)
	{
		KinLive.Acc = 0.f;
		const float PerkB = UAerondelRules::Perk(GetPS(), FName(TEXT("Hunter")), FName(TEXT("kinXp")));
		R->Xp += FMath::RoundToInt(1.f + PerkB * 4.f);
		if (R->Level < 1000 && R->Xp >= NeedFor(R->Level))
		{
			R->Xp -= NeedFor(R->Level);
			R->Level++;
			R->Hp = MaxHp(R->Level);
			KinLive.Hp = R->Hp;
			Say(FString::Printf(TEXT("%s grows to level %d."), *R->Name, R->Level));
		}
	}
	AAerondelMob* T = CurrentTarget();
	if (!T || T->IsDead() || T->GetHp() <= 0) return;
	if (NowMs() < KinLive.NextMs) return;
	if (FMath::FRand() > 0.4f + R->Loyalty / 200.f) { KinLive.NextMs = NowMs() + 900; return; }
	KinLive.NextMs = NowMs() + 1100;
	Strike(T, 0.2f, -0.35f, TEXT("kin-snap"));
	if (FMath::FRand() < 0.35f && !IsBossy(T)) T->Stun(2);
}

int32 UVenturesFeature::KinSoak(int32 Amount)
{
	if (!bKinOut || !KinLive.bOn || Amount <= 0) return Amount;
	if (FMath::FRand() >= 0.25f) return Amount;
	const int32 Soak = FMath::Min(KinLive.Hp, FMath::Max(1, FMath::RoundToInt(Amount * 0.25f)));
	KinLive.Hp -= Soak;
	if (FKinRec* R = KinRecOf(KinLive.Id)) R->Hp = KinLive.Hp;
	return Amount - Soak;
}

void UVenturesFeature::TabKin(FGPanelModel& M)
{
	M.Title = FString::Printf(TEXT("Kinbeasts · Hunter %d"), SkillLevel(TEXT("Hunter")));
	M.Lines.Add(TEXT("Weaken a catchable wild creature below a third of its health and throw a vessel. Hunter decides the catch and how fast they train. One Kinbeast or one Ashkin may be out — never both. A companion can still walk with you."));
	if (AshOut()) M.Lines.Add(TEXT("An Ashkin is out — sending a Kinbeast will recall it."));
	if (AAerondelMob* T = CurrentTarget())
	{
		const UMobDef* D = T->Definition.Get();
		const FString Name = D ? D->DisplayName.ToString() : FString();
		if (const FSpecies* Sp = ByType(Name))
		{
			const FString Vessel = KinBestVessel(Sp->Vessel);
			if (!Vessel.IsEmpty() && T->GetMaxHp() > 0 && (float)T->GetHp() / (float)T->GetMaxHp() <= 0.35f && !T->IsDead())
			{
				const int32 Pct = FMath::RoundToInt(KinChance(T, Vessel) * 100.f);
				VAct20(M, FString::Printf(TEXT("Bind this %s (%d%% with %s)"), *Name, Pct, *Vessel), [this] { KinThrow(); });
			}
			else M.Lines.Add(FString::Printf(TEXT("Target: %s. Weaken it below a third of its health and hold a vessel."), *Name));
		}
	}
	if (!KinRoster.Num()) M.Lines.Add(TEXT("None bound yet."));
	for (const FKinRec& R : KinRoster)
	{
		const FSpecies* Sp = FindSp(R.Species);
		const bool bOn = R.Id == KinActive;
		const bool bOut = bKinOut && KinLive.Id == R.Id;
		M.Lines.Add(FString::Printf(TEXT("%s — %s · lv %d · loyalty %d%s%s"),
			*R.Name, Sp ? Sp->Name : TEXT("?"), R.Level, R.Loyalty,
			bOn ? TEXT(" · active") : TEXT(""),
			bOut ? TEXT(" · out") : TEXT("")));
		if (!bOn) VAct20(M, FString::Printf(TEXT("Set %s active"), *R.Name), [this, Id = R.Id] { KinSetActive(Id); });
		VAct20(M, bOut ? FString::Printf(TEXT("Recall %s"), *R.Name) : FString::Printf(TEXT("Send %s out"), *R.Name), [this, Id = R.Id] { KinSend(Id); });
		VAct20(M, FString::Printf(TEXT("Feed %s"), *R.Name), [this, Id = R.Id] { KinFeed(Id); });
	}
	TArray<FString> SeenN;
	for (const FString& Id : KinSeen) if (const FSpecies* Sp = FindSp(Id)) SeenN.Add(Sp->Name);
	M.Lines.Add(FString::Printf(TEXT("Seen in the field: %s."), SeenN.Num() ? *FString::Join(SeenN, TEXT(", ")) : TEXT("none yet")));
	M.Lines.Add(FString::Printf(TEXT("Vessels: Bind %d · Sturdy %d · Fine %d. The Hearthhold kennel and Greenstead care speed training a little."),
		CountItem(TEXT("Bind vessel")), CountItem(TEXT("Sturdy vessel")), CountItem(TEXT("Fine vessel"))));
	for (const FVessel& V : Vessels)
	{
		const int32 Price = UAerondelRules::BuyPrice(GetPS(), V.Price);
		VAct20(M, FString::Printf(TEXT("Buy %s (%d coins)"), V.Name, Price), [this, N = FString(V.Name)] { KinBuy(N); }, SkillLevel(TEXT("Hunter")) >= V.Hun && Coins() >= Price);
	}
}
