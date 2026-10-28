#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "GielinorSaveGame.generated.h"

USTRUCT() struct FSavedSkill { GENERATED_BODY() UPROPERTY() FName Id; UPROPERTY() float Xp = 0.f; };
USTRUCT() struct FSavedSlot { GENERATED_BODY() UPROPERTY() int32 Index = 0; UPROPERTY() FName ItemId; UPROPERTY() int32 Count = 0; };
USTRUCT() struct FSavedEquip { GENERATED_BODY() UPROPERTY() uint8 Slot = 0; UPROPERTY() FName ItemId; };
USTRUCT() struct FSavedEffect { GENERATED_BODY() UPROPERTY() FName Id; UPROPERTY() int32 TicksLeft = 0; UPROPERTY() TMap<FName, int32> Boost; UPROPERTY() TMap<FName, float> Fx; };
USTRUCT() struct FSavedQuest { GENERATED_BODY() UPROPERTY() FName Id; UPROPERTY() uint8 State = 0; UPROPERTY() int32 Step = 0; UPROPERTY() int32 Counter = 0; };

/** Content is referenced by stable string ids (not asset paths) so renames/moves don't break saves. */
UCLASS()
class GIELINOR_API UGielinorSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	/** 1 = original slice (HP x1). 2 = WebGPU parity (HP x10, stats, toolbelt, quest extras). 3 = feature blocks (web v18 systems). Missing fields keep defaults. */
	static constexpr int32 CurrentVersion = 3;
	UPROPERTY() int32 Version = 1;
	UPROPERTY() FVector Location = FVector::ZeroVector;
	UPROPERTY() int32 Hitpoints = 10;
	UPROPERTY() TArray<FSavedSkill> Skills;
	UPROPERTY() TArray<FSavedSlot> Inventory;
	UPROPERTY() TArray<FSavedSlot> Bank;
	UPROPERTY() TArray<FSavedEquip> Equipment;
	UPROPERTY() TArray<FSavedQuest> Quests;

	// ---- v2 ----
	UPROPERTY() float PrayerPoints = -1.f;
	UPROPERTY() TArray<FName> ActivePrayers;
	UPROPERTY() TArray<FSavedEffect> Effects;
	UPROPERTY() int32 PoisonDamage = 0;
	UPROPERTY() int32 PoisonNext = 0;
	UPROPERTY() int32 PoisonImmuneTicks = 0;
	UPROPERTY() float RunEnergy = 100.f;
	UPROPERTY() bool bRunning = false;
	UPROPERTY() float Adrenaline = 0.f;
	UPROPERTY() TArray<FName> Toolbelt;
	UPROPERTY() int32 AmmoCount = 0;
	UPROPERTY() int32 AttackStyle = 0;
	UPROPERTY() FName AutocastSpell;
	UPROPERTY() bool bAutoRetaliate = true;
	UPROPERTY() FName AgilityCourse;
	UPROPERTY() int32 AgilityNext = 0;
	UPROPERTY() FName BountyId;
	UPROPERTY() int32 BountyKills = 0;
	UPROPERTY() TArray<int32> ClaimedMilestones;
	UPROPERTY() bool bQuestCapeLegacy = false;

	// ---- v3 ----
	/** One JSON block per UGielinorFeature, keyed by GetSaveKey() (same shape as the web save fields). */
	UPROPERTY() TMap<FString, FString> Features;
};
