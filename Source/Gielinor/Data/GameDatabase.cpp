#include "Data/GameDatabase.h"
#include "Data/ItemDef.h"
#include "Data/SkillDef.h"
#include "Data/QuestDef.h"
#include "Data/MobDef.h"
#include "Data/NpcDef.h"
#include "Data/ResourceNodeDef.h"
#include "Data/ContentDefs.h"
#include "Data/JsonContentLoader.h"
#include "Data/TownRenames.h"
#include "Gielinor.h"
#include "UObject/Package.h"

template<typename T>
static T* FindById(const TArray<TObjectPtr<T>>& Arr, FName Id)
{
	for (T* E : Arr) { if (E && E->Id == Id) return E; }
	return nullptr;
}

UItemDef* UGameDatabase::FindItem(FName Id) const { return FindById(Items, Id); }
USkillDef* UGameDatabase::FindSkill(FName Id) const { return FindById(Skills, Id); }
UQuestDef* UGameDatabase::FindQuest(FName Id) const { return FindById(Quests, Id); }
UMobDef* UGameDatabase::FindMob(FName Id) const { return FindById(Mobs, Id); }
UNpcDef* UGameDatabase::FindNpc(FName Id) const { return FindById(Npcs, Id); }
USpellDef* UGameDatabase::FindSpell(FName Id) const { return FindById(Spells, Id); }
UPrayerDef* UGameDatabase::FindPrayer(FName Id) const { return FindById(Prayers, Id); }
UAbilityDef* UGameDatabase::FindAbility(FName Id) const { return FindById(Abilities, Id); }
UShopDef* UGameDatabase::FindShop(FName Id) const { return FindById(Shops, Id); }
UAgilityCourseDef* UGameDatabase::FindAgilityCourse(FName Id) const
{
	if (UAgilityCourseDef* C = FindById(AgilityCourses, Id)) return C;
	return FindById(AgilityCourses, GielinorTowns::MigrateId(Id)); // pre-rename course ids (old saves, hand-placed actors)
}
UThieveTargetDef* UGameDatabase::FindThieveTarget(FName Id) const { return FindById(ThieveTargets, Id); }
URuneAltarDef* UGameDatabase::FindRuneAltar(FName Id) const { return FindById(RuneAltars, Id); }
UResourceNodeDef* UGameDatabase::FindResourceNode(FName Id) const { return FindById(ResourceNodes, Id); }

UPotionDef* UGameDatabase::FindPotionForItem(const UItemDef* Item) const
{
	if (!Item) return nullptr;
	for (UPotionDef* P : Potions) if (P && P->Item == Item) return P;
	return nullptr;
}

int32 UGameDatabase::TotalQuestPoints() const
{
	int32 N = 0;
	for (const UQuestDef* Q : Quests) if (Q) N += Q->QuestPoints;
	return N;
}

UGameDatabase* UGameDatabase::Get()
{
	if (UGielinorAssetManager* AM = Cast<UGielinorAssetManager>(UAssetManager::GetIfInitialized()))
	{
		return AM->Database ? AM->Database.Get() : AM->LoadDatabase();
	}
	// Fallback when the custom manager is not configured.
	static TWeakObjectPtr<UGameDatabase> Fallback;
	if (!Fallback.IsValid())
	{
		UGameDatabase* Db = nullptr;
		if (UGameDatabase* Authored = LoadObject<UGameDatabase>(nullptr, TEXT("/Game/Gielinor/Data/DA_GameDatabase.DA_GameDatabase")))
			Db = DuplicateObject<UGameDatabase>(Authored, GetTransientPackage(), TEXT("GameDatabase_Fallback"));
		else
			Db = NewObject<UGameDatabase>(GetTransientPackage(), TEXT("GameDatabase_Fallback"));
		Db->SetFlags(RF_Transient);
		Db->AddToRoot(); // lives for the session; the custom asset manager path is preferred
		FJsonContentLoader::MergeInto(Db, FJsonContentLoader::DefaultDataDir());
		Fallback = Db;
	}
	return Fallback.Get();
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
		if (UGameDatabase* Authored = Cast<UGameDatabase>(Path.TryLoad()))
		{
			// Work on a transient copy so JSON-created rows never end up saved into the authored asset.
			Database = DuplicateObject<UGameDatabase>(Authored, this, TEXT("GameDatabase_Runtime"));
			Database->SetFlags(RF_Transient);
		}
	}
	if (!Database)
	{
		UE_LOG(LogGielinor, Log, TEXT("No authored GameDatabase asset; building one from Content/Data/*.json."));
		Database = NewObject<UGameDatabase>(this, TEXT("GameDatabase_Runtime"), RF_Transient);
	}
	FJsonContentLoader::MergeInto(Database, FJsonContentLoader::DefaultDataDir());
	return Database;
}
