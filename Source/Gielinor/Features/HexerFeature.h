#pragma once
#include "CoreMinimal.h"
#include "Features/GielinorFeature.h"
#include "HexerFeature.generated.h"

/** Mama Sorrel's hex runes (web main.js hexerTalk). Up to 10 per brew: each Rune/Clear essence + one grimy herb gives 3 Bog runes
 *  (6 Glyphcraft xp each, half that in Magic); essence + Bones gives 3 Bone runes (9 xp). Stateless: nothing saved. */
UCLASS()
class GIELINOR_API UHexerFeature : public UGielinorFeature
{
	GENERATED_BODY()
public:
	virtual FString GetSaveKey() const override { return TEXT("hexer"); }
	/** Returns Mama Sorrel's reply. */
	UFUNCTION(BlueprintCallable, Category="Gielinor|Hexer") FString BrewBog() { return Brew(false); }
	UFUNCTION(BlueprintCallable, Category="Gielinor|Hexer") FString BrewBone() { return Brew(true); }
	static constexpr const TCHAR* Greeting = TEXT("Ah, a seeker. I am Mama Sorrel. Hexcraft is old magic of bog and bone: rotting, binding, withering and leeching hexes. Open your Spellbook to cast them.");
private:
	FString Brew(bool bBone);
	TArray<FString> GrimyHerbsHeld() const;
};
