#include "Features/VenturesFeature.h"
#include "UI/AerondelPanels.h"
#include "Core/AerondelPlayerState.h"
#include "Core/PlayerStatsComponent.h"
#include "Inventory/EquipmentComponent.h"
#include "Data/ItemDef.h"
#include "Data/AerondelTypes.h"
#include "World/AerondelScalability.h"
#include "Combat/HealthComponent.h"
#include "GameFramework/Character.h"
#include "Dom/JsonObject.h"

// Ventures group 22 (web adapt/leychem.js): a small, readable ruleset — fire, wind and metal.
// On Low this is a state, not a particle field. Town houses and the farm will not quietly burn down.

namespace VenturesChem
{
	static constexpr float Cell = 2.f;
	static FIntPoint KeyOf(float X, float Z)
	{
		return FIntPoint(FMath::RoundToInt(X / Cell) * (int32)Cell, FMath::RoundToInt(Z / Cell) * (int32)Cell);
	}
	static bool GfxLow()
	{
		const EAerondelTier T = UAerondelScalability::GetSavedTier();
		return T == EAerondelTier::Low || T == EAerondelTier::Quest;
	}
	static int32 FireCap() { return GfxLow() ? 8 : 24; }
	static void VAct22(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
}
using namespace VenturesChem;

void UVenturesFeature::ResetChem()
{
	bChemSeen = false;
	LeyFires.Reset();
	ChemAcc = 0.f;
}

void UVenturesFeature::SaveChem(FJsonObject& A) const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	S->SetBoolField(TEXT("seen"), bChemSeen);
	A.SetObjectField(TEXT("chem"), S);
}

void UVenturesFeature::LoadChem(const FJsonObject& A)
{
	if (const TSharedPtr<FJsonObject> S = JObj(A, TEXT("chem"))) bChemSeen = JBool(*S, TEXT("seen"));
}

FString UVenturesFeature::LeyWeather() const
{
	// A light cycle so rain and storms can be felt without a full weather sim: 4 min clear, then drizzle, then a storm.
	const int64 Slot = (NowMs() / 240000) % 6;
	if (Slot == 4) return TEXT("drizzle");
	if (Slot == 5) return TEXT("storm");
	return TEXT("clear");
}

bool UVenturesFeature::LeyIsRain() const
{
	const FString W = LeyWeather();
	return W == TEXT("drizzle") || W == TEXT("storm");
}

bool UVenturesFeature::LeyIsStorm() const { return LeyWeather() == TEXT("storm"); }

bool UVenturesFeature::LeyProtected(float X, float Z) const
{
	if (NearTownHouse(FVector2D(X, Z), 14.f)) return true;
	if (GreenHome() || HoldHome() || OnPad(TEXT("hold")) || OnPad(TEXT("green"))) return true;
	if (Run.bOn && !FrameOn(X, Z)) return true;
	return false;
}

bool UVenturesFeature::LeyCanBurn(float X, float Z, const FString& Kind) const
{
	if (LeyProtected(X, Z) && Kind == TEXT("grass")) return false;
	if (Kind == TEXT("wood")) return FrameFlammable(X, Z) || !LeyProtected(X, Z);
	return !LeyProtected(X, Z);
}

bool UVenturesFeature::LeyOnFire(float X, float Z) const
{
	return LeyFires.Contains(KeyOf(X, Z));
}

bool UVenturesFeature::LeyIgnite(float X, float Z, const FString& Kind, const FString& Src)
{
	if (!LeyCanBurn(X, Z, Kind)) return false;
	const FIntPoint K = KeyOf(X, Z);
	if (LeyFires.Num() >= FireCap() && !LeyFires.Contains(K)) return false;
	if (FLeyFire* E = LeyFires.Find(K)) { E->Heat = FMath::Min(3.f, E->Heat + 1.f); return true; }
	FLeyFire F; F.Kind = Kind; F.Heat = 1.f; F.X = (float)K.X; F.Z = (float)K.Y; F.Src = Src.IsEmpty() ? TEXT("spark") : Src;
	LeyFires.Add(K, F);
	bChemSeen = true;
	return true;
}

bool UVenturesFeature::LeyExtinguish(float X, float Z)
{
	return LeyFires.Remove(KeyOf(X, Z)) > 0;
}

void UVenturesFeature::LeyPushWind(float X, float Z, float Fx, float Fz)
{
	TArray<FLeyFire> List; LeyFires.GenerateValueArray(List);
	for (const FLeyFire& F : List)
	{
		if (FVector2D::Distance(FVector2D(F.X, F.Z), FVector2D(X, Z)) > 10.f) continue;
		LeyIgnite(F.X + FMath::Sign(Fx) * Cell, F.Z + FMath::Sign(Fz) * Cell, F.Kind, TEXT("wind"));
	}
}

void UVenturesFeature::LeyOnCry()
{
	FVector2D P; if (!PlayerWebPos(P)) return;
	const AAerondelPlayerState* PS = GetPS();
	const APawn* Pawn = PS ? PS->GetPawn() : nullptr;
	const FVector F = Pawn ? Pawn->GetActorForwardVector() : FVector::ForwardVector;
	LeyPushWind(P.X, P.Y, F.X, F.Y);
}

