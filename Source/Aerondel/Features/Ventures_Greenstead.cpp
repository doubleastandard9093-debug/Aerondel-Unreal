#include "Features/VenturesFeature.h"
#include "UI/AerondelPanels.h"
#include "Core/AerondelPlayerState.h"
#include "Inventory/InventoryComponent.h"
#include "Rules/AerondelRules.h"
#include "World/AerondelScalability.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// Ventures group 14 (web adapt/greenstead.js): the making-things farm on the Hearthhold island.
// Not a Townstead, not a Freeframe plot. Buying the deed does not train Farming; planting does.
// Décor grants a little Construction. Preserves are Farming, not Cooking.

namespace VenturesGreen
{
	struct FCrop { const TCHAR* Id; const TCHAR* Name; const TCHAR* Seed; const TCHAR* Item; const TCHAR* Season; int32 Lvl = 1; int32 Grow = 120000; int32 PlantXp = 8; int32 HarvestXp = 25; };
	static const FCrop Crops[] = {
		{ TEXT("potato"),     TEXT("Potato"),     TEXT("Potato seed"),     TEXT("Potato"),     TEXT("spring"), 1,  120000, 8,  25 },
		{ TEXT("cabbage"),    TEXT("Cabbage"),    TEXT("Cabbage seed"),    TEXT("Cabbage"),    TEXT("spring"), 5,  150000, 10, 32 },
		{ TEXT("wheat"),      TEXT("Wheat"),      TEXT("Wheat seed"),      TEXT("Wheat"),      TEXT("summer"), 1,  150000, 8,  28 },
		{ TEXT("tomato"),     TEXT("Tomato"),     TEXT("Tomato seed"),     TEXT("Tomato"),     TEXT("summer"), 15, 180000, 12, 40 },
		{ TEXT("pumpkin"),    TEXT("Pumpkin"),    TEXT("Pumpkin seed"),    TEXT("Pumpkin"),    TEXT("autumn"), 20, 240000, 14, 50 },
		{ TEXT("winterroot"), TEXT("Winterroot"), TEXT("Winterroot seed"), TEXT("Winterroot"), TEXT("winter"), 10, 200000, 12, 36 },
	};
	struct FAnimal { const TCHAR* Id; const TCHAR* Name; const TCHAR* Produce; const TCHAR* Feed; int32 Time = 120000; int32 Lvl = 1; int32 Xp = 18; int32 Gold = 200; };
	static const FAnimal Animals[] = {
		{ TEXT("chicken"), TEXT("Chicken"), TEXT("Egg"),  TEXT("Wheat"), 120000, 1,  18, 200 },
		{ TEXT("cow"),     TEXT("Cow"),     TEXT("Milk"), TEXT("Wheat"), 180000, 15, 28, 800 },
		{ TEXT("sheep"),   TEXT("Sheep"),   TEXT("Wool"), TEXT("Wheat"), 180000, 25, 24, 600 },
	};
	struct FDecor { const TCHAR* Id; const TCHAR* Name; const TCHAR* M1; int32 N1; const TCHAR* M2; int32 N2; int32 Xp; };
	static const FDecor Decors[] = {
		{ TEXT("fence"),     TEXT("Fence"),     TEXT("Logs"), 2, nullptr, 0, 8 },
		{ TEXT("scarecrow"), TEXT("Scarecrow"), TEXT("Logs"), 3, TEXT("Flax"), 1, 12 },
		{ TEXT("bench"),     TEXT("Bench"),     TEXT("Logs"), 4, nullptr, 0, 10 },
	};
	static const struct { const TCHAR* Seed; int32 Price; } Seeds[] = {
		{ TEXT("Potato seed"), 5 }, { TEXT("Cabbage seed"), 6 }, { TEXT("Wheat seed"), 5 },
		{ TEXT("Tomato seed"), 8 }, { TEXT("Pumpkin seed"), 10 }, { TEXT("Winterroot seed"), 8 },
	};
	static constexpr int32 DeedGold = 2000;
	static const FCrop* FindCrop(const FString& Id) { for (const FCrop& C : Crops) if (Id == C.Id) return &C; return nullptr; }
	static const FAnimal* FindAnimal(const FString& Id) { for (const FAnimal& A : Animals) if (Id == A.Id) return &A; return nullptr; }
	static const FDecor* FindDecor(const FString& Id) { for (const FDecor& D : Decors) if (Id == D.Id) return &D; return nullptr; }
	static FString SeasonNow()
	{
		const int32 M = FDateTime::Now().GetMonth();
		return M >= 3 && M <= 5 ? TEXT("spring") : M >= 6 && M <= 8 ? TEXT("summer") : M >= 9 && M <= 11 ? TEXT("autumn") : TEXT("winter");
	}
	static FString SeasonLabel(const FString& S)
	{
		if (S == TEXT("spring")) return TEXT("Spring");
		if (S == TEXT("summer")) return TEXT("Summer");
		if (S == TEXT("autumn")) return TEXT("Autumn");
		return TEXT("Winter");
	}
	static void VAct14(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
}
using namespace VenturesGreen;

void UVenturesFeature::ResetGreen()
{
	bGreenDeed = false;
	GreenPlots.Reset();
	GreenAnimals.Reset();
	GreenDecor.Reset();
	GreenGrown.Reset();
}

void UVenturesFeature::SaveGreen(FJsonObject& A) const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	S->SetBoolField(TEXT("deed"), bGreenDeed);
	TArray<TSharedPtr<FJsonValue>> Plots;
	for (const FGreenPlot& P : GreenPlots)
	{
		TSharedPtr<FJsonObject> J = MakeShared<FJsonObject>();
		if (P.Crop.IsEmpty()) J->SetField(TEXT("crop"), MakeShared<FJsonValueNull>());
		else J->SetStringField(TEXT("crop"), P.Crop);
		J->SetNumberField(TEXT("plantedAt"), (double)P.PlantedAt);
		J->SetStringField(TEXT("season"), P.Season);
		J->SetBoolField(TEXT("cover"), P.bCover);
		Plots.Add(MakeShared<FJsonValueObject>(J));
	}
	S->SetArrayField(TEXT("plots"), Plots);
	TArray<TSharedPtr<FJsonValue>> Ans;
	for (const FGreenAnimal& A1 : GreenAnimals)
	{
		TSharedPtr<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("type"), A1.Type);
		J->SetNumberField(TEXT("fedAt"), (double)A1.FedAt);
		J->SetBoolField(TEXT("ready"), A1.bReady);
		Ans.Add(MakeShared<FJsonValueObject>(J));
	}
	S->SetArrayField(TEXT("animals"), Ans);
	TArray<TSharedPtr<FJsonValue>> Dec;
	for (const FGreenDecor& D : GreenDecor)
	{
		TSharedPtr<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("id"), D.Id);
		J->SetNumberField(TEXT("x"), D.X);
		J->SetNumberField(TEXT("z"), D.Z);
		J->SetNumberField(TEXT("rot"), D.Rot);
		Dec.Add(MakeShared<FJsonValueObject>(J));
	}
	S->SetArrayField(TEXT("decor"), Dec);
	PutIntMap(*S, TEXT("grown"), GreenGrown);
	A.SetObjectField(TEXT("green"), S);
}

