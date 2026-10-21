#pragma once
#include "CoreMinimal.h"
#include "GielinorTypes.generated.h"

class UItemDef;
class USkillDef;
class UNpcDef;
class UMobDef;

UENUM(BlueprintType)
enum class EToolType : uint8 { None, Axe, Pickaxe, FishingNet, Hammer, Tinderbox };

UENUM(BlueprintType)
enum class EEquipSlot : uint8 { None, Head, Body, Legs, Weapon, Shield, Hands, Feet };

UENUM(BlueprintType)
enum class EResourceKind : uint8 { Tree, Rock, FishingSpot };

UENUM(BlueprintType)
enum class EStationType : uint8 { Furnace, Anvil, Range };

UENUM(BlueprintType)
enum class EQuestStepType : uint8 { Talk, Gather, Deliver, Kill };

UENUM(BlueprintType)
enum class EQuestState : uint8 { NotStarted, InProgress, Completed };

USTRUCT(BlueprintType)
struct FItemStack
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UItemDef> Item = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Count = 0;
	bool IsEmpty() const { return Item == nullptr || Count <= 0; }
};

USTRUCT(BlueprintType)
struct FItemAmount
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UItemDef> Item = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Amount = 1;
};

USTRUCT(BlueprintType)
struct FDropEntry
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UItemDef> Item = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Min = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Max = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin=0, ClampMax=1)) float Chance = 1.f;
};

USTRUCT(BlueprintType)
struct FXpReward
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<USkillDef> Skill = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Xp = 0.f;
};

USTRUCT(BlueprintType)
struct FQuestStep
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EQuestStepType Type = EQuestStepType::Talk;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UNpcDef> Npc = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UItemDef> Item = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UMobDef> Mob = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Amount = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Description;
};
