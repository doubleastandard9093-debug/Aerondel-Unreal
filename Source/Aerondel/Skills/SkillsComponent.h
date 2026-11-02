#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SkillsComponent.generated.h"

class USkillDef;

USTRUCT(BlueprintType)
struct FSkillProgress
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) TObjectPtr<USkillDef> Skill = nullptr;
	UPROPERTY(BlueprintReadOnly) float Xp = 0.f;
	UPROPERTY(BlueprintReadOnly) int32 Level = 1;
};

UCLASS(ClassGroup=(Aerondel), meta=(BlueprintSpawnableComponent))
class AERONDEL_API USkillsComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	USkillsComponent();
	virtual void InitializeComponent() override;

	UFUNCTION(BlueprintCallable, Category="Skills") void AddXp(USkillDef* Skill, float Amount);
	UFUNCTION(BlueprintPure, Category="Skills") int32 GetLevel(const USkillDef* Skill) const;
	UFUNCTION(BlueprintPure, Category="Skills") int32 GetLevelById(FName SkillId) const;
	UFUNCTION(BlueprintPure, Category="Skills") float GetXp(const USkillDef* Skill) const;
	UFUNCTION(BlueprintPure, Category="Skills") int32 TotalLevel() const;
	UFUNCTION(BlueprintPure, Category="Skills") int32 CombatLevel() const;
	UFUNCTION(BlueprintPure, Category="Skills") const TArray<FSkillProgress>& GetAll() const { return Entries; }

	/** Save support: raw xp by skill id. */
	void SetXpRaw(FName SkillId, float Xp);
	void ResetToDatabase();

private:
	FSkillProgress* Find(const USkillDef* Skill);
	const FSkillProgress* Find(const USkillDef* Skill) const;
	UPROPERTY() TArray<FSkillProgress> Entries;
};