void UVenturesFeature::LoadGreen(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> S = JObj(A, TEXT("green"));
	if (!S) return;
	bGreenDeed = JBool(*S, TEXT("deed"));
	if (const TArray<TSharedPtr<FJsonValue>>* Plots = nullptr; S->TryGetArrayField(TEXT("plots"), Plots) && Plots)
	{
		for (const TSharedPtr<FJsonValue>& V : *Plots)
		{
			if (!V.IsValid() || V->Type != EJson::Object) continue;
			const TSharedPtr<FJsonObject> J = V->AsObject();
			FGreenPlot P;
			P.Crop = JStr(*J, TEXT("crop"));
			if (P.Crop == TEXT("null") || !FindCrop(P.Crop)) { P.Crop.Reset(); }
			P.PlantedAt = P.Crop.IsEmpty() ? 0 : JInt(*J, TEXT("plantedAt"));
			P.Season = JStr(*J, TEXT("season"));
			P.bCover = JBool(*J, TEXT("cover"));
			GreenPlots.Add(P);
			if (GreenPlots.Num() >= 16) break;
		}
	}
	if (const TArray<TSharedPtr<FJsonValue>>* Ans = nullptr; S->TryGetArrayField(TEXT("animals"), Ans) && Ans)
	{
		for (const TSharedPtr<FJsonValue>& V : *Ans)
		{
			if (!V.IsValid() || V->Type != EJson::Object) continue;
			const TSharedPtr<FJsonObject> J = V->AsObject();
			const FString Type = JStr(*J, TEXT("type"));
			if (!FindAnimal(Type)) continue;
			FGreenAnimal A1; A1.Type = Type; A1.FedAt = JInt(*J, TEXT("fedAt")); A1.bReady = JBool(*J, TEXT("ready"));
			GreenAnimals.Add(A1);
			if (GreenAnimals.Num() >= 8) break;
		}
	}
	if (const TArray<TSharedPtr<FJsonValue>>* Dec = nullptr; S->TryGetArrayField(TEXT("decor"), Dec) && Dec)
	{
		for (const TSharedPtr<FJsonValue>& V : *Dec)
		{
			if (!V.IsValid() || V->Type != EJson::Object) continue;
			const TSharedPtr<FJsonObject> J = V->AsObject();
			const FString Id = JStr(*J, TEXT("id"));
			if (!FindDecor(Id)) continue;
			FGreenDecor D; D.Id = Id; D.X = (float)JNum(*J, TEXT("x")); D.Z = (float)JNum(*J, TEXT("z")); D.Rot = (float)JNum(*J, TEXT("rot"));
			GreenDecor.Add(D);
			if (GreenDecor.Num() >= 80) break;
		}
	}
	GreenGrown = JIntMap(*S, TEXT("grown"));
}

