#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Player/GielinorGamepadComponent.h"
#include "Features/TrueAimFeature.h"
#include "GielinorHUD.generated.h"

class UGielinorHudWidget;

/**
 * Spawns the UMG HUD if HudWidgetClass is set (Project: BP_GielinorHUD). Until real widgets exist it draws a
 * Canvas fallback (hover label, HP, inventory list, skills, quest step, messages, dialogue) so the slice is
 * playable straight after the C++ build. Desktop only: VR uses the wrist WidgetComponent.
 */
UCLASS()
class GIELINOR_API AGielinorHUD : public AHUD
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category="Gielinor") TSubclassOf<UGielinorHudWidget> HudWidgetClass;
	virtual void BeginPlay() override;
	virtual void DrawHUD() override;
	/** Opens (or, if already open, closes) a pad menu; mouse/keyboard can call it too. */
	UFUNCTION(BlueprintCallable, Category="Gielinor") void TogglePadMenu(EGielinorPadMenu Menu);
	UFUNCTION(BlueprintCallable, Category="Gielinor") void ClosePadMenu();
	UFUNCTION(BlueprintPure, Category="Gielinor") bool IsPadMenuOpen() const { return bMenuOpen; }
	/** Opens a C++ UMG feature panel (see FGielinorPanels) with an optional context actor (the NPC / shop counter). */
	UFUNCTION(BlueprintCallable, Category="Gielinor") void OpenPanel(FName Panel, AActor* Context);
	UFUNCTION(BlueprintCallable, Category="Gielinor") void ClosePanel();
private:
	void BindPad();
	void DrawPadMenu(class AGielinorPlayerState* PS);
	TWeakObjectPtr<UGielinorGamepadComponent> BoundPad;
	bool bMenuOpen = false; EGielinorPadMenu OpenMenu = EGielinorPadMenu::Inventory;
	UFUNCTION() void OnMsg(const FText& T);
	UFUNCTION() void OnService(FName Service, AActor* Source);
	UFUNCTION() void OnAimStrike(EAimShape Shape, FVector Origin, float YawDeg, int32 Hits);
	UPROPERTY() TObjectPtr<class UGielinorPanelWidget> Panel;
	bool bAimBound = false;
	UFUNCTION() void OnDlg(const FText& Speaker, const TArray<FText>& Lines);
	UPROPERTY() TObjectPtr<UGielinorHudWidget> Widget;
	TArray<FText> Messages;
	FText DialogueSpeaker; TArray<FText> DialogueLines; float DialogueUntil = 0.f;
};
