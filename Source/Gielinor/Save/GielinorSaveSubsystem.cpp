#include "Save/GielinorSaveSubsystem.h"
#include "Save/GielinorSaveGame.h"
#include "Core/GielinorPlayerState.h"
#include "Skills/SkillsComponent.h"
#include "Inventory/InventoryComponent.h"
#include "Inventory/EquipmentComponent.h"
#include "Bank/BankComponent.h"
#include "Quests/QuestComponent.h"
#include "Combat/HealthComponent.h"
#include "Core/PlayerStatsComponent.h"
#include "Data/GameDatabase.h"
#include "Data/ItemDef.h"
#include "Data/SkillDef.h"
#include "Data/QuestDef.h"
#include "Data/TownRenames.h"
#include "Data/ContentRenames.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "Engine/GameInstance.h"
#include "Gielinor.h"
#include "Features/GielinorFeature.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

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

	S->Version = UGielinorSaveGame::CurrentVersion;
	const UPlayerStatsComponent* St = PS->Stats;
	S->PrayerPoints = St->PrayerPoints;
	S->ActivePrayers = St->ActivePrayers;
	for (const FActiveEffect& E : St->Effects) { FSavedEffect& D = S->Effects.AddDefaulted_GetRef(); D.Id = E.Id; D.TicksLeft = E.TicksLeft; D.Boost = E.Boost; D.Fx = E.Fx; }
	S->PoisonDamage = St->Poison.Damage; S->PoisonNext = St->Poison.Next; S->PoisonImmuneTicks = St->PoisonImmuneTicks;
	S->RunEnergy = St->RunEnergy; S->bRunning = St->bRunning; S->Adrenaline = St->Adrenaline;
	for (const UItemDef* T : St->Toolbelt) if (T) S->Toolbelt.Add(T->Id);
	S->AmmoCount = St->AmmoCount; S->AttackStyle = St->AttackStyle; S->AutocastSpell = St->AutocastSpell; S->bAutoRetaliate = St->bAutoRetaliate;
	S->AgilityCourse = St->AgilityCourse; S->AgilityNext = St->AgilityNext; S->BountyId = St->BountyId; S->BountyKills = St->BountyKills;
	S->ClaimedMilestones = PS->Quests->ClaimedMilestones;
	S->bQuestCapeLegacy = PS->Quests->bQuestCapeLegacy;
	for (UGielinorFeature* F : UGielinorFeature::AllFor(GetGameInstance()))
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		F->SaveState(*J);
		FString Text;
		const TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Text);
		if (FJsonSerializer::Serialize(J, W)) S->Features.Add(F->GetSaveKey(), Text);
	}
	return UGameplayStatics::SaveGameToSlot(S, SlotName, 0);
}