int32 UVenturesFeature::GreenGrewOf(const FString& Item) const
{
	const int32* N = GreenGrown.Find(Item);
	return N ? FMath::Min(*N, CountItem(Item)) : 0;
}

void UVenturesFeature::GreenGrewAdd(const FString& Item, int32 N)
{
	int32& V = GreenGrown.FindOrAdd(Item);
	V = FMath::Max(0, V + N);
	if (V <= 0) GreenGrown.Remove(Item);
}

int32 UVenturesFeature::GreenPlotN() const
{
	const int32 L = SkillLevel(TEXT("Farming"));
	int32 N = 4;
	if (L >= 25) N += 2; if (L >= 50) N += 2; if (L >= 75) N += 2; if (L >= 99) N += 2;
	N += FMath::Max(0, L - 99) / 200;
	return FMath::Clamp(N, 4, 16);
}

void UVenturesFeature::GreenEnsurePlots()
{
	while (GreenPlots.Num() < GreenPlotN()) GreenPlots.Add(FGreenPlot());
}

int32 UVenturesFeature::GreenAnimalCap() const
{
	const int32 L = SkillLevel(TEXT("Farming"));
	int32 N = 2;
	if (L >= 50) N += 1; if (L >= 99) N += 1;
	if (const AAerondelPlayerState* PS = GetPS()) N += FMath::RoundToInt(UAerondelRules::Perk(PS, TEXT("farming"), TEXT("animalCap")));
	const EAerondelTier T = UAerondelScalability::GetAppliedTier();
	const int32 Hard = (T == EAerondelTier::Low || T == EAerondelTier::Quest) ? 4 : 6;
	return FMath::Clamp(N, 1, Hard);
}

