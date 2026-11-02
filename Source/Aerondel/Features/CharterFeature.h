#pragma once
#include "CoreMinimal.h"
#include "Features/AerondelFeature.h"
#include "CharterFeature.generated.h"

/** One stall line in Thistlewell (what is sold depends on the outcome). */
USTRUCT(BlueprintType)
struct FCharterStall
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FString Item;
	UPROPERTY(BlueprintReadOnly) int32 Count = 0;
	UPROPERTY(BlueprintReadOnly) int32 Cost = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCharterResolved, FName, Side);

/** Charter vs Commons: the frontier hamlet of Thistlewell. The Thistlewell Charter (merchants, factor Brannoch) and the Commons
 *  (elder Ysmay) both claim the same spring. Side with one, broker the Spring Accord (Persuade 40) or betray both (Lie 30).
 *  Permanent: it changes who lives there (level listens on OnResolved / IsNpcPresent), the stall and companion bond. */
UCLASS()
class AERONDEL_API UCharterFeature : public UAerondelFeature
{
	GENERATED_BODY()
public:
	static constexpr int32 DealNeed = 40;
	static constexpr int32 BetrayNeed = 30;
	virtual FString GetSaveKey() const override { return TEXT("charter"); }
	virtual void SaveState(FJsonObject& Out) const override;
	virtual void LoadState(const FJsonObject& In) override;
	virtual void ResetState() override { bMet = false; Side = NAME_None; RepCharter = RepCommons = 0; Bought = 0; }

	/** charter | commons | deal | betray. Fails if already decided or the Silver Tongue check fails. */
	UFUNCTION(BlueprintCallable, Category="Aerondel|Charter") bool Resolve(FName NewSide);
	UFUNCTION(BlueprintCallable, Category="Aerondel|Charter") void Meet() { bMet = true; }
	UFUNCTION(BlueprintPure, Category="Aerondel|Charter") FName GetSide() const { return Side; }
	UFUNCTION(BlueprintPure, Category="Aerondel|Charter") bool HasMet() const { return bMet; }
	UFUNCTION(BlueprintPure, Category="Aerondel|Charter") int32 GetRep(FName Faction) const { return Faction == FName(TEXT("commons")) ? RepCommons : RepCharter; }
	UFUNCTION(BlueprintPure, Category="Aerondel|Charter") TArray<FCharterStall> GetStall() const;
	UFUNCTION(BlueprintCallable, Category="Aerondel|Charter") bool Buy(int32 Index);
	/** brannoch | ysmay: false once that side has lost (or after a betrayal). */
	UFUNCTION(BlueprintPure, Category="Aerondel|Charter") bool IsNpcPresent(FName NpcId) const;
	/** Opening line for an NPC (brannoch / ysmay), matching the web script. */
	UFUNCTION(BlueprintPure, Category="Aerondel|Charter") FString Greeting(FName NpcId) const;
	UPROPERTY(BlueprintAssignable, Category="Aerondel|Charter") FOnCharterResolved OnResolved;
private:
	bool bMet = false;
	FName Side;
	int32 RepCharter = 0, RepCommons = 0, Bought = 0;
};