bool UVenturesFeature::LeyWearingMetal() const
{
	const AAerondelPlayerState* PS = GetPS();
	if (!PS || !PS->Equipment) return false;
	static const EEquipSlot Slots[] = { EEquipSlot::Head, EEquipSlot::Body, EEquipSlot::Legs, EEquipSlot::Weapon, EEquipSlot::Shield, EEquipSlot::Hands, EEquipSlot::Feet };
	for (EEquipSlot S : Slots)
	{
		const UItemDef* I = PS->Equipment->GetEquipped(S);
		if (!I) continue;
		const FString N = I->DisplayName.ToString();
		if (N.Contains(TEXT("Iron")) || N.Contains(TEXT("Steel")) || N.Contains(TEXT("Bluesteel")) || N.Contains(TEXT("Verdite"))
			|| N.Contains(TEXT("Cobalt")) || N.Contains(TEXT("Emberite")) || N.Contains(TEXT("mith"), ESearchCase::IgnoreCase)
			|| N.Contains(TEXT("plate"), ESearchCase::IgnoreCase)) return true;
	}
	return false;
}

void UVenturesFeature::LeyStrike(float X, float Z)
{
	Say(TEXT("Lightning finds the metal."));
	FVector2D P; PlayerWebPos(P);
	if (FVector2D::Distance(P, FVector2D(X, Z)) < 3.f)
	{
		if (AAerondelPlayerState* PS = GetPS()) if (PS->Health && PS->Health->GetCurrent() > 1)
			HurtPlayer(FMath::Max(1.f, PS->Health->GetMax() * 0.04f), TEXT("ley-lightning"));
	}
	LeyIgnite(X, Z, FrameFlammable(X, Z) ? TEXT("wood") : TEXT("grass"), TEXT("lightning"));
}

void UVenturesFeature::TickChem()
{
	ChemAcc += 0.6f;
	if (ChemAcc < 0.8f) return;
	ChemAcc = 0.f;
	if (LeyIsRain())
	{
		TArray<FLeyFire> List; LeyFires.GenerateValueArray(List);
		for (const FLeyFire& F : List) if (FMath::FRand() < 0.7f * LeyRainMul()) LeyExtinguish(F.X, F.Z);
	}
	TArray<FLeyFire> List; LeyFires.GenerateValueArray(List);
	int32 Grew = 0;
	const int32 MaxGrow = GfxLow() ? 1 : 2;
	static const int32 Dir[4][2] = { {2, 0}, {-2, 0}, {0, 2}, {0, -2} };
	for (FLeyFire& F : List)
	{
		if (Grew >= MaxGrow) break;
		FLeyFire* Live = LeyFires.Find(KeyOf(F.X, F.Z));
		if (!Live) continue;
		Live->Heat -= 0.15f;
		if (Live->Heat <= 0.f) { LeyExtinguish(F.X, F.Z); continue; }
		const int32 D = FMath::RandRange(0, 3);
		if (FMath::FRand() < 0.45f * LeySpreadMul() && LeyIgnite(F.X + Dir[D][0], F.Z + Dir[D][1], Live->Kind, TEXT("spread"))) Grew++;
	}
	if (LeyIsStorm() && FMath::FRand() < 0.08f)
	{
		FVector2D P; PlayerWebPos(P);
		if (LeyWearingMetal()) LeyStrike(P.X, P.Y);
		else { FVector2D M; if (FrameMetal(M)) LeyStrike(M.X, M.Y); }
	}
	// Storm attunement can also shove nearby flames, the way a Wordcry does.
	if (AttuneEl == TEXT("storm") && LeyFires.Num())
	{
		FVector2D P; if (PlayerWebPos(P))
		{
			const AAerondelPlayerState* PS = GetPS();
			const APawn* Pawn = PS ? PS->GetPawn() : nullptr;
			const FVector F = Pawn ? Pawn->GetActorForwardVector() : FVector::ForwardVector;
			if (FMath::FRand() < 0.25f) LeyPushWind(P.X, P.Y, F.X, F.Y);
		}
	}
}

void UVenturesFeature::TabChem(FGPanelModel& M)
{
	const int32 N = LeyFires.Num();
	const FString W = LeyWeather();
	M.Title = FString::Printf(TEXT("Leychemistry · %d flame%s"), N, N == 1 ? TEXT("") : TEXT("s"));
	M.Lines.Add(TEXT("A small ruleset: fire spreads on wild grass and on wooden builds on your claimed plot; rain puts it out; wind shoves it; metal draws a storm. Town houses and the farm are protected. On Low this is a state, not a particle field."));
	FString Sky = TEXT("The sky is quiet.");
	if (LeyIsRain() && !LeyIsStorm()) Sky = TEXT("It is raining — flames are dying back.");
	else if (LeyIsStorm()) Sky = TEXT("A storm is up — metal may draw a strike.");
	M.Lines.Add(FString::Printf(TEXT("%s %s"), *Sky, N ? *FString::Printf(TEXT("Burning cells: %d/%d."), N, FireCap()) : TEXT("No flames right now.")));
}