int32 UVenturesFeature::GreenDecorCap() const
{
	const EAerondelTier T = UAerondelScalability::GetAppliedTier();
	return (T == EAerondelTier::Low || T == EAerondelTier::Quest) ? 40 : 80;
}

int32 UVenturesFeature::GreenGrowMs(const FString& CropId) const
{
	const FCrop* C = FindCrop(CropId); if (!C) return 0;
	float Speed = 0.f;
	if (const AAerondelPlayerState* PS = GetPS()) Speed = UAerondelRules::Perk(PS, TEXT("farming"), TEXT("growSpeed"));
	Speed += GreenLeyGrowBonus();
	return FMath::Max(4000, FMath::RoundToInt(C->Grow / (1.f + Speed)));
}

int32 UVenturesFeature::GreenFarmXp(int32 Base, bool bInSeason) const
{
	float Bonus = 0.f;
	if (const AAerondelPlayerState* PS = GetPS()) Bonus = UAerondelRules::Perk(PS, TEXT("farming"), TEXT("xpBonus"));
	return FMath::RoundToInt(Base * (bInSeason ? 1.25f : 1.f) * (1.f + Bonus));
}



bool UVenturesFeature::GreenHome() const
{
	FVector2D P;
	if (Run.bOn || !bGreenDeed || !PlayerWebPos(P)) return false;
	const FVector2D C = ZoneC(TEXT("hold"));
	return FMath::Abs(P.X - C.X) < 20.0 && FMath::Abs(P.Y - C.Y) < 16.0;
}

bool UVenturesFeature::GreenCanSeason(const FString& CropId, bool bCovered) const
{
	const FCrop* C = FindCrop(CropId); if (!C) return false;
	return SeasonNow() == C->Season || (bCovered && SkillLevel(TEXT("Farming")) >= 50);
}

bool UVenturesFeature::GreenRipe(int32 I) const
{
	if (!GreenPlots.IsValidIndex(I) || GreenPlots[I].Crop.IsEmpty()) return false;
	const FCrop* C = FindCrop(GreenPlots[I].Crop);
	return C && NowMs() - GreenPlots[I].PlantedAt >= GreenGrowMs(GreenPlots[I].Crop);
}

bool UVenturesFeature::GreenBuy()
{
	if (bGreenDeed) { Say(TEXT("You already hold the Greenstead deed.")); return false; }
	if (!TakeCoins(DeedGold)) { Say(FString::Printf(TEXT("The Greenstead deed costs %d coins."), DeedGold)); return false; }
	bGreenDeed = true;
	GreenEnsurePlots();
	Say(TEXT("Greenstead is yours. Sail there from Ventures -> Greenstead. It sits on the Hearthhold island — a farm for making things, not a town storehouse."));
	return true;
}

bool UVenturesFeature::GreenGo()
{
	if (!bGreenDeed) { Say(TEXT("Buy the Greenstead deed first (Ventures -> Greenstead, 2,000 coins).")); return false; }
	if (Run.bOn || Puz.bOn) { Say(TEXT("Finish what you are doing first.")); return false; }
	FVector2D P;
	if (PlayerWebPos(P) && P.X > -700.0) HoldReturn = P;
	const FVector2D C = ZoneC(TEXT("hold"));
	TeleportPlayer(FVector2D(C.X, C.Y + 6.0));
	Say(TEXT("You sail to Greenstead, on the Hearthhold island."));
	return true;
}

void UVenturesFeature::GreenLeave()
{
	HoldLeave();
	Say(TEXT("You sail back to the mainland."));
}

