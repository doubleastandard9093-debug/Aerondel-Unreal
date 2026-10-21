#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "GielinorSaveSubsystem.generated.h"

class AGielinorPlayerState;

UCLASS()
class GIELINOR_API UGielinorSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	static constexpr const TCHAR* SlotName = TEXT("Gielinor_Slot0");

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category="Gielinor|Save") bool SaveNow();
	UFUNCTION(BlueprintCallable, Category="Gielinor|Save") void DeleteSave();
	bool Save(AGielinorPlayerState* PS);
	/** Returns false if there is no save (caller gives the starter kit). */
	bool LoadInto(AGielinorPlayerState* PS);

	UPROPERTY(EditAnywhere, Category="Gielinor|Save") float AutosaveSeconds = 60.f;
private:
	bool AutoTick(float);
	FTSTicker::FDelegateHandle Handle;
};
