#include "Data/JsonContentLoader.h"
#include "Data/GameDatabase.h"
#include "Data/ContentDefs.h"
#include "Data/ItemDef.h"
#include "Data/SkillDef.h"
#include "Data/MobDef.h"
#include "Data/NpcDef.h"
#include "Data/QuestDef.h"
#include "Data/RecipeDef.h"
#include "Data/ResourceNodeDef.h"
#include "Data/GielinorId.h"
#include "Gielinor.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace GielinorJson
{
	using FObj = TSharedPtr<FJsonObject>;
	using FArr = TArray<TSharedPtr<FJsonValue>>;

	static FObj LoadFile(const FString& Dir, const TCHAR* Name)
	{
		FString Text;
		const FString Path = FPaths::Combine(Dir, Name);
		if (!FFileHelper::LoadFileToString(Text, *Path)) return nullptr;
		FObj Out;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Out) || !Out.IsValid())
		{
			UE_LOG(LogGielinor, Warning, TEXT("Bad JSON in %s"), *Path);
			return nullptr;
		}
		return Out;
	}

	static FObj Obj(const FObj& O, const TCHAR* Key)
	{
		const TSharedPtr<FJsonObject>* Out = nullptr;
		return (O.IsValid() && O->TryGetObjectField(Key, Out) && Out) ? *Out : nullptr;
	}
	static const FArr* Arr(const FObj& O, const TCHAR* Key)
	{
		const FArr* Out = nullptr;
		return (O.IsValid() && O->TryGetArrayField(Key, Out)) ? Out : nullptr;
	}
	static double Num(const FObj& O, const TCHAR* Key, double Default = 0.0)
	{
		double V = Default;
		if (O.IsValid()) O->TryGetNumberField(Key, V);
		return V;
	}
	static FString Str(const FObj& O, const TCHAR* Key)
	{
		FString V;
		if (O.IsValid()) O->TryGetStringField(Key, V);
		return V;
	}
	static bool Bool(const FObj& O, const TCHAR* Key)
	{
		bool V = false;
		if (O.IsValid()) O->TryGetBoolField(Key, V);
		return V;
	}
	static bool Has(const FObj& O, const TCHAR* Key)
	{
		if (!O.IsValid()) return false;
		const TSharedPtr<FJsonValue> V = O->TryGetField(Key);
		return V.IsValid() && !V->IsNull();
	}
	static FText Txt(const FString& S) { return FText::FromString(S); }
	static FName Id(const FString& S) { return GielinorId::FromName(S); }

	/** Find by Id or create a transient def. bFill is false for authored assets (they win). */
	template<typename T>
	static T* Upsert(UGameDatabase* Db, TArray<TObjectPtr<T>>& List, FName InId, bool& bFill, int32& Created)
	{
		for (T* E : List)
		{
			if (E && E->Id == InId) { bFill = E->HasAnyFlags(RF_Transient); return E; }
		}
		T* N = NewObject<T>(Db, NAME_None, RF_Transient);
		N->Id = InId;
		List.Add(N);
		bFill = true;
		++Created;
		return N;
	}

	/** Effects map: numbers kept, {Skill: n} objects flattened to "<key>.<skill id>", poison {chance,dmg} -> poison / poisonDmg. */
	static void ReadFx(const FObj& Fx, TMap<FName, float>& Out)
	{
		if (!Fx.IsValid()) return;
		for (const auto& Pair : Fx->Values)
		{
			if (!Pair.Value.IsValid()) continue;
			if (Pair.Value->Type == EJson::Number)
			{
				FString Key = Pair.Key;
				// Quest bonuses name skills / monsters: xp.Cooking -> xp.cooking, dmgVs.Giant Spider -> dmgVs.giant_spider.
				int32 Dot;
				if (Key.FindChar(TEXT('.'), Dot))
				{
					const FString Head = Key.Left(Dot), Tail = Key.Mid(Dot + 1);
					Key = Head + TEXT(".") + (Tail == TEXT("*") ? Tail : Id(Tail).ToString());
				}
				Out.Add(FName(*Key), (float)Pair.Value->AsNumber());
			}
			else if (Pair.Value->Type == EJson::Object)
			{
				const FObj Sub = Pair.Value->AsObject();
				if (Pair.Key == TEXT("poison"))
				{
					Out.Add(TEXT("poison"), (float)Num(Sub, TEXT("chance")));
					Out.Add(TEXT("poisonDmg"), (float)Num(Sub, TEXT("dmg")));
				}
				else
				{
					for (const auto& S : Sub->Values)
						if (S.Value.IsValid() && S.Value->Type == EJson::Number)
							Out.Add(FName(*(Pair.Key + TEXT(".") + Id(S.Key).ToString())), (float)S.Value->AsNumber());
				}
			}
		}
	}

	static EToolType ToolFrom(const FString& S)
	{
		if (S == TEXT("axe")) return EToolType::Axe;
		if (S == TEXT("pickaxe")) return EToolType::Pickaxe;
		if (S == TEXT("net")) return EToolType::FishingNet;
		if (S == TEXT("rod")) return EToolType::FishingRod;
		if (S == TEXT("cage")) return EToolType::LobsterPot;
		if (S == TEXT("harpoon")) return EToolType::Harpoon;
		if (S == TEXT("hammer")) return EToolType::Hammer;
		if (S == TEXT("knife")) return EToolType::Knife;
		if (S == TEXT("needle")) return EToolType::Needle;
		if (S == TEXT("tinderbox")) return EToolType::Tinderbox;
		return EToolType::None;
	}

	static EEquipSlot SlotFrom(const FString& Type)
	{
		if (Type == TEXT("weapon")) return EEquipSlot::Weapon;
		if (Type == TEXT("head")) return EEquipSlot::Head;
		if (Type == TEXT("body")) return EEquipSlot::Body;
		if (Type == TEXT("legs")) return EEquipSlot::Legs;
		if (Type == TEXT("shield")) return EEquipSlot::Shield;
		if (Type == TEXT("cape")) return EEquipSlot::Cape;
		if (Type == TEXT("neck")) return EEquipSlot::Neck;
		if (Type == TEXT("ring")) return EEquipSlot::Ring;
		if (Type == TEXT("ammo")) return EEquipSlot::Ammo;
		if (Type == TEXT("hands")) return EEquipSlot::Hands;
		if (Type == TEXT("feet")) return EEquipSlot::Feet;
		return EEquipSlot::None;
	}

	static FLinearColor ColorFromInt(double V)
	{
		const uint32 C = (uint32)V;
		return FLinearColor(FColor((C >> 16) & 0xFF, (C >> 8) & 0xFF, C & 0xFF));
	}
}