bool UVenturesFeature::GreenBuySeed(const FString& Seed, int32 N)
{
	int32 Price = 0; for (const auto& S : Seeds) if (Seed == S.Seed) Price = S.Price;
	if (Price <= 0 || N <= 0) return false;
	const int32 Cost = Price * N;
	if (!TakeCoins(Cost)) { Say(FString::Printf(TEXT("Those seeds cost %d coins."), Cost)); return false; }
	GiveItem(Seed, N);
	Say(FString::Printf(TEXT("You buy %d %s."), N, *Seed));
	return true;
}

bool UVenturesFeature::GreenPlant(int32 I, const FString& CropId)
{
	if (!bGreenDeed) { Say(TEXT("Buy the Greenstead deed first.")); return false; }
	GreenEnsurePlots();
	const FCrop* C = FindCrop(CropId);
	if (!C || I < 0 || I >= GreenPlotN() || !GreenPlots.IsValidIndex(I)) return false;
	FGreenPlot& P = GreenPlots[I];
	if (!P.Crop.IsEmpty()) { Say(TEXT("That plot is already planted.")); return false; }
	if (SkillLevel(TEXT("Farming")) < C->Lvl) { Say(FString::Printf(TEXT("You need Farming %d to grow %s."), C->Lvl, C->Name)); return false; }
	if (!GreenCanSeason(CropId, P.bCover)) { Say(FString::Printf(TEXT("%s wants %s%s"), C->Name, C->Season, SkillLevel(TEXT("Farming")) >= 50 ? TEXT(" — or a winter cover on this plot (Farming 50).") : TEXT("."))); return false; }
	if (CountItem(C->Seed) < 1) { Say(FString::Printf(TEXT("You need a %s."), C->Seed)); return false; }
	TakeItem(C->Seed, 1);
	P.Crop = CropId; P.PlantedAt = NowMs(); P.Season = SeasonNow();
	const int32 Xp = GreenFarmXp(C->PlantXp, SeasonNow() == C->Season);
	GiveXp(TEXT("Farming"), (float)Xp);
	Say(FString::Printf(TEXT("You plant %s. (+%d Farming)"), C->Name, Xp));
	return true;
}

bool UVenturesFeature::GreenCover(int32 I)
{
	if (SkillLevel(TEXT("Farming")) < 50) { Say(TEXT("A winter cover wants Farming 50 (Homestead).")); return false; }
	GreenEnsurePlots();
	if (!GreenPlots.IsValidIndex(I)) return false;
	int32 Used = 0; for (const FGreenPlot& P : GreenPlots) if (P.bCover) ++Used;
	if (Used >= GreenCoverCap() && !GreenPlots[I].bCover) { Say(FString::Printf(TEXT("You can only cover %d plot%s at a time."), GreenCoverCap(), GreenCoverCap() > 1 ? TEXT("s") : TEXT(""))); return false; }
	GreenPlots[I].bCover = !GreenPlots[I].bCover;
	Say(GreenPlots[I].bCover ? TEXT("You set a winter cover on this plot. One crop can grow out of season here.") : TEXT("You take the cover off."));
	return true;
}

bool UVenturesFeature::GreenHarvest(int32 I)
{
	GreenEnsurePlots();
	if (!GreenRipe(I)) { Say(TEXT("Nothing is ready on that plot.")); return false; }
	FGreenPlot& P = GreenPlots[I];
	const FCrop* C = FindCrop(P.Crop); if (!C) return false;
	const bool bInS = P.Season == C->Season;
	int32 N = 1;
	if (const AAerondelPlayerState* PS = GetPS())
	{
		if (FMath::FRand() < UAerondelRules::Perk(PS, TEXT("farming"), TEXT("farmYield"))) N += 1;
		if (FMath::FRand() < UAerondelRules::Perk(PS, TEXT("farming"), TEXT("rareCrop"))) N += 1;
	}
	GiveItem(C->Item, N); GreenGrewAdd(C->Item, N);
	const int32 Xp = GreenFarmXp(C->HarvestXp, bInS);
	GiveXp(TEXT("Farming"), (float)Xp);
	P.Crop.Reset(); P.PlantedAt = 0; P.Season.Reset();
	Say(FString::Printf(TEXT("You harvest %d %s. (+%d Farming)"), N, C->Item, Xp));
	return true;
}

