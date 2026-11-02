#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "AerondelSaveSubsystem.generated.h"

class AAerondelPlayerState;

UCLASS()
class AERONDEL_API UAerondelSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	static constexpr const TCHAR* SlotName = TEXT("Aerondel_Slot0");
	/** Pre-rename slot: loaded when no Aerondel slot exists yet (the next save writes the new slot; the old file is left alone). */
	static constexpr const TCHAR* LegacySlotName = TEXT("Gielinor_Slot0");

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category="Aerondel|Save") bool SaveNow();
	UFUNCTION(BlueprintCallable, Category="Aerondel|Save") void DeleteSave();
	bool Save(AAerondelPlayerState* PS);
	/** Returns false if there is no save (caller gives the starter kit). */
	bool LoadInto(AAerondelPlayerState* PS);

	UPROPERTY(EditAnywhere, Category="Aerondel|Save") float AutosaveSeconds = 60.f;
private:
	bool AutoTick(float);
	FTSTicker::FDelegateHandle Handle;
};
