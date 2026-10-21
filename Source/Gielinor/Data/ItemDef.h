#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "Data/GielinorTypes.h"
#include "ItemDef.generated.h"

UCLASS(BlueprintType)
class GIELINOR_API UItemDef : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") FName Id;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") FText Examine;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") FLinearColor Tint = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") TSoftObjectPtr<UTexture2D> Icon;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") TSoftObjectPtr<UStaticMesh> WorldMesh;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") bool bStackable = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") int32 Value = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item") int32 HealAmount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tool") EToolType ToolType = EToolType::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tool") int32 ToolTier = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment") EEquipSlot EquipSlot = EEquipSlot::None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment") int32 AttackBonus = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment") int32 StrengthBonus = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment") int32 DefenceBonus = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment") int32 RequiredLevel = 1;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("Item"), GetFName()); }
};
