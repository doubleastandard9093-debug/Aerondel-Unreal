#pragma once
#include "CoreMinimal.h"
#include "Player/AerondelGamepadComponent.h"
#include "Blueprint/UserWidget.h"
#include "AerondelWidgetBase.generated.h"

class AAerondelPlayerState; class UAerondelEvents;

/**
 * C++ bases for UMG. Make a Widget Blueprint that inherits one of these, lay it out in the designer,
 * and implement Refresh. The same widgets render on screen (desktop) or on the wrist panel (VR WidgetComponent).
 */
UCLASS(Abstract)
class AERONDEL_API UAerondelWidgetBase : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category="Aerondel") AAerondelPlayerState* GetAerondelPlayerState() const;
	UFUNCTION(BlueprintPure, Category="Aerondel") UAerondelEvents* GetEvents() const;
	/** Called on construct and whenever the relevant data changes. */
	UFUNCTION(BlueprintImplementableEvent, Category="Aerondel") void Refresh();
protected:
	virtual void NativeConstruct() override;
	virtual void BindSources() {}
	UFUNCTION() void RequestRefresh() { Refresh(); }
};

UCLASS(Abstract)
class AERONDEL_API UAerondelHudWidget : public UAerondelWidgetBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category="Aerondel") FText GetHoverText() const;
	UFUNCTION(BlueprintPure, Category="Aerondel") float GetHealthPercent() const;
	UFUNCTION(BlueprintPure, Category="Aerondel") const TArray<FText>& GetRecentMessages() const { return Messages; }
	/** Gamepad menu buttons (Y context, View map, Start inventory / hold settings). bOpen false = close it. */
	UFUNCTION(BlueprintImplementableEvent, Category="Aerondel") void ShowPadMenu(EAerondelPadMenu Menu, bool bOpen);
	UFUNCTION(BlueprintImplementableEvent, Category="Aerondel") void ShowDialogue(const FText& Speaker, const TArray<FText>& Lines);
protected:
	virtual void BindSources() override;
	UFUNCTION() void OnMsg(const FText& T);
	UFUNCTION() void OnDlg(const FText& Speaker, const TArray<FText>& Lines) { ShowDialogue(Speaker, Lines); }
	UFUNCTION() void OnHp(int32 Cur, int32 Max, int32 Delta) { Refresh(); }
	TArray<FText> Messages;
};

UCLASS(Abstract)
class AERONDEL_API UAerondelInventoryWidget : public UAerondelWidgetBase
{
	GENERATED_BODY()
public:
	/** Click on a slot: equip gear, eat food, or deposit when the bank is open. */
	UFUNCTION(BlueprintCallable, Category="Aerondel") void UseSlot(int32 SlotIndex);
	UFUNCTION(BlueprintCallable, Category="Aerondel") void DropSlot(int32 SlotIndex);
protected:
	virtual void BindSources() override;
};

UCLASS(Abstract)
class AERONDEL_API UAerondelSkillsWidget : public UAerondelWidgetBase
{
	GENERATED_BODY()
public:
	/** 0..1 progress to next level. */
	UFUNCTION(BlueprintPure, Category="Aerondel") static float ProgressToNext(float Xp, int32 Level);
protected:
	virtual void BindSources() override;
	UFUNCTION() void OnXp(class USkillDef* Skill, float Amount, float Total, int32 Level) { Refresh(); }
};

UCLASS(Abstract)
class AERONDEL_API UAerondelQuestLogWidget : public UAerondelWidgetBase
{
	GENERATED_BODY()
protected:
	virtual void BindSources() override;
	UFUNCTION() void OnQuest(class UQuestDef* Quest) { Refresh(); }
};
