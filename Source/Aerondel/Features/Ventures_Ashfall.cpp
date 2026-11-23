#include "Features/VenturesFeature.h"
#include "Features/VenturesTowns.h"
#include "UI/AerondelPanels.h"
#include "Combat/AerondelMob.h"
#include "Data/GameDatabase.h"
#include "Data/MobDef.h"
#include "EngineUtils.h"
#include "Dom/JsonObject.h"

// Ventures group 39 (web adapt/ashfall.js): a skippable, telegraphed Ley flare in a wild region.
// Not a Burst Tide, not a Warband Siege, never a strike on a town, a Townstead, Greenstead or Claimridge.
// Save key is "ash", not "flare" — Overflare already owns adapt.flare.
// In-progress flares end safely on load and are never resumed mid-flare.
// On Low: a sky tint and a spawn-table swap, no blast mesh and no extra lights.

namespace VenturesAshfall
{
	static constexpr int64 AshfallDurationMs = 12ll * 60 * 1000, AshfallWarnMs = 3ll * 60 * 1000;
	static constexpr int64 AshfallCadenceMin = 22ll * 60 * 1000, AshfallCadenceSpan = 12ll * 60 * 1000;
	static constexpr float AshfallTownR = 16.f + 8.f, AshfallRegionPad = 8.f, AshfallDefaultR = 40.f, AshfallGatherR = 5.f;
	static constexpr float AshfallPlotX = 22.f, AshfallPlotZ = 78.f, AshfallPlotHalf = 12.f;
	struct FAshfallReg { const TCHAR* Id; const TCHAR* Name; float X, Z, R; };
	static const FAshfallReg AshfallRegions[] = {
		{ TEXT("rimeholt"),    TEXT("Rimeholt Tundra"),      -240.f, -347.f, 40.f },
		{ TEXT("skyreach"),    TEXT("Skyreach Highlands"),    245.f, -342.f, 40.f },
		{ TEXT("ashveil"),     TEXT("Ashveil Wastes"),        365.f, -132.f, 40.f },
		{ TEXT("sunscour"),    TEXT("Sunscour Dunes"),        370.f,  128.f, 40.f },
		{ TEXT("vinehollow"),  TEXT("Verdant Tangle"),        220.f,  368.f, 40.f },
		{ TEXT("mirelow"),     TEXT("Mirelow Fen"),          -350.f,  193.f, 40.f },
		{ TEXT("grimhollow"),  TEXT("Grimhollow Barrens"),    550.f,    8.f, 40.f },
		{ TEXT("thornmere"),   TEXT("Thornmere Wilds"),       390.f,  408.f, 40.f },
		{ TEXT("hollowfrost"), TEXT("Hollowfrost Expanse"),    10.f,  548.f, 40.f },
		{ TEXT("stormcrown"),  TEXT("Stormcrown Peaks"),     -380.f,  408.f, 40.f },
		{ TEXT("duskmarsh"),   TEXT("Duskmarsh Deeps"),      -535.f,   -2.f, 40.f },
		{ TEXT("glasswind"),   TEXT("Glasswind Erg"),        -400.f, -432.f, 40.f },
		{ TEXT("emberdeep"),   TEXT("Emberdeep Caldera"),      10.f, -537.f, 40.f },
		{ TEXT("sundered"),    TEXT("The Sundered Reach"),    430.f, -422.f, 40.f },
	};
	static const TCHAR* SwapOf(const FString& Type)
	{
		if (Type == TEXT("Wolf")) return TEXT("Goblin");
		if (Type == TEXT("Goblin")) return TEXT("Skeleton");
		if (Type == TEXT("Cow")) return TEXT("Wolf");
		if (Type == TEXT("Giant Spider")) return TEXT("Skeleton");
		return nullptr;
	}
	static int32 FindAshveil()
	{
		for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(AshfallRegions); ++i) if (FCString::Strcmp(AshfallRegions[i].Id, TEXT("ashveil")) == 0) return i;
		return 0;
	}
	static void VAct39(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
}
using namespace VenturesAshfall;

FString UVenturesFeature::AshForbidden(float X, float Z) const
{
	for (const VenturesTowns::FTownPt& T : VenturesTowns::All) // every town (the web flare only checks the five main ones; see review notes)
		if (VenturesTowns::Dist(T, X, Z) < AshfallTownR) return T.Name;
	if (NearTownHouse(FVector2D(X, Z), 8.f)) return TEXT("a Townstead");
	const FVector2D Hold = ZoneC(TEXT("hold"));
	if (FMath::Abs(X - Hold.X) < 20.f && FMath::Abs(Z - Hold.Y) < 16.f) return TEXT("Greenstead");
	if (FMath::Abs(X - AshfallPlotX) <= AshfallPlotHalf && FMath::Abs(Z - AshfallPlotZ) <= AshfallPlotHalf) return TEXT("Claimridge");
	return FString();
}

