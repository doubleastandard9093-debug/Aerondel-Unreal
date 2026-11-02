#pragma once
#include "CoreMinimal.h"
#include "Features/AerondelFeature.h"
#include "CompanionsFeature.generated.h"

class UMobDef;

UENUM(BlueprintType)
enum class EWheelTone : uint8 { Kind, Firm, Cunning };

USTRUCT(BlueprintType)
struct FCompanionDef
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName Id;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) FString Role;
	UPROPERTY(BlueprintReadOnly) TArray<FString> Skills;
	UPROPERTY(BlueprintReadOnly) EWheelTone Likes = EWheelTone::Kind;
	/** Gift item names contain/end with one of these (web: giftRe). */
	UPROPERTY(BlueprintReadOnly) TArray<FString> GiftWords;
	UPROPERTY(BlueprintReadOnly) FString Style;
	UPROPERTY(BlueprintReadOnly) FVector2D WebPos = FVector2D::ZeroVector;
	/** Recruit task: kill N of RecruitMob, or bring N of RecruitItem with ReqSkill >= ReqLevel. */
	UPROPERTY(BlueprintReadOnly) FString RecruitMob;
	UPROPERTY(BlueprintReadOnly) FString RecruitItem;
	UPROPERTY(BlueprintReadOnly) int32 RecruitCount = 0;
	UPROPERTY(BlueprintReadOnly) FString ReqSkill;
	UPROPERTY(BlueprintReadOnly) int32 ReqLevel = 0;
	UPROPERTY(BlueprintReadOnly) FString Ask;
};

USTRUCT(BlueprintType)
struct FCompanionState
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) bool bRecruited = false;
	UPROPERTY(BlueprintReadOnly) bool bStarted = false;
	UPROPERTY(BlueprintReadOnly) int32 Bond = 0;
	UPROPERTY(BlueprintReadOnly) int64 GiftAt = 0;
	UPROPERTY(BlueprintReadOnly) int64 ToneAt = 0;
	/** Recruit-task progress (kills). */
	UPROPERTY(BlueprintReadOnly) int32 Task = 0;
	/** Crew skill XP per errand skill (see UErrandsFeature). */
	UPROPERTY(BlueprintReadOnly) TMap<FString, int32> Crew;
};

/** Phase 1 "Bonds & Choices": four Sworn Companions (bond 0-10, gifts, liked tones, perks, assists) and the Words of Choice alignment. */
UCLASS()
class AERONDEL_API UCompanionsFeature : public UAerondelFeature
{
	GENERATED_BODY()
public:
	static constexpr int32 BondMax = 10;
	static constexpr int64 GiftCooldownMs = 10 * 60 * 1000;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual FString GetSaveKey() const override { return TEXT("companions"); }
	virtual void SaveState(FJsonObject& Out) const override;
	virtual void LoadState(const FJsonObject& In) override;
	virtual void ResetState() override;
	virtual float XpBonus(FName SkillId) const override;

	static const TArray<FCompanionDef>& Defs();
	static const FCompanionDef* Def(FName Id);

	UFUNCTION(BlueprintPure, Category="Aerondel|Companions") FCompanionState GetState(FName Id) const { const FCompanionState* S = States.Find(Id); return S ? *S : FCompanionState(); }
	UFUNCTION(BlueprintPure, Category="Aerondel|Companions") FName GetActive() const { return Active; }
	UFUNCTION(BlueprintCallable, Category="Aerondel|Companions") void StartRecruit(FName Id);
	/** Hand in recruit items / finish a kill task. Returns true when recruited. */
	UFUNCTION(BlueprintCallable, Category="Aerondel|Companions") bool TryRecruit(FName Id);
	UFUNCTION(BlueprintCallable, Category="Aerondel|Companions") bool SetActive(FName Id);
	UFUNCTION(BlueprintCallable, Category="Aerondel|Companions") void SendHome() { Active = NAME_None; }
	/** Give one gift item (+1 bond, +2 for a liked gift; 10 min cooldown). */
	UFUNCTION(BlueprintCallable, Category="Aerondel|Companions") bool Gift(FName Id, const FString& ItemName);
	UFUNCTION(BlueprintCallable, Category="Aerondel|Companions") int32 AddBond(FName Id, int32 N);
	/** Words of Choice: record a picked tone (alignment) and let the follower react. */
	UFUNCTION(BlueprintCallable, Category="Aerondel|Choices") void RecordTone(EWheelTone Tone);
	UFUNCTION(BlueprintPure, Category="Aerondel|Choices") int32 GetAlignment(EWheelTone Tone) const { return Alignment[(int32)Tone]; }
	/** Assist: max hit and ticks between hits for the follower. */
	UFUNCTION(BlueprintPure, Category="Aerondel|Companions") int32 AssistMax() const;
	UFUNCTION(BlueprintPure, Category="Aerondel|Companions") int32 AssistEvery() const;
	UFUNCTION(BlueprintPure, Category="Aerondel|Companions") int32 ActiveBond() const;

	FCompanionState* Find(FName Id) { return States.Find(Id); }
	static FString ToneName(EWheelTone T) { return T == EWheelTone::Kind ? TEXT("kind") : T == EWheelTone::Firm ? TEXT("firm") : TEXT("cunning"); }

private:
	UFUNCTION() void HandleMobKilled(UMobDef* Mob);
	UPROPERTY() TMap<FName, FCompanionState> States;
	UPROPERTY() FName Active;
	int32 Alignment[3] = { 0, 0, 0 };
};
