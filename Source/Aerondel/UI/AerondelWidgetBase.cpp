#include "UI/AerondelWidgetBase.h"
#include "Core/AerondelPlayerState.h"
#include "Core/AerondelEvents.h"
#include "Player/AerondelPlayerController.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/EquipmentComponent.h"
#include "Bank/BankComponent.h"
#include "Combat/HealthComponent.h"
#include "Interaction/GroundItem.h"
#include "Data/ItemDef.h"
#include "Data/SkillDef.h"
#include "GameFramework/Pawn.h"

AAerondelPlayerState* UAerondelWidgetBase::GetAerondelPlayerState() const
{
	APlayerController* PC = GetOwningPlayer();
	return PC ? PC->GetPlayerState<AAerondelPlayerState>() : nullptr;
}

UAerondelEvents* UAerondelWidgetBase::GetEvents() const { return UAerondelEvents::Get(this); }

void UAerondelWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	BindSources();
	Refresh();
}

// ---- HUD ----
FText UAerondelHudWidget::GetHoverText() const
{
	const AAerondelPlayerController* PC = Cast<AAerondelPlayerController>(GetOwningPlayer());
	return PC ? PC->GetHoverText() : FText::GetEmpty();
}

float UAerondelHudWidget::GetHealthPercent() const
{
	const AAerondelPlayerState* PS = GetAerondelPlayerState();
	return PS ? float(PS->Health->GetCurrent()) / FMath::Max(1, PS->Health->GetMax()) : 1.f;
}

void UAerondelHudWidget::BindSources()
{
	if (UAerondelEvents* Ev = GetEvents())
	{
		Ev->OnMessage.AddDynamic(this, &UAerondelHudWidget::OnMsg);
		Ev->OnDialogue.AddDynamic(this, &UAerondelHudWidget::OnDlg);
	}
	if (AAerondelPlayerState* PS = GetAerondelPlayerState()) PS->Health->OnHealthChanged.AddDynamic(this, &UAerondelHudWidget::OnHp);
}

void UAerondelHudWidget::OnMsg(const FText& T)
{
	Messages.Add(T);
	if (Messages.Num() > 6) Messages.RemoveAt(0);
	Refresh();
}

// ---- Inventory ----
void UAerondelInventoryWidget::BindSources()
{
	if (AAerondelPlayerState* PS = GetAerondelPlayerState())
	{
		PS->Inventory->OnChanged.AddDynamic(this, &UAerondelInventoryWidget::RequestRefresh);
		PS->Equipment->OnChanged.AddDynamic(this, &UAerondelInventoryWidget::RequestRefresh);
	}
}

void UAerondelInventoryWidget::UseSlot(int32 SlotIndex)
{
	AAerondelPlayerState* PS = GetAerondelPlayerState();
	if (!PS) return;
	const FItemStack S = PS->Inventory->GetSlot(SlotIndex);
	if (S.IsEmpty()) return;
	if (PS->Bank->IsOpen()) { PS->Bank->Deposit(PS->Inventory, S.Item, S.Item->bStackable ? S.Count : 1); return; }
	if (S.Item->EquipSlot != EEquipSlot::None) { PS->Equipment->EquipFromInventory(SlotIndex); return; }
	if (S.Item->HealAmount > 0) { PS->Inventory->RemoveFromSlot(SlotIndex, 1); PS->Health->Heal(S.Item->HealAmount); return; }
}

void UAerondelInventoryWidget::DropSlot(int32 SlotIndex)
{
	AAerondelPlayerState* PS = GetAerondelPlayerState();
	APawn* Pawn = GetOwningPlayerPawn();
	if (!PS || !Pawn) return;
	const FItemStack S = PS->Inventory->GetSlot(SlotIndex);
	if (S.IsEmpty()) return;
	PS->Inventory->RemoveFromSlot(SlotIndex, S.Count);
	AGroundItem::Spawn(Pawn->GetWorld(), S.Item, S.Count, Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 60.f - FVector(0, 0, 80));
}

// ---- Skills ----
void UAerondelSkillsWidget::BindSources()
{
	if (UAerondelEvents* Ev = GetEvents()) Ev->OnXpGained.AddDynamic(this, &UAerondelSkillsWidget::OnXp);
}

float UAerondelSkillsWidget::ProgressToNext(float Xp, int32 Level)
{
	const float A = (float)UXpCurve::XpForLevel(Level);
	const float B = (float)UXpCurve::XpForLevel(Level + 1);
	return B > A ? FMath::Clamp((Xp - A) / (B - A), 0.f, 1.f) : 1.f;
}

// ---- Quest log ----
void UAerondelQuestLogWidget::BindSources()
{
	if (UAerondelEvents* Ev = GetEvents()) Ev->OnQuestChanged.AddDynamic(this, &UAerondelQuestLogWidget::OnQuest);
}