bool UGielinorSaveSubsystem::LoadInto(AGielinorPlayerState* PS)
{
	const UGameDatabase* Db = UGameDatabase::Get();
	if (!PS || !Db || !UGameplayStatics::DoesSaveGameExist(SlotName, 0)) return false;
	UGielinorSaveGame* S = Cast<UGielinorSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	if (!S) return false;
	const bool bV6 = S->Version >= 6; // v6: content renames (Data/ContentRenames.h)
	auto Mig = [bV6](FName Id) { return bV6 ? Id : GielinorContent::MigrateId(Id); };
	for (const FSavedSkill& K : S->Skills) PS->Skills->SetXpRaw(Mig(K.Id), S->Version >= 4 ? K.Xp : UXpCurve::ConvertXpV1(K.Xp)); // v4 re-curve
	LoadContainer(PS->Inventory, S->Inventory, Db);
	LoadContainer(PS->Bank, S->Bank, Db);
	PS->Equipment->ClearAll();
	for (const FSavedEquip& E : S->Equipment) PS->Equipment->SetRaw((EEquipSlot)E.Slot, Db->FindItem(E.ItemId));
	for (const FSavedQuest& Q : S->Quests)
		if (UQuestDef* Definition = Db->FindQuest(Q.Id)) { FQuestProgress& P = PS->Quests->FindOrAdd(Definition); P.State = (EQuestState)Q.State; P.Step = Q.Step; P.Counter = Q.Counter; }

	// ---- v2 fields (v1 saves: defaults + migration) ----
	UPlayerStatsComponent* St = PS->Stats;
	St->ResetRuntime();
	if (S->Version >= 2)
	{
		if (S->PrayerPoints >= 0.f) St->PrayerPoints = S->PrayerPoints;
		St->ActivePrayers.Reset(); for (const FName& P : S->ActivePrayers) St->ActivePrayers.Add(Mig(P));
		for (const FSavedEffect& E : S->Effects) { FActiveEffect& D = St->Effects.AddDefaulted_GetRef(); D.Id = Mig(E.Id); D.TicksLeft = E.TicksLeft; for (const TPair<FName, int32>& B : E.Boost) D.Boost.Add(Mig(B.Key), B.Value); D.Fx = E.Fx; }
		St->Poison.Damage = S->PoisonDamage; St->Poison.Next = S->PoisonNext; St->PoisonImmuneTicks = S->PoisonImmuneTicks;
		St->RunEnergy = S->RunEnergy; St->bRunning = S->bRunning; St->Adrenaline = S->Adrenaline;
		for (const FName& T : S->Toolbelt) St->AddTool(Db->FindItem(T));
		St->AmmoCount = S->AmmoCount; St->AttackStyle = S->AttackStyle; St->AutocastSpell = Mig(S->AutocastSpell); St->bAutoRetaliate = S->bAutoRetaliate;
		St->AgilityCourse = S->Version >= 5 ? S->AgilityCourse : GielinorTowns::MigrateId(S->AgilityCourse); St->AgilityNext = S->AgilityNext; St->BountyId = S->BountyId; St->BountyKills = S->BountyKills;
		PS->Quests->ClaimedMilestones = S->ClaimedMilestones;
		PS->Quests->bQuestCapeLegacy = S->bQuestCapeLegacy;
	}
	else
	{
		// v1 -> v2: starter tools onto the toolbelt, tools in the pack move to the belt, worn ammo counts as 1.
		for (UItemDef* T : Db->StarterTools) St->AddTool(T);
		for (const FItemStack& Slot : TArray<FItemStack>(PS->Inventory->GetSlots()))
			if (Slot.Item && Slot.Item->IsTool() && St->AddTool(Slot.Item)) PS->Inventory->Remove(Slot.Item, Slot.Count);
		if (PS->Equipment->GetEquipped(EEquipSlot::Ammo)) St->AmmoCount = 1;
		PS->Quests->bQuestCapeLegacy = PS->Quests->AllQuestsDone();
	}
	if (PS->Equipment->GetEquipped(EEquipSlot::Ammo) == nullptr) St->AmmoCount = 0;
	St->RefreshMaxHp(false);
	PS->Health->SetCurrentRaw(S->Version >= 2 ? S->Hitpoints : S->Hitpoints * 10); // v1 used the x1 HP scale

	// ---- v3 feature blocks (older saves: every feature starts fresh) ----
	for (UGielinorFeature* F : UGielinorFeature::AllFor(GetGameInstance()))
	{
		F->ResetState();
		if (const FString* Text = S->Features.Find(F->GetSaveKey()))
		{
			FString Migrated = S->Version >= 5 ? *Text : GielinorTowns::MigrateText(*Text); // v5: town renames
			if (!bV6) Migrated = GielinorContent::MigrateText(Migrated); // v6: content renames
			TSharedPtr<FJsonObject> J;
			const TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Migrated);
			if (FJsonSerializer::Deserialize(R, J) && J.IsValid()) F->LoadState(*J);
		}
	}

	UE_LOG(LogGielinor, Log, TEXT("Loaded save: %d skills, %d inventory slots."), S->Skills.Num(), S->Inventory.Num());
	return true;
}
