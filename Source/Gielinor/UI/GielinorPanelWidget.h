#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "UI/GielinorPanels.h"
#include "GielinorPanelWidget.generated.h"

class UGielinorPanelWidget; class UTextBlock; class UVerticalBox; class UScrollBox;

/** A button that remembers which panel action it runs (UButton's OnClicked has no payload). */
UCLASS()
class GIELINOR_API UGielinorPanelButton : public UButton
{
	GENERATED_BODY()
public:
	void Setup(UGielinorPanelWidget* InOwner, int32 InIndex);
private:
	UFUNCTION() void HandleClicked();
	TWeakObjectPtr<UGielinorPanelWidget> Owner;
	int32 Index = INDEX_NONE;
};

/**
 * Feature panel built entirely in C++ UMG (no Blueprint asset needed): title, wrapped body lines, a scrolling column of
 * buttons. Content comes from FGielinorPanels::Build; it rebuilds after every click, and live panels (cards, sea)
 * refresh 4x a second. Gamepad: focus lands on the first button so the d-pad / stick navigates, A clicks.
 */
UCLASS()
class GIELINOR_API UGielinorPanelWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void Open(FName InPanel, AActor* InContext);
	void Refresh();
	void RunAction(int32 Index);
	FName GetPanel() const { return Panel; }
	DECLARE_DELEGATE(FOnClosed);
	FOnClosed OnClosed;
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& Geo, float Dt) override;
private:
	void Populate(bool bForceButtons);
	FName Panel; TWeakObjectPtr<AActor> Context;
	FGPanelModel Model;
	float LiveTimer = 0.f;
	UPROPERTY() TObjectPtr<UTextBlock> TitleText;
	UPROPERTY() TObjectPtr<UTextBlock> BodyText;
	UPROPERTY() TObjectPtr<UScrollBox> ButtonBox;
	UPROPERTY() TArray<TObjectPtr<UGielinorPanelButton>> Buttons;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> ButtonLabels;
};

/** Ground flash for a True Aim strike: cone/line = a stretched quad ahead of the pawn, circle = a disc. Lives ~0.4 s. */
UCLASS()
class GIELINOR_API AGielinorAimMarker : public AActor
{
	GENERATED_BODY()
public:
	AGielinorAimMarker();
	void Setup(uint8 Shape, float YawDeg, int32 Hits);
	virtual void Tick(float Dt) override;
private:
	UPROPERTY() TObjectPtr<class UStaticMeshComponent> Mesh;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> Mat;
	float Age = 0.f;
};
