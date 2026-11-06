#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Player/AerondelGamepadComponent.h"
#include "Features/TrueAimFeature.h"
#include "AerondelHUD.generated.h"

class UAerondelHudWidget;

/**
 * Spawns the UMG HUD if HudWidgetClass is set (Project: BP_AerondelHUD). Until real widgets exist it draws a
 * Canvas fallback (hover label, HP, inventory list, skills, quest step, messages, dialogue) so the slice is
 * playable straight after the C++ build. Desktop only: VR uses the wrist WidgetComponent.
 */
UCLASS()
class AERONDEL_API AAerondelHUD : public AHUD
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, Category="Aerondel") TSubclassOf<UAerondelHudWidget> HudWidgetClass;
	virtual void BeginPlay() override;
	virtual void DrawHUD() override;
	/** Opens (or, if already open, closes) a pad menu; mouse/keyboard can call it too. */
	UFUNCTION(BlueprintCallable, Category="Aerondel") void TogglePadMenu(EAerondelPadMenu Menu);
	UFUNCTION(BlueprintCallable, Category="Aerondel") void ClosePadMenu();
	UFUNCTION(BlueprintPure, Category="Aerondel") bool IsPadMenuOpen() const { return bMenuOpen; }
	/** Opens a C++ UMG feature panel (see FAerondelPanels) with an optional context actor (the NPC / shop counter). */
	UFUNCTION(BlueprintCallable, Category="Aerondel") void OpenPanel(FName Panel, AActor* Context);
	UFUNCTION(BlueprintCallable, Category="Aerondel") void ClosePanel();
	/** Name of the open panel, or None. */
	FName GetOpenPanel() const;
	/** Opens Settings, or closes it if it is already open (F10 / Esc / the on-screen Settings button). */
	void ToggleSettings();
	/** Left-click at a viewport position: true (and handled) when it hit the on-screen Settings button. */
	bool HandleHudClick(const FVector2D& Pos);
private:
	void BindPad();
	void DrawPadMenu(class AAerondelPlayerState* PS);
	TWeakObjectPtr<UAerondelGamepadComponent> BoundPad;
	bool bMenuOpen = false; EAerondelPadMenu OpenMenu = EAerondelPadMenu::Inventory;
	UFUNCTION() void OnMsg(const FText& T);
	UFUNCTION() void OnService(FName Service, AActor* Source);
	UFUNCTION() void OnAimStrike(EAimShape Shape, FVector Origin, float YawDeg, int32 Hits);
	UPROPERTY() TObjectPtr<class UAerondelPanelWidget> Panel;
	bool bAimBound = false;
	FVector2D SettingsBtnMin = FVector2D::ZeroVector, SettingsBtnMax = FVector2D::ZeroVector;
	UFUNCTION() void OnDlg(const FText& Speaker, const TArray<FText>& Lines);
	UPROPERTY() TObjectPtr<UAerondelHudWidget> Widget;
	TArray<FText> Messages;
	FText DialogueSpeaker; TArray<FText> DialogueLines; float DialogueUntil = 0.f;
};