FString FJsonContentLoader::DefaultDataDir()
{
	return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data"));
}

int32 FJsonContentLoader::MergeInto(UGameDatabase* Db, const FString& Dir)
{
	using namespace GielinorJson;
	if (!Db) return 0;
	if (!FPaths::DirectoryExists(Dir))
	{
		UE_LOG(LogGielinor, Log, TEXT("No content JSON at %s (authored data assets only)."), *Dir);
		return 0;
	}
	int32 Created = 0;
	bool bFill = false;
	auto Item = [Db](const FString& Name) -> UItemDef* { return Name.IsEmpty() ? nullptr : Db->FindItem(Id(Name)); };
	auto Skill = [Db](const FString& Name) -> USkillDef* { return Name.IsEmpty() ? nullptr : Db->FindSkill(Id(Name)); };

	// ---------------- Skills ----------------
	if (FObj J = LoadFile(Dir, TEXT("skills.json")))
	{
		const FObj Perks = Obj(J, TEXT("perks"));
		if (const FArr* Names = Arr(J, TEXT("skills")))
		{
			for (const TSharedPtr<FJsonValue>& V : *Names)
			{
				const FString Name = V->AsString();
				USkillDef* S = Upsert(Db, Db->Skills, Id(Name), bFill, Created);
				if (S->DisplayName.IsEmpty()) S->DisplayName = Txt(Name);
				if (Name == TEXT("Hitpoints")) S->StartLevel = 10;
				if (S->Perks.Num() == 0)
				{
					if (const FArr* P = Arr(Perks, *Name))
					{
						for (const TSharedPtr<FJsonValue>& PV : *P)
						{
							const FObj PO = PV->AsObject();
							FSkillPerk Perk;
							Perk.Level = (int32)Num(PO, TEXT("level"), 5);
							Perk.Name = Txt(Str(PO, TEXT("name")));
							Perk.Description = Txt(Str(PO, TEXT("desc")));
							Perk.bMilestone = Bool(PO, TEXT("milestone"));
							if (FObj Fx = Obj(PO, TEXT("fx")))
								for (const auto& F : Fx->Values) if (F.Value.IsValid() && F.Value->Type == EJson::Number) Perk.Effects.Add(FName(*F.Key), (float)F.Value->AsNumber());
							S->Perks.Add(Perk);
						}
					}
				}
			}
		}
		if (FObj Caps = Obj(J, TEXT("perkCaps")))
			for (const auto& C : Caps->Values) if (!Db->PerkCaps.Contains(FName(*C.Key))) Db->PerkCaps.Add(FName(*C.Key), (float)C.Value->AsNumber());
	}

	// ---------------- Items ----------------
	if (FObj J = LoadFile(Dir, TEXT("items.json")))
	{
		Db->HealScale = (int32)Num(J, TEXT("healScale"), 10);
		TMap<FString, int32> MetalTier;
		if (const FArr* M = Arr(J, TEXT("metals"))) for (int32 i = 0; i < M->Num(); ++i) MetalTier.Add((*M)[i]->AsString(), i);

		if (FObj Items = Obj(J, TEXT("items")))
		{
			for (const auto& Pair : Items->Values)
			{
				const FObj O = Pair.Value->AsObject();
				UItemDef* I = Upsert(Db, Db->Items, Id(Pair.Key), bFill, Created);
				if (!bFill) continue;
				const FString Type = Str(O, TEXT("type"));
				I->DisplayName = Txt(Pair.Key);
				I->ItemType = FName(*Type);
				I->Value = (int32)Num(O, TEXT("value"), 1);
				I->bStackable = true; // every item stacks in Gielinor Lite
				I->HealAmount = (int32)(Num(O, TEXT("heal")) * Db->HealScale);
				I->PrayerXp = (float)Num(O, TEXT("prayerXp"));
				I->ToolType = ToolFrom(Str(O, TEXT("tool")));
				I->ToolTier = (int32)Num(O, TEXT("toolTier"));
				I->EquipSlot = SlotFrom(Type);
				if (const int32* T = MetalTier.Find(Str(O, TEXT("metal")))) I->MetalTier = *T;
				I->Bonuses.Attack = (int32)Num(O, TEXT("atk"));
				I->Bonuses.Strength = (int32)Num(O, TEXT("str"));
				I->Bonuses.RangedAttack = (int32)Num(O, TEXT("ranged"));
				I->Bonuses.RangedStrength = (int32)Num(O, TEXT("rstr"));
				I->Bonuses.Defence = (int32)Num(O, TEXT("def"));
				I->Bonuses.RangedDefence = (int32)Num(O, TEXT("rdef"));
				I->Bonuses.Prayer = (int32)Num(O, TEXT("prayer"));
				I->Bonuses.Magic = (int32)Num(O, TEXT("mag"));
				I->AttackSpeedTicks = (int32)Num(O, TEXT("speed"), 4);
				I->bRangedWeapon = Type == TEXT("weapon") && Num(O, TEXT("ranged")) != 0.0;
				I->bThrown = Bool(O, TEXT("thrown"));
				if (I->bThrown) { I->bRangedWeapon = true; I->EquipSlot = EEquipSlot::Weapon; }
				I->SetName = FName(*Str(O, TEXT("set")));
				I->CapeSkill = Has(O, TEXT("capeSkill")) ? Id(Str(O, TEXT("capeSkill"))) : NAME_None;
				I->bQuestCape = Bool(O, TEXT("questCape"));
				I->RuneSource = Has(O, TEXT("runeSource")) ? Id(Str(O, TEXT("runeSource"))) : NAME_None;
				I->CleansInto = Has(O, TEXT("clean")) ? Id(Str(O, TEXT("clean"))) : NAME_None;
				I->HerbLevel = (int32)Num(O, TEXT("herbLevel"));
				I->bUnique = Bool(O, TEXT("unique"));
				I->Rarity = FName(*Str(O, TEXT("rarity")));
				I->SpecialText = Txt(Str(O, TEXT("special")));
				if (FObj Req = Obj(O, TEXT("req")))
					for (const auto& R : Req->Values) I->SkillRequirements.Add(Id(R.Key), (int32)R.Value->AsNumber());
				ReadFx(Obj(O, TEXT("eff")), I->Effects);
				if (Has(O, TEXT("regen"))) I->Effects.Add(TEXT("regen"), (float)Num(O, TEXT("regen")));
				if (FObj G = Obj(O, TEXT("gather"))) for (const auto& S : G->Values) I->Effects.Add(FName(*(TEXT("gather.") + Id(S.Key).ToString())), (float)S.Value->AsNumber());
				if (FObj X = Obj(O, TEXT("xp"))) for (const auto& S : X->Values) I->Effects.Add(FName(*(TEXT("xp.") + Id(S.Key).ToString())), (float)S.Value->AsNumber());
				if (FObj Buff = Obj(O, TEXT("buff")))
				{
					I->BuffTicks = (int32)Num(Buff, TEXT("ticks"));
					I->BuffPrayer = (int32)Num(Buff, TEXT("prayer"));
					if (FObj B = Obj(Buff, TEXT("boost")))
					{
						for (const auto& S : B->Values)
						{
							const FArr& FP = S.Value->AsArray();
							if (FP.Num() >= 2) { I->BuffBoostFlat.Add(Id(S.Key), (int32)FP[0]->AsNumber()); I->BuffBoostPct.Add(Id(S.Key), (float)FP[1]->AsNumber()); }
						}
					}
				}
			}
		}
		if (FObj Sets = Obj(J, TEXT("sets")))
		{
			for (const auto& Pair : Sets->Values)
			{
				if (Db->ArmourSets.ContainsByPredicate([&](const FArmourSet& S) { return S.Name == FName(*Pair.Key); })) continue;
				const FObj O = Pair.Value->AsObject();
				FArmourSet Set;
				Set.Name = FName(*Pair.Key);
				if (const FArr* P = Arr(O, TEXT("pieces"))) for (const TSharedPtr<FJsonValue>& V : *P) Set.Pieces.Add(SlotFrom(V->AsString()));
				ReadFx(Obj(O, TEXT("fx")), Set.Effects);
				Db->ArmourSets.Add(Set);
			}
		}
		if (Db->StarterTools.Num() == 0)
			if (const FArr* T = Arr(J, TEXT("starterTools")))
				for (const TSharedPtr<FJsonValue>& V : *T) if (UItemDef* I = Item(V->AsString())) Db->StarterTools.Add(I);
	}

	// Coins are a plain stackable item in Unreal (the web build keeps a gold counter).
	{
		UItemDef* C = Upsert(Db, Db->Items, TEXT("coins"), bFill, Created);
		if (bFill) { C->DisplayName = Txt(TEXT("Coins")); C->bStackable = true; C->Value = 1; C->ItemType = TEXT("currency"); C->Tint = FLinearColor(1.f, 0.85f, 0.2f); }
	}

	// ---------------- Player ----------------
	if (FObj J = LoadFile(Dir, TEXT("player.json"))) Db->InventorySize = (int32)Num(J, TEXT("invSize"), 28);

	// Grimy herb lookup for loot pools: clean herb name -> grimy item.
	TMap<FName, UItemDef*> GrimyOf;
	for (UItemDef* I : Db->Items) if (I && !I->CleansInto.IsNone()) GrimyOf.Add(I->CleansInto, I);

	auto ReadDrops = [&](const FArr* List, TArray<FDropEntry>& Out)
	{
		if (!List) return;
		for (const TSharedPtr<FJsonValue>& V : *List)
		{
			const FObj D = V->AsObject();
			FDropEntry E;
			E.Chance = (float)Num(D, TEXT("chance"), 1.0);
			if (const FArr* Q = Arr(D, TEXT("qty")); Q && Q->Num() >= 2) { E.Min = (int32)(*Q)[0]->AsNumber(); E.Max = (int32)(*Q)[1]->AsNumber(); }
			if (const FArr* H = Arr(D, TEXT("herbs")))
			{
				for (const TSharedPtr<FJsonValue>& HV : *H) if (UItemDef** G = GrimyOf.Find(Id(HV->AsString()))) E.OneOf.Add(*G);
				if (E.OneOf.Num() == 0) continue;
			}
			else
			{
				E.Item = Item(Str(D, TEXT("item")));
				if (!E.Item) continue;
			}
			Out.Add(E);
		}
	};

	// ---------------- Mobs ----------------
	if (FObj J = LoadFile(Dir, TEXT("mobs.json")))
	{
		const FObj Loot = Obj(J, TEXT("loot"));
		if (FObj Monsters = Obj(J, TEXT("monsters")))
		{
			for (const auto& Pair : Monsters->Values)
			{
				const FObj O = Pair.Value->AsObject();
				UMobDef* M = Upsert(Db, Db->Mobs, Id(Pair.Key), bFill, Created);
				if (!bFill) continue;
				M->DisplayName = Txt(Str(O, TEXT("name")));
				M->Title = Txt(Str(O, TEXT("title")));
				M->CombatLevel = (int32)Num(O, TEXT("level"), 1);
				M->Hitpoints = (int32)Num(O, TEXT("hp"), 50);
				M->MaxHit = (int32)Num(O, TEXT("maxHit"), 5);
				M->AttackLevel = (int32)Num(O, TEXT("att"), 1);
				M->DefenceLevel = (int32)Num(O, TEXT("def"), 1);
				M->AttackSeconds = 0.6f * (float)Num(O, TEXT("speed"), 4);
				M->RespawnSeconds = (float)Num(O, TEXT("respawn"), 20000) / 1000.f;
				M->Tint = ColorFromInt(Num(O, TEXT("color"), 0xFFFFFF));
				M->bBoss = Bool(O, TEXT("boss"));
				M->bRanged = Bool(O, TEXT("ranged"));
				if (M->bRanged) M->AttackRange = 100.f * (float)Num(O, TEXT("range"), 5);
				if (FObj P = Obj(O, TEXT("poison"))) { M->PoisonChance = (float)Num(P, TEXT("chance")); M->PoisonDamage = (int32)Num(P, TEXT("dmg")); }
				if (Has(O, TEXT("shape"))) M->Shape = FName(*Str(O, TEXT("shape")));
				const float Scale = (float)Num(O, TEXT("scale"), 1.0);
				M->BodySize = FVector(60, 170, 60) * Scale;
				M->bAggressive = M->bBoss || M->CombatLevel >= 20;
				M->Drops.Reset();
				ReadDrops(Arr(Loot, *Pair.Key), M->Drops);
				if (M->Drops.Num() == 0) if (UItemDef* B = Db->FindItem(TEXT("bones"))) { FDropEntry E; E.Item = B; M->Drops.Add(E); }
			}
		}
		if (Db->Bounties.Num() == 0)
		{
			if (const FArr* B = Arr(J, TEXT("bounties")))
			{
				for (const TSharedPtr<FJsonValue>& V : *B)
				{
					const FObj O = V->AsObject();
					FBountyDef D;
					D.Id = FName(*Str(O, TEXT("id")));
					D.MobId = Id(Str(O, TEXT("mon")));
					D.Count = (int32)Num(O, TEXT("n"), 5);
					D.Coins = (int32)Num(O, TEXT("gp"));
					if (FObj X = Obj(O, TEXT("xp"))) for (const auto& S : X->Values) D.Xp.Add(Id(S.Key), (float)S.Value->AsNumber());
					D.Ask = Txt(Str(O, TEXT("ask")));
					D.Giver = Txt(Str(O, TEXT("giver")));
					D.Where = Txt(Str(O, TEXT("where")));
					Db->Bounties.Add(D);
				}
			}
		}
	}

	// ---------------- Gathering ----------------
	if (FObj J = LoadFile(Dir, TEXT("gathering.json")))
	{
		auto Simple = [&](const TCHAR* Group, const TCHAR* Prefix, EResourceKind Kind, const TCHAR* SkillName, EToolType Tool)
		{
			FObj G = Obj(J, Group);
			if (!G) return;
			for (const auto& Pair : G->Values)
			{
				const FObj O = Pair.Value->AsObject();
				UResourceNodeDef* N = Upsert(Db, Db->ResourceNodes, FName(*(FString(Prefix) + Id(Pair.Key).ToString())), bFill, Created);
				if (!bFill) continue;
				N->DisplayName = Txt(Kind == EResourceKind::Rock ? Pair.Key + TEXT(" rocks") : Kind == EResourceKind::HerbPatch ? Pair.Key + TEXT(" patch") : Pair.Key);
				N->Kind = Kind;
				N->Skill = Skill(SkillName);
				N->ToolRequired = Tool;
				N->LevelRequired = (int32)Num(O, TEXT("level"), 1);
				N->Xp = (float)Num(O, TEXT("xp"));
				N->Product = Item(Str(O, TEXT("item")));
				N->BaseSuccess = 0.3f;
				N->DepleteChance = 0.f; // resources never run out in Gielinor Lite
				N->RespawnSeconds = (float)Num(O, TEXT("respawn"), 8000) / 1000.f;
				N->TickSeconds = 1.2f;
				N->PureLevel = (int32)Num(O, TEXT("pureAt"));
				N->PureProduct = Item(Str(O, TEXT("pure")));
			}
		};
		Simple(TEXT("trees"), TEXT("tree_"), EResourceKind::Tree, TEXT("Woodcutting"), EToolType::Axe);
		Simple(TEXT("rocks"), TEXT("rock_"), EResourceKind::Rock, TEXT("Mining"), EToolType::Pickaxe);
		Simple(TEXT("herbPatches"), TEXT("herb_"), EResourceKind::HerbPatch, TEXT("Herblore"), EToolType::None);
		if (FObj F = Obj(J, TEXT("fishSpots")))
		{
			for (const auto& Pair : F->Values)
			{
				const FObj O = Pair.Value->AsObject();
				UResourceNodeDef* N = Upsert(Db, Db->ResourceNodes, FName(*(TEXT("fish_") + Id(Pair.Key).ToString())), bFill, Created);
				if (!bFill) continue;
				N->DisplayName = Txt(TEXT("Fishing spot (") + Str(O, TEXT("verb")) + TEXT(")"));
				N->Kind = EResourceKind::FishingSpot;
				N->Skill = Skill(TEXT("Fishing"));
				N->ToolRequired = ToolFrom(Str(O, TEXT("tool")));
				N->BaseSuccess = 0.3f;
				N->DepleteChance = 0.f;
				N->TickSeconds = 1.2f;
				N->Catches.Reset();
				if (const FArr* C = Arr(O, TEXT("catches")))
				{
					for (const TSharedPtr<FJsonValue>& V : *C)
					{
						const FObj CO = V->AsObject();
						FResourceCatch Catch;
						Catch.Item = Item(Str(CO, TEXT("item")));
						Catch.Level = (int32)Num(CO, TEXT("level"), 1);
						Catch.Xp = (float)Num(CO, TEXT("xp"));
						if (Catch.Item) N->Catches.Add(Catch);
					}
				}
				if (N->Catches.Num() > 0)
				{
					N->Product = N->Catches.Last().Item;
					N->LevelRequired = N->Catches.Last().Level;
					N->Xp = N->Catches.Last().Xp;
				}
			}
		}
	}

	// ---------------- Recipes ----------------
	if (FObj J = LoadFile(Dir, TEXT("recipes.json")))
	{
		if (const FArr* List = Arr(J, TEXT("recipes")))
		{
			for (const TSharedPtr<FJsonValue>& V : *List)
			{
				const FObj O = V->AsObject();
				URecipeDef* R = Upsert(Db, Db->Recipes, Id(Str(O, TEXT("name"))), bFill, Created);
				if (!bFill) continue;
				R->DisplayName = Txt(Str(O, TEXT("name")));
				const FString St = Str(O, TEXT("station"));
				R->Station = St == TEXT("furnace") ? EStationType::Furnace : St == TEXT("anvil") ? EStationType::Anvil : St == TEXT("range") ? EStationType::Range : EStationType::Hand;
				R->Skill = Skill(Str(O, TEXT("skill")));
				R->LevelRequired = (int32)Num(O, TEXT("level"), 1);
				R->Xp = (float)Num(O, TEXT("xp"));
				R->ToolRequired = ToolFrom(Str(O, TEXT("tool")));
				R->Inputs.Reset();
				if (FObj In = Obj(O, TEXT("inputs")))
					for (const auto& P : In->Values) { FItemAmount A; A.Item = Item(P.Key); A.Amount = (int32)P.Value->AsNumber(); if (A.Item) R->Inputs.Add(A); }
				R->Output = Item(Str(O, TEXT("output")));
				R->OutputAmount = 1;
				if (R->Output && R->Output->ItemType == TEXT("ammo")) R->OutputAmount = 15;
				R->Seconds = 1.8f; // 3 ticks
				if (Has(O, TEXT("burnStop")) && R->Station == EStationType::Range)
				{
					R->FailItem = Db->FindItem(TEXT("burnt_food"));
					R->FailChanceAtReq = 0.45f;
					R->StopFailLevel = (int32)Num(O, TEXT("burnStop"), 99);
				}
			}
		}
	}
	// Ammo/bolt recipe yields come from the item's "count" in the web data; patch from items.json if present.
	if (FObj J = LoadFile(Dir, TEXT("items.json")))
	{
		if (FObj Items = Obj(J, TEXT("items")))
		{
			for (URecipeDef* R : Db->Recipes)
			{
				if (!R || !R->HasAnyFlags(RF_Transient) || !R->Output) continue;
				const FObj O = Obj(Items, *R->Output->DisplayName.ToString());
				if (Has(O, TEXT("count"))) R->OutputAmount = (int32)Num(O, TEXT("count"), 1);
			}
		}
	}

	// ---------------- NPCs, towns, quests ----------------
	if (FObj J = LoadFile(Dir, TEXT("quests.json")))
	{
		if (FObj Npcs = Obj(J, TEXT("npcs")))
		{
			for (const auto& Pair : Npcs->Values)
			{
				const FObj O = Pair.Value->AsObject();
				UNpcDef* N = Upsert(Db, Db->Npcs, FName(*Pair.Key), bFill, Created);
				if (!bFill) continue;
				N->DisplayName = Txt(Str(O, TEXT("name")));
				N->IdleLines = { Txt(Str(O, TEXT("examine"))) };
			}
		}
		if (const FArr* Milestones = Arr(J, TEXT("qpMilestones")); Milestones && Db->QuestPointMilestones.Num() == 0)
		{
			for (const TSharedPtr<FJsonValue>& V : *Milestones)
			{
				const FObj O = V->AsObject();
				FQuestPointMilestone M;
				M.Points = (int32)Num(O, TEXT("qp"));
				M.Description = Txt(Str(O, TEXT("desc")));
				ReadFx(Obj(O, TEXT("fx")), M.Bonus);
				Db->QuestPointMilestones.Add(M);
			}
		}
		if (const FArr* Quests = Arr(J, TEXT("quests")))
		{
			for (const TSharedPtr<FJsonValue>& V : *Quests)
			{
				const FObj O = V->AsObject();
				UQuestDef* Q = Upsert(Db, Db->Quests, FName(*Str(O, TEXT("id"))), bFill, Created);
				if (!bFill) continue;
				Q->DisplayName = Txt(Str(O, TEXT("name")));
				Q->Giver = Db->FindNpc(FName(*Str(O, TEXT("giver"))));
				Q->QuestPoints = (int32)Num(O, TEXT("points"), 1);
				Q->Difficulty = Txt(Str(O, TEXT("diff")));
				Q->Town = FName(*Str(O, TEXT("town")));
				Q->StartText = Txt(Str(O, TEXT("start")));
				Q->RewardLines.Reset();
				if (const FArr* RL = Arr(O, TEXT("rewards"))) for (const TSharedPtr<FJsonValue>& L : *RL) Q->RewardLines.Add(Txt(L->AsString()));
				if (FObj Bonus = Obj(O, TEXT("bonus"))) { Q->PermanentBonusText = Txt(Str(Bonus, TEXT("desc"))); ReadFx(Obj(Bonus, TEXT("fx")), Q->PermanentBonus); }

				const FObj D = Obj(O, TEXT("data"));
				Q->bNeedsStepAuthoring = !D.IsValid();
				if (!D) continue;
				Q->IntroLines.Reset();
				if (const FArr* In = Arr(D, TEXT("intro"))) for (const TSharedPtr<FJsonValue>& L : *In) Q->IntroLines.Add(Txt(L->AsString()));
				Q->EndLine = Txt(Str(D, TEXT("end")));
				Q->ThanksLine = Txt(Str(D, TEXT("thanks")));
				Q->Coins = (int32)Num(D, TEXT("gold"));
				Q->XpRewards.Reset();
				if (FObj X = Obj(D, TEXT("xp"))) for (const auto& S : X->Values) { FXpReward R; R.Skill = Skill(S.Key); R.Xp = (float)S.Value->AsNumber(); if (R.Skill) Q->XpRewards.Add(R); }
				Q->ItemRewards.Reset();
				if (FObj IR = Obj(D, TEXT("items"))) for (const auto& S : IR->Values) { FItemAmount A; A.Item = Item(S.Key); A.Amount = (int32)S.Value->AsNumber(); if (A.Item) Q->ItemRewards.Add(A); }
				if (FObj Req = Obj(D, TEXT("req")))
				{
					if (FObj L = Obj(Req, TEXT("lvl"))) for (const auto& S : L->Values) Q->RequiredLevels.Add(Id(S.Key), (int32)S.Value->AsNumber());
					if (const FArr* QL = Arr(Req, TEXT("q"))) for (const TSharedPtr<FJsonValue>& S : *QL) Q->RequiredQuests.Add(FName(*S->AsString()));
				}
				Q->Steps.Reset();
				if (const FArr* Steps = Arr(D, TEXT("steps")))
				{
					for (const TSharedPtr<FJsonValue>& SV : *Steps)
					{
						const FObj S = SV->AsObject();
						const FString T = Str(S, TEXT("t"));
						FQuestStep Step;
						FString Desc = Str(S, TEXT("nag"));
						if (Desc.IsEmpty()) Desc = Str(S, TEXT("hint"));
						if (T == TEXT("bring"))
						{
							Step.Type = EQuestStepType::Deliver;
							Step.Npc = Q->Giver;
							if (FObj Its = Obj(S, TEXT("items"))) for (const auto& P : Its->Values) { FItemAmount A; A.Item = Item(P.Key); A.Amount = (int32)P.Value->AsNumber(); if (A.Item) Step.Items.Add(A); }
							if (Has(S, TEXT("to"))) if (UNpcDef* To = Db->FindNpc(FName(*Str(S, TEXT("to"))))) Step.Npc = To;
						}
						else if (T == TEXT("talk"))
						{
							Step.Type = EQuestStepType::Talk;
							Step.Npc = Db->FindNpc(FName(*Str(S, TEXT("npc"))));
							if (const FArr* Say = Arr(S, TEXT("say"))) for (const TSharedPtr<FJsonValue>& L : *Say) Step.NpcLines.Add(Txt(L->AsString()));
							if (Desc.IsEmpty() && Step.Npc) Desc = FString::Printf(TEXT("Talk to %s %s."), *Step.Npc->DisplayName.ToString(), *Str(S, TEXT("where")));
						}
						else if (T == TEXT("xp")) { Step.Type = EQuestStepType::Xp; Step.SkillId = Id(Str(S, TEXT("skill"))); Step.Amount = (int32)Num(S, TEXT("n"), 1); }
						else if (T == TEXT("visit")) { Step.Type = EQuestStepType::Visit; Step.PlaceId = FName(*Str(S, TEXT("place"))); if (Desc.IsEmpty()) Desc = TEXT("Go to ") + Str(S, TEXT("place")) + TEXT("."); }
						else if (T == TEXT("kill")) { Step.Type = EQuestStepType::Kill; Step.Mob = Db->FindMob(Id(Str(S, TEXT("mon")))); Step.Amount = (int32)Num(S, TEXT("n"), 1); }
						else if (T == TEXT("bury")) { Step.Type = EQuestStepType::Bury; Step.Amount = (int32)Num(S, TEXT("n"), 1); }
						else continue;
						Step.Description = Txt(Desc);
						Q->Steps.Add(Step);
					}
				}
			}
		}
	}

	// ---------------- Combat: styles, spells, abilities ----------------
	if (FObj J = LoadFile(Dir, TEXT("combat.json")))
	{
		Db->DamageReductionCap = (float)Num(J, TEXT("drCap"), 0.6);
		Db->SpellTicks = (int32)Num(J, TEXT("spellTicks"), 5);
		auto Styles = [&](const TCHAR* Key, ECombatType Type, TArray<FCombatStyle>& Out)
		{
			if (Out.Num() > 0) return;
			if (const FArr* L = Arr(J, Key))
			{
				for (const TSharedPtr<FJsonValue>& V : *L)
				{
					const FObj O = V->AsObject();
					FCombatStyle S;
					S.Id = Id(Str(O, TEXT("name")));
					S.DisplayName = Txt(Str(O, TEXT("name")));
					S.Type = Type;
					S.AttackBonus = (int32)Num(O, TEXT("att"));
					S.StrengthBonus = (int32)Num(O, TEXT("str"));
					S.DefenceBonus = (int32)Num(O, TEXT("def"));
					S.SpeedTicks = (int32)Num(O, TEXT("speed"));
					if (const FArr* X = Arr(O, TEXT("xp"))) for (const TSharedPtr<FJsonValue>& XV : *X) S.XpSkills.Add(Id(XV->AsString()));
					Out.Add(S);
				}
			}
		};
		Styles(TEXT("melee"), ECombatType::Melee, Db->MeleeStyles);
		Styles(TEXT("ranged"), ECombatType::Ranged, Db->RangedStyles);
		if (const FArr* L = Arr(J, TEXT("spells")))
		{
			for (const TSharedPtr<FJsonValue>& V : *L)
			{
				const FObj O = V->AsObject();
				USpellDef* S = Upsert(Db, Db->Spells, FName(*Str(O, TEXT("id"))), bFill, Created);
				if (!bFill) continue;
				S->DisplayName = Txt(Str(O, TEXT("name")));
				S->Level = (int32)Num(O, TEXT("level"), 1);
				S->MaxHit = (int32)Num(O, TEXT("maxHit"), 20);
				S->Xp = (float)Num(O, TEXT("xp"));
				S->Runes.Reset();
				if (FObj R = Obj(O, TEXT("runes"))) for (const auto& P : R->Values) { FRuneCost C; C.Rune = Item(P.Key); C.Amount = (int32)P.Value->AsNumber(); if (C.Rune) S->Runes.Add(C); }
				const FString N = S->DisplayName.ToString();
				S->Color = N.StartsWith(TEXT("Fire")) ? FLinearColor(1.f, 0.35f, 0.1f) : N.StartsWith(TEXT("Water")) ? FLinearColor(0.2f, 0.45f, 1.f) : N.StartsWith(TEXT("Earth")) ? FLinearColor(0.5f, 0.35f, 0.15f) : FLinearColor(0.85f, 0.95f, 1.f);
			}
		}
		if (const FArr* L = Arr(J, TEXT("abilities")))
		{
			for (const TSharedPtr<FJsonValue>& V : *L)
			{
				const FObj O = V->AsObject();
				UAbilityDef* A = Upsert(Db, Db->Abilities, FName(*Str(O, TEXT("id"))), bFill, Created);
				if (!bFill) continue;
				A->DisplayName = Txt(Str(O, TEXT("name")));
				A->Description = Txt(Str(O, TEXT("desc")));
				const FString Tier = Str(O, TEXT("tier"));
				A->Kind = Tier == TEXT("threshold") ? EAbilityKind::Threshold : Tier == TEXT("ultimate") ? EAbilityKind::Ultimate : EAbilityKind::Basic;
				A->Cooldown = (int32)Num(O, TEXT("cd"), 10);
				A->Skill = Id(Str(O, TEXT("skill")));
				A->Level = (int32)Num(O, TEXT("level"), 1);
				A->Style = FName(*Str(O, TEXT("style")));
				A->Special = FName(*Str(O, TEXT("id")));
			}
		}
	}

	// ---------------- Prayers ----------------
	if (FObj J = LoadFile(Dir, TEXT("prayers.json")))
	{
		if (const FArr* L = Arr(J, TEXT("prayers")))
		{
			for (const TSharedPtr<FJsonValue>& V : *L)
			{
				const FObj O = V->AsObject();
				UPrayerDef* P = Upsert(Db, Db->Prayers, FName(*Str(O, TEXT("id"))), bFill, Created);
				if (!bFill) continue;
				P->DisplayName = Txt(Str(O, TEXT("name")));
				P->Description = Txt(Str(O, TEXT("desc")));
				P->Level = (int32)Num(O, TEXT("level"), 1);
				P->DrainPerTick = (float)Num(O, TEXT("drain"), 0.17);
				P->Group = FName(*Str(O, TEXT("group")));
				P->RequiredQuest = FName(*Str(O, TEXT("quest")));
				ReadFx(Obj(O, TEXT("fx")), P->Effects);
			}
		}
	}

	// ---------------- Potions ----------------
	if (FObj J = LoadFile(Dir, TEXT("potions.json")))
	{
		if (FObj Pots = Obj(J, TEXT("potions")))
		{
			for (const auto& Pair : Pots->Values)
			{
				const FObj O = Pair.Value->AsObject();
				UPotionDef* P = Upsert(Db, Db->Potions, Id(Pair.Key), bFill, Created);
				if (!bFill) continue;
				P->Item = Item(Pair.Key);
				P->Instant = FName(*Str(O, TEXT("instant")));
				P->ImmuneTicks = (int32)Num(O, TEXT("ticks"));
				if (FObj B = Obj(O, TEXT("boost")))
					for (const auto& S : B->Values) { const FObj BO = S.Value->AsObject(); P->BoostFlat.Add(Id(S.Key), (int32)Num(BO, TEXT("flat"))); P->BoostPct.Add(Id(S.Key), (float)Num(BO, TEXT("pct"))); }
				ReadFx(Obj(O, TEXT("fx")), P->Fx);
			}
		}
	}

	// ---------------- Shops ----------------
	if (FObj J = LoadFile(Dir, TEXT("shops.json")))
	{
		if (FObj Shops = Obj(J, TEXT("shops")))
		{
			for (const auto& Pair : Shops->Values)
			{
				const FObj O = Pair.Value->AsObject();
				UShopDef* S = Upsert(Db, Db->Shops, FName(*Pair.Key), bFill, Created);
				if (!bFill) continue;
				S->DisplayName = Txt(Str(O, TEXT("title")));
				S->Stock.Reset();
				if (const FArr* L = Arr(O, TEXT("stock")))
				{
					for (const TSharedPtr<FJsonValue>& V : *L)
					{
						const FObj E = V->AsObject();
						FShopStock Entry;
						Entry.Item = Item(Str(E, TEXT("item")));
						if (!Entry.Item) continue;
						Entry.Price = (int32)Num(E, TEXT("price"));
						const FString Unlock = Str(E, TEXT("unlock"));
						if (Unlock == TEXT("all")) Entry.bRequiresQuestCape = true; else Entry.UnlockQuest = FName(*Unlock);
						if (Has(E, TEXT("skill99"))) Entry.RequiresSkill99 = Id(Str(E, TEXT("skill99")));
						S->Stock.Add(Entry);
					}
				}
			}
		}
	}

	// ---------------- Agility, thieving, runecrafting ----------------
	if (FObj J = LoadFile(Dir, TEXT("activities.json")))
	{
		if (FObj Courses = Obj(J, TEXT("agilityCourses")))
		{
			for (const auto& Pair : Courses->Values)
			{
				const FObj O = Pair.Value->AsObject();
				UAgilityCourseDef* C = Upsert(Db, Db->AgilityCourses, FName(*Pair.Key), bFill, Created);
				if (!bFill) continue;
				C->DisplayName = Txt(Str(O, TEXT("name")));
				C->Level = (int32)Num(O, TEXT("level"), 1);
				C->LapXp = (float)Num(O, TEXT("lapXp"));
				C->Obstacles.Reset();
				if (const FArr* L = Arr(O, TEXT("obstacles")))
				{
					for (const TSharedPtr<FJsonValue>& V : *L)
					{
						const FObj OO = V->AsObject();
						FAgilityObstacle Ob;
						Ob.DisplayName = Txt(Str(OO, TEXT("name")));
						Ob.Verb = Txt(Str(OO, TEXT("verb")));
						Ob.Kind = FName(*Str(OO, TEXT("kind")));
						Ob.Level = (int32)Num(OO, TEXT("level"), C->Level);
						Ob.Xp = (float)Num(OO, TEXT("xp"));
						Ob.FailDamage = (int32)Num(OO, TEXT("failDmg"), 10);
						C->Obstacles.Add(Ob);
					}
				}
			}
		}
		if (FObj T = Obj(J, TEXT("thieveTargets")))
		{
			for (const auto& Pair : T->Values)
			{
				const FObj O = Pair.Value->AsObject();
				UThieveTargetDef* D = Upsert(Db, Db->ThieveTargets, Id(Pair.Key), bFill, Created);
				if (!bFill) continue;
				D->DisplayName = Txt(Pair.Key);
				D->Level = (int32)Num(O, TEXT("level"), 1);
				D->Xp = (float)Num(O, TEXT("xp"));
				D->StunTicks = (int32)Num(O, TEXT("stun"), 5);
				D->FailDamage = (int32)Num(O, TEXT("dmg"), 10);
				D->Loot.Reset();
				ReadDrops(Arr(O, TEXT("loot")), D->Loot);
			}
		}
		if (FObj A = Obj(J, TEXT("runeAltars")))
		{
			for (const auto& Pair : A->Values)
			{
				const FObj O = Pair.Value->AsObject();
				URuneAltarDef* D = Upsert(Db, Db->RuneAltars, Id(Pair.Key), bFill, Created);
				if (!bFill) continue;
				D->DisplayName = Txt(Pair.Key + TEXT(" altar"));
				D->Rune = Item(Str(O, TEXT("rune")));
				D->Level = (int32)Num(O, TEXT("level"), 1);
				D->XpPerEssence = (float)Num(O, TEXT("xp"), 5);
				D->MultiplierEvery = (int32)Num(O, TEXT("multi"));
				D->bPureEssenceOnly = Bool(O, TEXT("pure"));
			}
		}
	}

	// ---------------- World (towns only; the level blockout is built by Content/Python/build_world_from_json.py) ----------------
	if (FObj J = LoadFile(Dir, TEXT("world.json")))
	{
		if (FObj Towns = Obj(J, TEXT("towns")); Towns && Db->Towns.Num() == 0)
		{
			for (const auto& Pair : Towns->Values)
			{
				const FObj O = Pair.Value->AsObject();
				FTownInfo T;
				T.Id = Id(Pair.Key);
				T.DisplayName = Txt(Pair.Key);
				T.Position = FVector2D(Num(O, TEXT("x")), Num(O, TEXT("z")));
				Db->Towns.Add(T);
			}
		}
	}

	UE_LOG(LogGielinor, Log, TEXT("JSON content: created %d defs (%d items, %d quests, %d mobs) from %s"),
		Created, Db->Items.Num(), Db->Quests.Num(), Db->Mobs.Num(), *Dir);
	return Created;
}