bool UVenturesFeature::AshInRegion(int32 Region, float X, float Z) const
{
	if (Region < 0 || Region >= (int32)UE_ARRAY_COUNT(AshfallRegions)) return false;
	const FAshfallReg& R = AshfallRegions[Region];
	return FMath::Sqrt(FMath::Square(X - R.X) + FMath::Square(Z - R.Z)) < R.R + AshfallRegionPad;
}

int32 UVenturesFeature::AshPickRegion() const
{
	TArray<int32> Ok;
	for (int32 i = 0; i < (int32)UE_ARRAY_COUNT(AshfallRegions); ++i)
		if (AshForbidden(AshfallRegions[i].X, AshfallRegions[i].Z).IsEmpty()) Ok.Add(i);
	const int32 Pref = FindAshveil();
	if (Ok.Contains(Pref)) return Pref;
	return Ok.Num() ? Ok[0] : -1;
}

bool UVenturesFeature::AshActiveIn(const FString& Name) const
{
	if (!bAshActive || !AshfallRegions[0].Name || AshRegion < 0 || AshRegion >= (int32)UE_ARRAY_COUNT(AshfallRegions)) return false;
	return Name == AshfallRegions[AshRegion].Name;
}

FString UVenturesFeature::AshSwapName(const FString& Type, float X, float Z) const
{
	if (!bAshActive || !AshInRegion(AshRegion, X, Z)) return Type;
	if (const TCHAR* S = SwapOf(Type)) return S;
	return Type;
}

void UVenturesFeature::AshMarkMob(AAerondelMob* Mob)
{
	if (!bAshActive || !Mob || Mob->IsDead()) return;
	const FVector Loc = Mob->GetActorLocation();
	if (!AshInRegion(AshRegion, Loc.X / 100.f, Loc.Y / 100.f)) return;
	if (const UMobDef* D = Mob->Definition)
	{
		if (D->bBoss || D->DisplayName.ToString().Contains(TEXT("Skywyrm")) || D->DisplayName.ToString().Contains(TEXT("Colossus"))) return;
	}
	AshMarked.AddUnique(Mob);
}

void UVenturesFeature::ResetAshfall()
{
	AshSeen = 0;
	AshLast = 0;
	AshNext = NowMs() + AshfallCadenceMin;
	AshRegion = -1;
	AshUntil = AshOfferUntil = 0;
	bAshOffered = bAshActive = bAshNode = bAshWyrm = false;
	AshNodeX = AshNodeZ = 0.f;
	AshMarked.Reset();
}

void UVenturesFeature::SaveAshfall(FJsonObject& A) const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	S->SetNumberField(TEXT("last"), (double)AshLast);
	S->SetNumberField(TEXT("started"), 0); // never persist a mid-flare
	S->SetNumberField(TEXT("seen"), AshSeen);
	S->SetNumberField(TEXT("next"), (double)AshNext);
	A.SetObjectField(TEXT("ash"), S);
}

void UVenturesFeature::LoadAshfall(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> S = JObj(A, TEXT("ash"));
	if (!S)
	{
		AshNext = NowMs() + AshfallCadenceMin;
		return;
	}
	AshLast = (int64)JInt(*S, TEXT("last"));
	AshSeen = FMath::Max(0, (int32)JInt(*S, TEXT("seen")));
	AshNext = (int64)JInt(*S, TEXT("next"));
	bAshOffered = bAshActive = bAshNode = false;
	AshMarked.Reset();
	AshRegion = -1;
	if (JInt(*S, TEXT("started")) > 0)
	{
		Say(TEXT("The Ashfall faded while you were away. The region is ordinary again."));
	}
	if (AshNext <= 0) AshNext = NowMs() + AshfallCadenceMin;
}

void UVenturesFeature::AshEnd(const FString& Why)
{
	const FString Name = (AshRegion >= 0 && AshRegion < (int32)UE_ARRAY_COUNT(AshfallRegions)) ? FString(AshfallRegions[AshRegion].Name) : FString();
	bAshNode = false;
	bAshActive = false;
	bAshOffered = false;
	bAshWyrm = false;
	AshMarked.Reset();
	AshRegion = -1;
	AshUntil = AshOfferUntil = 0;
	AshLast = NowMs();
	AshNext = NowMs() + AshfallCadenceMin + (int64)FMath::RandRange(0, (int32)AshfallCadenceSpan);
	if (Why == TEXT("load")) Say(TEXT("The Ashfall faded while you were away. The region is ordinary again."));
	else if (!Name.IsEmpty()) Say(FString::Printf(TEXT("The Ashfall over %s fades. The region is ordinary again."), *Name));
}

