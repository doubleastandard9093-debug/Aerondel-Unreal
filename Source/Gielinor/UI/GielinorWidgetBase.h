#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GielinorWidgetBase.generated.h"

class AGielinorPlayerState; class UGielinorEvents;

/**
 * C++ bases for UMG. Make a Widget Blueprint that inherits one of these, lay it out in the designer,
 * and implement Refresh. The same widgets render on screen (desktop) or on the wrist panel (VR WidgetComponent).
 */
UCLASS(Abstract)
class GIELINOR_API UGielinorWidgetBase : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category="Gielinor") AGielinorPlayerState* GetGielinorPlayerState() const;
	UFUNCTION(BlueprintPure, Category="Gielinor") UGielinorEvents* GetEvents() const;
	/** Called on construct and whenever the relevant data changes. */
	UFUNCTION(BlueprintImplementableEvent, Category="Gielinor") void Refresh();
protected:
	virtual void NativeConstruct() override;
	virtual void BindSources() {}
	UFUNCTION() void RequestRefresh() { Refresh(); }
};

UCLASS(Abstract)
class GIELINOR_API UGielinorHudWidget : public UGielinorWidgetBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category="Gielinor") FText GetHoverText() const;
	UFUNCTION(BlueprintPure, Category="Gielinor") float GetHealthPercent() const;
	UFUNCTION(BlueprintPure, Category="Gielinor") const TArray<FText>& GetRecentMessages() const { return Messages; }
	UFUNCTION(BlueprintImplementableEvent, Category="Gielinor") void ShowDialogue(const FText& Speaker, const TArray<FText>& Lines);
protected:
	virtual void BindSources() override;
	UFUNCTION() void OnMsg(const FText& T);
	UFUNCTION() void OnDlg(const FText& Speaker, const TArray<FText>& Lines) { ShowDialogue(Speaker, Lines); }
	UFUNCTION() void OnHp(int32 Cur, int32 Max, int32 Delta) { Refresh(); }
	TArray<FText> Messages;
};

UCLASS(Abstract)
class GIELINOR_API UGielinorInventoryWidget : public UGielinorWidgetBase
{
	GENERATED_BODY()
public:
	/** Click on a slot: equip gear, eat food, or deposit when the bank is open. */
	UFUNCTION(BlueprintCallable, Category="Gielinor") void UseSlot(int32 Slot);
	UFUNCTION(BlueprintCallable, Category="Gielinor") void DropSlot(int32 Slot);
protected:
	virtual void BindSources() override;
};

UCLASS(Abstract)
class GIELINOR_API UGielinorSkillsWidget : public UGielinorWidgetBase
{
	GENERATED_BODY()
public:
	/** 0..1 progress to next level. */
	UFUNCTION(BlueprintPure, Category="Gielinor") static float ProgressToNext(float Xp, int32 Level);
protected:
	virtual void BindSources() override;
	UFUNCTION() void OnXp(class USkillDef* Skill, float Amount, float Total, int32 Level) { Refresh(); }
};

UCLASS(Abstract)
class GIELINOR_API UGielinorQuestLogWidget : public UGielinorWidgetBase
{
	GENERATED_BODY()
protected:
	virtual void BindSources() override;
	UFUNCTION() void OnQuest(class UQuestDef* Quest) { Refresh(); }
};