bool UVenturesFeature::GreenBuyAnimal(const FString& Type)
{
	const FAnimal* A = FindAnimal(Type); if (!A) return false;
	if (SkillLevel(TEXT("Farming")) < A->Lvl) { Say(FString::Printf(TEXT("You need Farming %d to keep a %s."), A->Lvl, A->Name)); return false; }
	if (GreenAnimals.Num() >= GreenAnimalCap()) { Say(FString::Printf(TEXT("Your farm only supports %d animals."), GreenAnimalCap())); return false; }
	if (!TakeCoins(A->Gold)) { Say(FString::Printf(TEXT("A %s costs %d coins."), A->Name, A->Gold)); return false; }
	FGreenAnimal Rec; Rec.Type = Type;
	GreenAnimals.Add(Rec);
	Say(FString::Printf(TEXT("A %s joins the farm."), A->Name));
	return true;
}

bool UVenturesFeature::GreenFeed(int32 I)
{
	if (!GreenAnimals.IsValidIndex(I)) return false;
	FGreenAnimal& A = GreenAnimals[I];
	const FAnimal* D = FindAnimal(A.Type); if (!D) return false;
	if (CountItem(D->Feed) < 1) { Say(FString::Printf(TEXT("You need %s to feed the %s."), D->Feed, D->Name)); return false; }
	if (A.bReady) { Say(TEXT("Collect from them first.")); return false; }
	TakeItem(D->Feed, 1); A.FedAt = NowMs(); A.bReady = false;
	Say(FString::Printf(TEXT("You feed the %s."), D->Name));
	return true;
}

bool UVenturesFeature::GreenCollect(int32 I)
{
	if (!GreenAnimals.IsValidIndex(I)) return false;
	FGreenAnimal& A = GreenAnimals[I];
	const FAnimal* D = FindAnimal(A.Type); if (!D) return false;
	const bool bReady = A.bReady || (A.FedAt && NowMs() - A.FedAt >= D->Time);
	if (!bReady) { Say(TEXT("They are not ready yet.")); return false; }
	GiveItem(D->Produce, 1); GreenGrewAdd(D->Produce, 1);
	GiveXp(TEXT("Farming"), (float)D->Xp);
	A.FedAt = 0; A.bReady = false;
	Say(FString::Printf(TEXT("You collect %s. (+%d Farming)"), D->Produce, D->Xp));
	return true;
}

bool UVenturesFeature::GreenPreserve(const FString& Item)
{
	static const TCHAR* Ok[] = { TEXT("Potato"), TEXT("Cabbage"), TEXT("Tomato"), TEXT("Pumpkin"), TEXT("Winterroot"), TEXT("Egg"), TEXT("Milk"), nullptr };
	bool bOk = false; for (int32 i = 0; Ok[i]; ++i) if (Item == Ok[i]) bOk = true;
	if (!bOk) { Say(TEXT("That is not farm produce you can jar.")); return false; }
	if (CountItem(Item) < 2) { Say(FString::Printf(TEXT("You need 2 %s to make preserves."), *Item)); return false; }
	if (!TakeItem(Item, 2)) return false;
	GreenGrewAdd(Item, -2);
	GiveItem(TEXT("Farm preserves"), 1); GreenGrewAdd(TEXT("Farm preserves"), 1);
	GiveXp(TEXT("Farming"), 20.f);
	Say(FString::Printf(TEXT("You jar farm preserves from %s. (+20 Farming) This is Farming, not Cooking."), *Item));
	return true;
}

