#include "Data/GameDatabase.h"
#include "Data/ItemDef.h"
#include "Data/SkillDef.h"
#include "Data/QuestDef.h"
#include "Gielinor.h"

template<typename T>
static T* FindById(const TArray<TObjectPtr<T>>& Arr, FName Id)
{
	for (T* E : Arr) { if (E && E->Id == Id) return E; }
	return nullptr;
}

UItemDef* UGameDatabase::FindItem(FName Id) const { return FindById(Items, Id); }
USkillDef* UGameDatabase::FindSkill(FName Id) const { return FindById(Skills, Id); }
UQuestDef* UGameDatabase::FindQuest(FName Id) const { return FindById(Quests, Id); }

UGameDatabase* UGameDatabase::Get()
{
	if (UGielinorAssetManager* AM = Cast<UGielinorAssetManager>(UAssetManager::GetIfInitialized()))
	{
		return AM->Database ? AM->Database.Get() : AM->LoadDatabase();
	}
	// Fallback when the custom manager is not configured.
	return LoadObject<UGameDatabase>(nullptr, TEXT("/Game/Gielinor/Data/DA_GameDatabase.DA_GameDatabase"));
}

void UGielinorAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();
	LoadDatabase();
}

UGameDatabase* UGielinorAssetManager::LoadDatabase()
{
	TArray<FPrimaryAssetId> Ids;
	GetPrimaryAssetIdList(FPrimaryAssetType(TEXT("GameDatabase")), Ids);
	if (Ids.Num() > 0)
	{
		const FSoftObjectPath Path = GetPrimaryAssetPath(Ids[0]);
		Database = Cast<UGameDatabase>(Path.TryLoad());
	}
	if (!Database)
	{
		UE_LOG(LogGielinor, Warning, TEXT("No GameDatabase found. Run Gielinor > Generate Starter Data."));
	}
	return Database;
}
