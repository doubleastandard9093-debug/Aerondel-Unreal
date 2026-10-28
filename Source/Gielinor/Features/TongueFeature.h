#pragma once
#include "CoreMinimal.h"
#include "Features/GielinorFeature.h"
#include "TongueFeature.generated.h"

UENUM(BlueprintType)
enum class ETongueCheck : uint8 { Persuade, Lie, Intimidate };

/** An open dialogue check for UI. */
USTRUCT(BlueprintType)
struct FTongueOffer
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName QuestId;
	UPROPERTY(BlueprintReadOnly) ETongueCheck Kind = ETongueCheck::Persuade;
	UPROPERTY(BlueprintReadOnly) int32 Need = 0;
	UPROPERTY(BlueprintReadOnly) int32 Score = 0;
	UPROPERTY(BlueprintReadOnly) FString Label;
};

/** Silver Tongue: Persuade (Prayer, kind), Lie (Thieving, cunning), Intimidate (Strength, firm). No dice:
 *  score = level/2 + 2 per matching wheel answer (max 30) + 10 with a bond-6 follower. 15 quest givers carry a one-time check. */
UCLASS()
class GIELINOR_API UTongueFeature : public UGielinorFeature
{
	GENERATED_BODY()
public:
	virtual FString GetSaveKey() const override { return TEXT("tongue"); }
	virtual void SaveState(FJsonObject& Out) const override;
	virtual void LoadState(const FJsonObject& In) override;
	virtual void ResetState() override { Done.Reset(); Passes = 0; Fails = 0; }

	UFUNCTION(BlueprintPure, Category="Gielinor|Tongue") int32 Score(ETongueCheck Kind) const;
	/** The still-open quest check for this NPC (quest started or done, check not spent). */
	UFUNCTION(BlueprintPure, Category="Gielinor|Tongue") bool OpenCheck(FName NpcId, FTongueOffer& Out) const;
	/** Resolve it: pass pays need*12 coins and 150 XP in the check's skill. Either way the check is spent. */
	UFUNCTION(BlueprintCallable, Category="Gielinor|Tongue") bool Resolve(const FTongueOffer& Offer);
	/** Generic check for other systems (key may be empty for repeatable checks). */
	UFUNCTION(BlueprintCallable, Category="Gielinor|Tongue") bool Check(const FString& Key, ETongueCheck Kind, int32 Need);
	UFUNCTION(BlueprintPure, Category="Gielinor|Tongue") int32 GetPasses() const { return Passes; }
	UFUNCTION(BlueprintPure, Category="Gielinor|Tongue") int32 GetFails() const { return Fails; }
	static const TCHAR* CheckName(ETongueCheck K);
private:
	TMap<FString, bool> Done;
	int32 Passes = 0, Fails = 0;
};