bool UVenturesFeature::GreenPlace(const FString& Id)
{
	const FDecor* D = FindDecor(Id); if (!D) return false;
	if (GreenDecor.Num() >= GreenDecorCap()) { Say(FString::Printf(TEXT("The farm only holds %d décor pieces."), GreenDecorCap())); return false; }
	if (CountItem(D->M1) < D->N1 || (D->M2 && CountItem(D->M2) < D->N2)) { Say(FString::Printf(TEXT("You need %d %s%s."), D->N1, D->M1, D->M2 ? *FString::Printf(TEXT(" and %d %s"), D->N2, D->M2) : TEXT(""))); return false; }
	TakeItem(D->M1, D->N1); if (D->M2) TakeItem(D->M2, D->N2);
	FGreenDecor Rec; Rec.Id = Id;
	const int32 Slot = GreenDecor.Num();
	Rec.X = -10.f + (Slot % 5) * 1.4f; Rec.Z = 0.f + (Slot / 5) * 1.4f;
	GreenDecor.Add(Rec);
	GiveXp(TEXT("Construction"), (float)D->Xp);
	Say(FString::Printf(TEXT("You place a %s. (+%d Construction)"), D->Name, D->Xp));
	return true;
}

bool UVenturesFeature::GreenPick(int32 I)
{
	if (!GreenDecor.IsValidIndex(I)) return false;
	const FDecor* D = FindDecor(GreenDecor[I].Id);
	GreenDecor.RemoveAt(I);
	if (D) { GiveItem(D->M1, D->N1); if (D->M2) GiveItem(D->M2, D->N2); }
	Say(FString::Printf(TEXT("You pick up the %s."), D ? D->Name : TEXT("piece")));
	return true;
}

void UVenturesFeature::TickGreen()
{
	if (!bGreenDeed) return;
	for (FGreenAnimal& A : GreenAnimals)
	{
		const FAnimal* D = FindAnimal(A.Type);
		if (D && A.FedAt && !A.bReady && NowMs() - A.FedAt >= D->Time) A.bReady = true;
	}
}

