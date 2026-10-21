#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
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
private:
	UFUNCTION() void OnMsg(const FText& T);
	UFUNCTION() void OnDlg(const FText& Speaker, const TArray<FText>& Lines);
	UPROPERTY() TObjectPtr<UGielinorHudWidget> Widget;
	TArray<FText> Messages;
	FText DialogueSpeaker; TArray<FText> DialogueLines; float DialogueUntil = 0.f;
};