bool UVenturesFeature::AshPass()
{
	if (!bAshOffered) return false;
	const FString Name = (AshRegion >= 0 && AshRegion < (int32)UE_ARRAY_COUNT(AshfallRegions)) ? FString(AshfallRegions[AshRegion].Name) : FString();
	bAshOffered = false;
	AshOfferUntil = 0;
	AshRegion = -1;
	AshNext = NowMs() + AshfallCadenceMin / 2;
	Say(FString::Printf(TEXT("You let the Ashfall over %s pass. Nothing is owed."), *Name));
	return true;
}

bool UVenturesFeature::AshOffer()
{
	if (bAshActive || bAshOffered) return false;
	int32 I = AshPickRegion();
	if (I < 0) return false;
	// Do not start a warning on top of a Burst Tide in the same region.
	if (TideRegion >= 0 && TideRegions.IsValidIndex(TideRegion) && TideRegions[TideRegion].Name == AshfallRegions[I].Name)
	{
		for (int32 j = 0; j < (int32)UE_ARRAY_COUNT(AshfallRegions); ++j)
			if (j != I && AshForbidden(AshfallRegions[j].X, AshfallRegions[j].Z).IsEmpty() && TideRegions[TideRegion].Name != AshfallRegions[j].Name) { I = j; break; }
		if (TideRegions.IsValidIndex(TideRegion) && TideRegions[TideRegion].Name == AshfallRegions[I].Name) return false;
	}
	AshRegion = I;
	bAshOffered = true;
	AshOfferUntil = NowMs() + AshfallWarnMs;
	Say(FString::Printf(TEXT("An Ashfall is gathering over %s. You can begin it, or let it pass — you are not punished for leaving it."), AshfallRegions[I].Name));
	return true;
}

bool UVenturesFeature::AshBegin()
{
	int32 I = -1;
	if (bAshOffered && AshRegion >= 0) I = AshRegion;
	if (I < 0)
	{
		FVector2D P;
		if (PlayerWebPos(P))
		{
			if (const FString Why = AshForbidden(P.X, P.Y); !Why.IsEmpty())
			{
				Say(FString::Printf(TEXT("An Ashfall cannot settle on %s. Walk into the wilds first."), *Why));
				return false;
			}
			for (int32 j = 0; j < (int32)UE_ARRAY_COUNT(AshfallRegions); ++j) if (AshInRegion(j, P.X, P.Y)) { I = j; break; }
		}
		if (I < 0) { Say(TEXT("Carve the totem in a wild region to call an Ashfall.")); return false; }
	}
	if (const FString Why = AshForbidden(AshfallRegions[I].X, AshfallRegions[I].Z); !Why.IsEmpty())
	{
		Say(TEXT("An Ashfall cannot settle on a town, a Townstead, Greenstead or Claimridge."));
		return false;
	}
	if (TideRegion >= 0 && TideRegions.IsValidIndex(TideRegion) && TideRegions[TideRegion].Name == AshfallRegions[I].Name)
	{
		Say(TEXT("A Burst Tide is already surging there. The Ashfall will not stack on it."));
		return false;
	}
	if (bAshActive) { Say(TEXT("An Ashfall is already burning in the wilds.")); return false; }
	bAshOffered = false;
	bAshActive = true;
	AshRegion = I;
	AshUntil = NowMs() + AshfallDurationMs;
	AshSeen++;
	AshLast = NowMs();
	bAshNode = true;
	AshNodeX = AshfallRegions[I].X + 6.f;
	AshNodeZ = AshfallRegions[I].Z + 6.f;
	AshMarked.Reset();
	if (UWorld* W = GetWorld())
	{
		for (TActorIterator<AAerondelMob> It(W); It; ++It)
		{
			const FVector Loc = It->GetActorLocation();
			const float X = Loc.X / 100.f, Z = Loc.Y / 100.f; // WebToWorld is X*100, Z*100 as Unreal Y
			if (AshInRegion(I, X, Z)) AshMarkMob(*It);
		}
	}
	bAshWyrm = false;
	if (!WyrmActive() && !bWyrmOffered && WyrmOffer()) bAshWyrm = true;
	Say(FString::Printf(TEXT("Ashfall over %s for about 12 minutes. Spawns shift, an Ash crystal may be gathered, and a Wildmark may settle after a marked foe. You can walk away."), AshfallRegions[I].Name));
	return true;
}

bool UVenturesFeature::AshGo()
{
	if (!bAshOffered) { Say(TEXT("No Ashfall is gathering right now.")); return false; }
	return AshBegin();
}

bool UVenturesFeature::AshCall()
{
	if (AshBegin())
	{
		Say(TEXT("The Ash-totem gathers a flare in the wilds."));
		return true;
	}
	return false;
}

