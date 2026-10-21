#include "Save/GielinorSaveSubsystem.h"
#include "Save/GielinorSaveGame.h"
#include "Core/GielinorPlayerState.h"
#include "Skills/SkillsComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/EquipmentComponent.h"
#include "Bank/BankComponent.h"
#include "Quests/QuestComponent.h"
#include "Combat/HealthComponent.h"
#include "Data/GameDatabase.h"
#include "Data/ItemDef.h"
#include "Data/SkillDef.h"
#include "Data/QuestDef.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "Engine/GameInstance.h"
#include "Gielinor.h"

void UGielinorSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Handle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UGielinorSaveSubsystem::AutoTick), AutosaveSeconds);
}

void UGielinorSaveSubsystem::Deinitialize()
{
	SaveNow();
	FTSTicker::GetCoreTicker().RemoveTicker(Handle);
	Super::Deinitialize();
}

bool UGielinorSaveSubsystem::AutoTick(float) { SaveNow(); return true; }

bool UGielinorSaveSubsystem::SaveNow()
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
	return Save(PC ? PC->GetPlayerState<AGielinorPlayerState>() : nullptr);
}

void UGielinorSaveSubsystem::DeleteSave() { UGameplayStatics::DeleteGameInSlot(SlotName, 0); }

static void SaveContainer(const UInventoryComponent* C, TArray<FSavedSlot>& Out)
{
	const TArray<FItemStack>& Slots = C->GetSlots();
	for (int32 i = 0; i < Slots.Num(); ++i)
		if (!Slots[i].IsEmpty()) { FSavedSlot& D = Out.AddDefaulted_GetRef(); D.Index = i; D.ItemId = Slots[i].Item->Id; D.Count = Slots[i].Count; }
}

static void LoadContainer(UInventoryComponent* C, const TArray<FSavedSlot>& In, const UGameDatabase* Db)
{
	C->ClearAll();
	for (const FSavedSlot& S : In) if (UItemDef* I = Db->FindItem(S.ItemId)) C->SetSlotRaw(S.Index, I, S.Count);
	C->OnChanged.Broadcast();
}

bool UGielinorSaveSubsystem::Save(AGielinorPlayerState* PS)
{
	if (!PS) return false;
	UGielinorSaveGame* S = Cast<UGielinorSaveGame>(UGameplayStatics::CreateSaveGameObject(UGielinorSaveGame::StaticClass()));
	if (APawn* P = PS->GetPawn()) S->Location = P->GetActorLocation();
	S->Hitpoints = PS->Health->GetCurrent();
	for (const FSkillProgress& P : PS->Skills->GetAll()) if (P.Skill) { FSavedSkill& D = S->Skills.AddDefaulted_GetRef(); D.Id = P.Skill->Id; D.Xp = P.Xp; }
	SaveContainer(PS->Inventory, S->Inventory);
	SaveContainer(PS->Bank, S->Bank);
	for (const auto& E : PS->Equipment->GetAll()) if (E.Value) { FSavedEquip& D = S->Equipment.AddDefaulted_GetRef(); D.Slot = (uint8)E.Key; D.ItemId = E.Value->Id; }
	for (const FQuestProgress& Q : PS->Quests->GetAll()) if (Q.Quest) { FSavedQuest& D = S->Quests.AddDefaulted_GetRef(); D.Id = Q.Quest->Id; D.State = (uint8)Q.State; D.Step = Q.Step; D.Counter = Q.Counter; }
	return UGameplayStatics::SaveGameToSlot(S, SlotName, 0);
}

bool UGielinorSaveSubsystem::LoadInto(AGielinorPlayerState* PS)
{
	const UGameDatabase* Db = UGameDatabase::Get();
	if (!PS || !Db || !UGameplayStatics::DoesSaveGameExist(SlotName, 0)) return false;
	UGielinorSaveGame* S = Cast<UGielinorSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!S) return false;
	for (const FSavedSkill& K : S->Skills) PS->Skills->SetXpRaw(K.Id, K.Xp);
	LoadContainer(PS->Inventory, S->Inventory, Db);
	LoadContainer(PS->Bank, S->Bank, Db);
	PS->Equipment->ClearAll();
	for (const FSavedEquip& E : S->Equipment) PS->Equipment->SetRaw((EEquipSlot)E.Slot, Db->FindItem(E.ItemId));
	for (const FSavedQuest& Q : S->Quests)
		if (UQuestDef* Definition = Db->FindQuest(Q.Id)) { FQuestProgress& P = PS->Quests->FindOrAdd(Definition); P.State = (EQuestState)Q.State; P.Step = Q.Step; P.Counter = Q.Counter; }
	UE_LOG(LogGielinor, Log, TEXT("Loaded save: %d skills, %d inventory slots."), S->Skills.Num(), S->Inventory.Num());
	return true;
}
