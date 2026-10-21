#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "NpcDef.generated.h"

UCLASS(BlueprintType)
class GIELINOR_API UNpcDef : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Npc") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Npc") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Npc") FLinearColor Tint = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Npc") TArray<FText> IdleLines;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("Npc"), GetFName()); }
};