bool UVenturesFeature::AshGather()
{
	if (!bAshActive || !bAshNode) { Say(TEXT("There is no Ash crystal to gather.")); return false; }
	FVector2D P; if (!PlayerWebPos(P)) return false;
	if (FMath::Sqrt(FMath::Square(P.X - AshNodeX) + FMath::Square(P.Y - AshNodeZ)) > AshfallGatherR) { Say(TEXT("Stand closer to the Ash crystal.")); return false; }
	bAshNode = false;
	GiveItem(TEXT("Ash crystal"), 1);
	Say(TEXT("You gather an Ash crystal from the flare."));
	return true;
}

bool UVenturesFeature::AshCarve()
{
	if (SkillLevel(TEXT("Construction")) < 10) { Say(TEXT("You need Construction 10 to carve an Ash-totem.")); return false; }
	if (CountItem(TEXT("Logs")) < 3 || CountItem(TEXT("Clear essence")) < 1) { Say(TEXT("You need 3 Logs and 1 Clear essence.")); return false; }
	if (!TakeItem(TEXT("Logs"), 3) || !TakeItem(TEXT("Clear essence"), 1)) return false;
	GiveItem(TEXT("Ash-totem"), 1);
	GiveXp(TEXT("Construction"), 30.f);
	Say(TEXT("You carve an Ash-totem. Use it in the wilds to call an Ashfall."));
	return true;
}

void UVenturesFeature::TickAshfall()
{
	const int64 T = NowMs();
	if (!bAshOffered && !bAshActive && T > AshNext) AshOffer();
	if (bAshOffered && T > AshOfferUntil) AshPass();
	if (bAshActive && T > AshUntil) AshEnd(TEXT("time"));
}

void UVenturesFeature::DiedAshfall(AAerondelMob* Mob, bool bByPlayer)
{
	if (!bAshActive || !bByPlayer || !Mob) return;
	bool bMarked = false;
	for (int32 i = AshMarked.Num() - 1; i >= 0; --i)
	{
		if (!AshMarked[i].IsValid()) { AshMarked.RemoveAt(i); continue; }
		if (AshMarked[i].Get() == Mob) { bMarked = true; AshMarked.RemoveAt(i); break; }
	}
	if (!bMarked) return;
	if (FMath::FRand() > 0.2f) return;
	TArray<FString> Opts;
	for (const TCHAR* Id : { TEXT("nighteye"), TEXT("kindled"), TEXT("deeplung") })
		if (!MarkHeld.Contains(Id)) Opts.Add(Id);
	if (!Opts.Num()) return;
	if (MarkGrant(Opts[FMath::RandRange(0, Opts.Num() - 1)]))
		Say(TEXT("The marked foe leaves a Wildmark on you."));
}

void UVenturesFeature::TabAshfall(FGPanelModel& M)
{
	M.Title = TEXT("Ashfall");
	M.Lines.Add(TEXT("A skippable Ley flare in a wild region. Not a Burst Tide, not a siege, and never a strike on a town, a Townstead, Greenstead or Claimridge. On Low this is a sky tint and a spawn swap, no blast mesh and no extra lights."));
	M.Lines.Add(FString::Printf(TEXT("Flares seen: %d"), AshSeen));
	if (bAshActive && AshRegion >= 0 && AshRegion < (int32)UE_ARRAY_COUNT(AshfallRegions))
	{
		const int32 MinLeft = FMath::Max(1, (int32)((AshUntil - NowMs() + 59999) / 60000));
		M.Lines.Add(FString::Printf(TEXT("Ashfall in %s — %d minutes. You can walk away."), AshfallRegions[AshRegion].Name, MinLeft));
		if (bAshNode) VAct39(M, TEXT("Gather the Ash crystal"), [this] { AshGather(); });
	}
	else if (bAshOffered && AshRegion >= 0 && AshRegion < (int32)UE_ARRAY_COUNT(AshfallRegions))
	{
		M.Lines.Add(FString::Printf(TEXT("An Ashfall is gathering over %s. Begin it, or let it pass. Nothing is owed if you leave it."), AshfallRegions[AshRegion].Name));
		VAct39(M, TEXT("Begin"), [this] { AshGo(); });
		VAct39(M, TEXT("Not now"), [this] { AshPass(); });
	}
	else
	{
		M.Lines.Add(TEXT("No flare is gathering right now. Carve an Ash-totem (Construction) to call one in the wilds, or wait for a warning."));
	}
	VAct39(M, TEXT("Carve Ash-totem (Construction 10, 3 Logs, 1 Clear essence)"), [this] { AshCarve(); });
	if (CountItem(TEXT("Ash-totem")) > 0) VAct39(M, TEXT("Call with Ash-totem"), [this]
	{
		if (CountItem(TEXT("Ash-totem")) < 1) { Say(TEXT("You need an Ash-totem.")); return; }
		if (AshCall() && TakeItem(TEXT("Ash-totem"), 1)) { /* consumed */ }
	});
}