void UVenturesFeature::TabGreen(FGPanelModel& M)
{
	M.Title = FString::Printf(TEXT("Greenstead · Farming %d · %s"), SkillLevel(TEXT("Farming")), *SeasonLabel(SeasonNow()));
	if (!bGreenDeed)
	{
		M.Lines.Add(TEXT("A making-things farm on the Hearthhold island. Town houses are for storage; this is where you grow, tend and jar. The deed is 2,000 coins and does not train Farming — planting does."));
		VAct14(M, FString::Printf(TEXT("Buy the deed (%d coins)"), DeedGold), [this] { GreenBuy(); });
		return;
	}
	GreenEnsurePlots();
	M.Lines.Add(FString::Printf(TEXT("Town houses store things. This farm, and the Hold forge and garden, are where you make them. Crops follow the season; Farming 50 unlocks one winter cover. Décor %d/%d (a little Construction XP). Animals %d/%d."), GreenDecor.Num(), GreenDecorCap(), GreenAnimals.Num(), GreenAnimalCap()));
	if (GreenHome()) VAct14(M, TEXT("Sail home"), [this] { GreenLeave(); }, true, true);
	else VAct14(M, TEXT("Sail to Greenstead"), [this] { GreenGo(); }, true, true);
	M.Lines.Add(TEXT("Seed stall"));
	for (const auto& S : Seeds) VAct14(M, FString::Printf(TEXT("Buy %s (%dc) ×%d"), S.Seed, S.Price, CountItem(S.Seed)), [this, S] { GreenBuySeed(S.Seed, 1); });
	const int32 N = GreenPlotN();
	for (int32 I = 0; I < N && I < GreenPlots.Num(); ++I)
	{
		const FGreenPlot& P = GreenPlots[I];
		const FCrop* C = FindCrop(P.Crop);
		FString St = !C ? TEXT("empty") : GreenRipe(I) ? TEXT("ready") : FString::Printf(TEXT("growing (~%ds)"), FMath::Max(1, (int32)((GreenGrowMs(P.Crop) - (NowMs() - P.PlantedAt)) / 1000)));
		M.Lines.Add(FString::Printf(TEXT("Plot %d — %s%s"), I + 1, C ? *FString::Printf(TEXT("%s · %s"), C->Name, *St) : TEXT("empty"), P.bCover ? TEXT(" · covered") : TEXT("")));
		if (!C) { for (const FCrop& X : Crops) VAct14(M, FString::Printf(TEXT("Plant %s on plot %d"), X.Name, I + 1), [this, I, Id = FString(X.Id)] { GreenPlant(I, Id); }); }
		else if (GreenRipe(I)) VAct14(M, FString::Printf(TEXT("Harvest plot %d"), I + 1), [this, I] { GreenHarvest(I); });
		if (SkillLevel(TEXT("Farming")) >= 50) VAct14(M, FString::Printf(TEXT("%s on plot %d"), P.bCover ? TEXT("Remove cover") : TEXT("Winter cover"), I + 1), [this, I] { GreenCover(I); });
	}
	M.Lines.Add(TEXT("Animals"));
	if (GreenAnimals.Num() == 0) M.Lines.Add(TEXT("No animals yet."));
	for (int32 I = 0; I < GreenAnimals.Num(); ++I)
	{
		const FGreenAnimal& A = GreenAnimals[I];
		const FAnimal* D = FindAnimal(A.Type); if (!D) continue;
		const bool bReady = A.bReady || (A.FedAt && NowMs() - A.FedAt >= D->Time);
		M.Lines.Add(FString::Printf(TEXT("%s — %s"), D->Name, bReady ? TEXT("ready") : A.FedAt ? TEXT("digesting") : TEXT("hungry")));
		if (bReady) VAct14(M, FString::Printf(TEXT("Collect %s"), D->Produce), [this, I] { GreenCollect(I); });
		else VAct14(M, FString::Printf(TEXT("Feed %s %s"), D->Name, D->Feed), [this, I] { GreenFeed(I); });
	}
	for (const FAnimal& A : Animals) VAct14(M, FString::Printf(TEXT("Buy a %s (%d coins, Farming %d)"), A.Name, A.Gold, A.Lvl), [this, Id = FString(A.Id)] { GreenBuyAnimal(Id); });
	M.Lines.Add(TEXT("Preserves (Farming, not Cooking)"));
	static const TCHAR* Jar[] = { TEXT("Potato"), TEXT("Cabbage"), TEXT("Tomato"), TEXT("Pumpkin"), TEXT("Winterroot"), TEXT("Egg"), TEXT("Milk"), nullptr };
	int32 Jars = 0;
	for (int32 i = 0; Jar[i]; ++i) if (CountItem(Jar[i]) >= 2) { VAct14(M, FString::Printf(TEXT("Jar 2 %s"), Jar[i]), [this, Item = FString(Jar[i])] { GreenPreserve(Item); }); ++Jars; }
	if (!Jars) M.Lines.Add(TEXT("Harvest two of the same produce first."));
	M.Lines.Add(TEXT("Décor (a little Construction XP)"));
	for (const FDecor& D : Decors)
	{
		const FString Need = D.M2 ? FString::Printf(TEXT("%d %s, %d %s"), D.N1, D.M1, D.N2, D.M2) : FString::Printf(TEXT("%d %s"), D.N1, D.M1);
		VAct14(M, FString::Printf(TEXT("Place %s (%s)"), D.Name, *Need), [this, Id = FString(D.Id)] { GreenPlace(Id); });
	}
	for (int32 I = 0; I < GreenDecor.Num(); ++I)
	{
		const FDecor* D = FindDecor(GreenDecor[I].Id);
		VAct14(M, FString::Printf(TEXT("Pick up %s"), D ? D->Name : TEXT("piece")), [this, I] { GreenPick(I); });
	}
}
