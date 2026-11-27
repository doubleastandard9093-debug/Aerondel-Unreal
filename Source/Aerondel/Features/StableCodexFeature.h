#pragma once
#include "CoreMinimal.h"
#include "Features/AerondelFeature.h"
#include "StableCodexFeature.generated.h"

USTRUCT(BlueprintType)
struct FCodexMount
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName Id;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) FString Kind;
	UPROPERTY(BlueprintReadOnly) FString How;
	UPROPERTY(BlueprintReadOnly) bool bOwned = false;
};

/** Stable Codex (WoW round): every mount in one list, Skyrook flights between the five roosts (straight arc, 34 m/s,
 *  26 m high, min 2.5 s), and a wardrobe that overrides the look of head/body/legs/shield/cape/weapon with any item seen. */
UCLASS()
class AERONDEL_API UStableCodexFeature : public UAerondelFeature
{
	GENERATED_BODY()
public:
	virtual FString GetSaveKey() const override { return TEXT("codex"); }
	virtual void SaveState(FJsonObject& Out) const override;
	virtual void LoadState(const FJsonObject& In) override;
	virtual void ResetState() override { bSkyrook = false; Flights = 0; Looks.Reset(); Seen.Reset(); }

	static const TArray<FString>& Roosts();
	UFUNCTION(BlueprintPure, Category="Aerondel|Codex") TArray<FCodexMount> GetMounts() const;
	/** Roostmaster Hale at Larkhaven. */
	UFUNCTION(BlueprintCallable, Category="Aerondel|Codex") void LearnSkyrook() { bSkyrook = true; }
	/** Validates a flight; the pawn plays the arc (FlightSeconds) and lands at the roost. */
	UFUNCTION(BlueprintCallable, Category="Aerondel|Codex") bool StartFlight(const FString& Roost);
	UFUNCTION(BlueprintPure, Category="Aerondel|Codex") static float FlightSeconds(float DistanceM) { return FMath::Max(2.5f, DistanceM / 34.f); }
	UFUNCTION(BlueprintCallable, Category="Aerondel|Codex") void MarkSeen(const FString& Item) { if (Seen.Num() < 400) Seen.AddUnique(Item); }
	/** Slot: head, body, legs, shield, cape, weapon. Empty item clears the look. Stats always come from real gear. */
	UFUNCTION(BlueprintCallable, Category="Aerondel|Codex") bool SetLook(const FString& Slot, const FString& Item);
	const TArray<FString>& GetSeen() const { return Seen; }
	UFUNCTION(BlueprintPure, Category="Aerondel|Codex") FString GetLook(const FString& Slot) const { const FString* L = Looks.Find(Slot); return L ? *L : FString(); }

private:
	bool bSkyrook = false;
	int32 Flights = 0;
	TMap<FString, FString> Looks;
	TArray<FString> Seen;
};
