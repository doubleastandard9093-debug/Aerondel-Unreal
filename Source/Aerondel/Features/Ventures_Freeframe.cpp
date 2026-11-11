#include "Features/VenturesFeature.h"
#include "Features/FreeframePiece.h"
#include "UI/AerondelPanels.h"
#include "Core/AerondelPlayerState.h"
#include "Rules/AerondelRules.h"
#include "World/AerondelScalability.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// Ventures group 23 (web adapt/freeframe.js): piece-by-piece building with a support rule, only on a claimed wild plot.
// This is the main Construction grind. Not a Townstead and not Greenstead.

namespace VenturesFrame
{
	static constexpr float PlotX = 22.f, PlotZ = 78.f, PlotHalf = 12.f, Grid = 1.5f;
	static constexpr int32 DeedGold = 3000;
	struct FKind
	{
		const TCHAR* Id; const TCHAR* Name;
		const TCHAR* MatA; int32 NA; const TCHAR* MatB; int32 NB;
		int32 Xp, Hp, Lvl; bool bWood, bThatch, bMetal, bSupport; const TCHAR* Tier;
		float R, G, B; float HeightM;
	};
	static const FKind Kinds[] = {
		{ TEXT("foundation"), TEXT("Foundation"),        TEXT("Logs"), 4, nullptr, 0, 28, 40, 1,  true,  false, false, true,  TEXT("wood"),       0.54f, 0.35f, 0.16f, 0.4f },
		{ TEXT("wall"),       TEXT("Wall"),              TEXT("Logs"), 3, nullptr, 0, 22, 28, 1,  true,  false, false, false, TEXT("wood"),       0.54f, 0.35f, 0.16f, 1.6f },
		{ TEXT("floor"),      TEXT("Floor"),             TEXT("Logs"), 3, nullptr, 0, 22, 24, 1,  true,  false, false, false, TEXT("wood"),       0.54f, 0.35f, 0.16f, 0.4f },
		{ TEXT("roof"),       TEXT("Roof"),              TEXT("Logs"), 3, TEXT("Soft leather"), 1, 26, 20, 10, true,  true,  false, false, TEXT("thatch"),     0.78f, 0.63f, 0.31f, 0.5f },
		{ TEXT("palisade"),   TEXT("Palisade"),          TEXT("Logs"), 5, nullptr, 0, 30, 36, 20, true,  false, false, false, TEXT("wood"),       0.54f, 0.35f, 0.16f, 1.6f },
		{ TEXT("stonewall"),  TEXT("Stone wall"),        TEXT("Stone slab"), 2, TEXT("Logs"), 1, 36, 50, 40, false, false, false, false, TEXT("stone"),      0.48f, 0.48f, 0.45f, 1.6f },
		{ TEXT("plate"),      TEXT("Reinforced plate"),  TEXT("Iron bar"), 2, TEXT("Stone slab"), 1, 48, 70, 70, false, false, true,  false, TEXT("reinforced"), 0.54f, 0.54f, 0.59f, 1.6f },
	};
	static const FKind* FindKind(const FString& Id) { for (const FKind& K : Kinds) if (Id == K.Id) return &K; return nullptr; }
	static bool InPlot(float X, float Z) { return FMath::Abs(X - PlotX) <= PlotHalf && FMath::Abs(Z - PlotZ) <= PlotHalf; }
	static int32 GxOf(float X) { return FMath::RoundToInt((X - PlotX) / Grid); }
	static int32 GzOf(float Z) { return FMath::RoundToInt((Z - PlotZ) / Grid); }
	static float WorldX(int32 Gx) { return PlotX + Gx * Grid; }
	static float WorldZ(int32 Gz) { return PlotZ + Gz * Grid; }
	static bool GfxLow()
	{
		const EAerondelTier T = UAerondelScalability::GetSavedTier();
		return T == EAerondelTier::Low || T == EAerondelTier::Quest;
	}
	static void VAct23(FGPanelModel& M, const FString& L, TFunction<void()> F, bool bEnabled = true, bool bClose = false)
	{ FGPanelAction A; A.Label = L; A.Run = MoveTemp(F); A.bEnabled = bEnabled; A.bClose = bClose; M.Actions.Add(MoveTemp(A)); }
}
using namespace VenturesFrame;

void UVenturesFeature::FrameClearMeshes()
{
	for (AActor* A : FrameMeshes) if (IsValid(A)) A->Destroy();
	FrameMeshes.Reset();
}

const FFramePiece* UVenturesFeature::FrameAt(int32 Gx, int32 Gz) const
{
	for (const FFramePiece& P : FrameList) if (P.Gx == Gx && P.Gz == Gz) return &P;
	return nullptr;
}

FFramePiece* UVenturesFeature::FrameAtMut(int32 Gx, int32 Gz)
{
	for (FFramePiece& P : FrameList) if (P.Gx == Gx && P.Gz == Gz) return &P;
	return nullptr;
}

int32 UVenturesFeature::FrameCap() const
{
	const int32 Soft = 20 + SkillLevel(TEXT("Construction")) / 40 + FMath::RoundToInt(UAerondelRules::Perk(GetPS(), FName(TEXT("Construction")), FName(TEXT("pieceCap"))));
	return FMath::Min(GfxLow() ? 28 : 48, Soft);
}

int32 UVenturesFeature::FrameRange() const
{
	return 3 + FMath::RoundToInt(UAerondelRules::Perk(GetPS(), FName(TEXT("Construction")), FName(TEXT("supportRange"))) * 8.f);
}

TSet<FIntPoint> UVenturesFeature::FrameSupported() const
{
	TSet<FIntPoint> Have;
	TArray<FIntPoint> Q;
	TArray<int32> Dist;
	const int32 R = FrameRange();
	for (const FFramePiece& P : FrameList)
		if (const FKind* K = FindKind(P.Kind); K && K->bSupport)
		{
			const FIntPoint C(P.Gx, P.Gz);
			if (!Have.Contains(C)) { Have.Add(C); Q.Add(C); Dist.Add(0); }
		}
	for (int32 i = 0; i < Q.Num(); ++i)
	{
		if (Dist[i] >= R) continue;
		static const int32 D[4][2] = { {1,0},{-1,0},{0,1},{0,-1} };
		for (int32 d = 0; d < 4; ++d)
		{
			const FIntPoint N(Q[i].X + D[d][0], Q[i].Y + D[d][1]);
			if (Have.Contains(N)) continue;
			bool bOcc = false;
			for (const FFramePiece& P : FrameList) if (P.Gx == N.X && P.Gz == N.Y) { bOcc = true; break; }
			if (!bOcc) continue;
			Have.Add(N); Q.Add(N); Dist.Add(Dist[i] + 1);
		}
	}
	return Have;
}

void UVenturesFeature::FrameShow(const FFramePiece& P)
{
	const FKind* K = FindKind(P.Kind); if (!K) return;
	UWorld* W = GetWorld(); if (!W) return;
	const FVector Top = WebToWorld(WorldX(P.Gx), WorldZ(P.Gz), 40.0);
	const FVector Bot = WebToWorld(WorldX(P.Gx), WorldZ(P.Gz), -20.0);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(VenturesFrame), false);
	FHitResult H;
	FVector At = WebToWorld(WorldX(P.Gx), WorldZ(P.Gz), 0.0);
	if (W->LineTraceSingleByChannel(H, Top, Bot, ECC_Visibility, Q) && H.bBlockingHit) At = H.ImpactPoint;
	At.Z += K->HeightM * 50.f;
	FActorSpawnParameters Sp; Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (AFreeframePiece* A = W->SpawnActor<AFreeframePiece>(At, FRotator::ZeroRotator, Sp))
	{
		const bool bBurn = LeyOnFire(WorldX(P.Gx), WorldZ(P.Gz));
		A->Setup(FLinearColor(K->R, K->G, K->B), K->HeightM * 100.f, bBurn);
		A->PieceId = P.Id;
		FrameMeshes.Add(A);
	}
}

void UVenturesFeature::FrameRebuild()
{
	FrameClearMeshes();
	if (!bFrameDeed) return;
	for (const FFramePiece& P : FrameList) FrameShow(P);
}

void UVenturesFeature::ResetFrame()
{
	FrameClearMeshes();
	bFrameDeed = false;
	FrameList.Reset();
	FrameNextId = 1;
}

void UVenturesFeature::SaveFrame(FJsonObject& A) const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	S->SetBoolField(TEXT("deed"), bFrameDeed);
	TArray<TSharedPtr<FJsonValue>> Arr;
	for (const FFramePiece& P : FrameList)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("id"), P.Id);
		O->SetStringField(TEXT("kind"), P.Kind);
		O->SetNumberField(TEXT("gx"), P.Gx);
		O->SetNumberField(TEXT("gz"), P.Gz);
		O->SetNumberField(TEXT("rot"), P.Rot);
		O->SetNumberField(TEXT("hp"), P.Hp);
		O->SetStringField(TEXT("tier"), P.Tier);
		Arr.Add(MakeShared<FJsonValueObject>(O));
	}
	S->SetArrayField(TEXT("pieces"), Arr);
	S->SetNumberField(TEXT("nextId"), FrameNextId);
	A.SetObjectField(TEXT("frame"), S);
}

void UVenturesFeature::LoadFrame(const FJsonObject& A)
{
	const TSharedPtr<FJsonObject> S = JObj(A, TEXT("frame"));
	if (!S) return;
	bFrameDeed = JBool(*S, TEXT("deed"));
	FrameList.Reset();
	if (const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr; S->TryGetArrayField(TEXT("pieces"), Arr) && Arr)
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			if (!V.IsValid() || V->Type != EJson::Object) continue;
			const TSharedPtr<FJsonObject> O = V->AsObject();
			if (!O.IsValid()) continue;
			const FString Kind = JStr(*O, TEXT("kind"));
			const FKind* K = FindKind(Kind); if (!K || FrameList.Num() >= 80) continue;
			FFramePiece P;
			P.Id = JStr(*O, TEXT("id"));
			if (P.Id.IsEmpty()) P.Id = FString::Printf(TEXT("p%d"), FrameList.Num() + 1);
			P.Kind = Kind;
			P.Gx = FMath::Clamp((int32)JInt(*O, TEXT("gx")), -20, 20);
			P.Gz = FMath::Clamp((int32)JInt(*O, TEXT("gz")), -20, 20);
			P.Rot = FMath::Clamp((int32)JInt(*O, TEXT("rot")), 0, 3);
			P.Hp = FMath::Max(0, (int32)JInt(*O, TEXT("hp"), K->Hp));
			P.Tier = K->Tier;
			FrameList.Add(P);
		}
	}
	FrameNextId = FMath::Max(1, (int32)JInt(*S, TEXT("nextId"), 1));
	for (const FFramePiece& P : FrameList)
	{
		int32 N = 0;
		if (P.Id.StartsWith(TEXT("p"))) N = FCString::Atoi(*P.Id.Mid(1));
		FrameNextId = FMath::Max(FrameNextId, N + 1);
	}
}

bool UVenturesFeature::FrameOn(float X, float Z) const { return bFrameDeed && InPlot(X, Z); }
bool UVenturesFeature::FrameClaimed() const { return bFrameDeed; }
bool UVenturesFeature::FrameHome() const
{
	FVector2D P; return PlayerWebPos(P) && FrameOn(P.X, P.Y);
}
bool UVenturesFeature::FrameFlammable(float X, float Z) const
{
	for (const FFramePiece& P : FrameList)
		if (P.Gx == GxOf(X) && P.Gz == GzOf(Z))
			if (const FKind* K = FindKind(P.Kind)) if (K->bWood || K->bThatch) return true;
	return false;
}
bool UVenturesFeature::FrameMetal(FVector2D& Out) const
{
	for (const FFramePiece& P : FrameList)
		if (const FKind* K = FindKind(P.Kind); K && K->bMetal) { Out = FVector2D(WorldX(P.Gx), WorldZ(P.Gz)); return true; }
	return false;
}

bool UVenturesFeature::FrameBuy()
{
	if (bFrameDeed) { Say(TEXT("You already hold the Claimridge deed.")); return true; }
	if (Coins() < DeedGold) { Say(FString::Printf(TEXT("The Claimridge deed costs %d coins."), DeedGold)); return false; }
	TakeCoins(DeedGold);
	bFrameDeed = true;
	Say(TEXT("You claim Claimridge, a wild plot north of Larkhaven. Build here — not in town, not on the farm. Buying the deed does not train Construction; placing pieces does."));
	return true;
}

bool UVenturesFeature::FramePlace(const FString& Kind)
{
	FVector2D P; if (!PlayerWebPos(P)) return false;
	return FramePlaceAt(Kind, GxOf(P.X), GzOf(P.Y));
}

bool UVenturesFeature::FramePlaceAt(const FString& Kind, int32 Gx, int32 Gz)
{
	const FKind* Def = FindKind(Kind); if (!Def) return false;
	if (!bFrameDeed) { Say(TEXT("Claim the wild plot first.")); return false; }
	if (SkillLevel(TEXT("Construction")) < Def->Lvl) { Say(FString::Printf(TEXT("You need Construction %d to place a %s."), Def->Lvl, Def->Name)); return false; }
	if (FMath::Abs(Gx * Grid) > PlotHalf || FMath::Abs(Gz * Grid) > PlotHalf) { Say(TEXT("That is outside the claimed plot.")); return false; }
	if (FrameList.Num() >= FrameCap()) { Say(FString::Printf(TEXT("The plot holds %d pieces at your Construction."), FrameCap())); return false; }
	for (const FFramePiece& P : FrameList) if (P.Gx == Gx && P.Gz == Gz && P.Kind == Kind) { Say(TEXT("That cell already has one.")); return false; }
	if (!Def->bSupport)
	{
		bool bNear = false;
		for (const FFramePiece& P : FrameList)
			if (const FKind* K = FindKind(P.Kind); K && K->bSupport && FMath::Abs(P.Gx - Gx) + FMath::Abs(P.Gz - Gz) <= 1) { bNear = true; break; }
		if (!bNear) { Say(TEXT("Place a foundation, then build out from it.")); return false; }
	}
	if (CountItem(Def->MatA) < Def->NA || (Def->MatB && CountItem(Def->MatB) < Def->NB))
	{
		FString Need = FString::Printf(TEXT("%d %s"), Def->NA, Def->MatA);
		if (Def->MatB) Need += FString::Printf(TEXT(", %d %s"), Def->NB, Def->MatB);
		Say(FString::Printf(TEXT("You need %s."), *Need));
		return false;
	}
	TakeItem(Def->MatA, Def->NA);
	if (Def->MatB) TakeItem(Def->MatB, Def->NB);
	FFramePiece P;
	P.Id = FString::Printf(TEXT("p%d"), FrameNextId++);
	P.Kind = Def->Id;
	P.Gx = Gx; P.Gz = Gz;
	P.Hp = FMath::RoundToInt(Def->Hp * (1.f + UAerondelRules::Perk(GetPS(), FName(TEXT("Construction")), FName(TEXT("pieceHp")))));
	P.Tier = Def->Tier;
	FrameList.Add(P);
	FrameShow(P);
	GiveXp(TEXT("Construction"), (float)Def->Xp);
	Say(FString::Printf(TEXT("You raise a %s. (+%d Construction)"), Def->Name, Def->Xp));
	return true;
}

bool UVenturesFeature::FrameRemove(const FString& Id, const FString& Why)
{
	const int32 I = FrameList.IndexOfByPredicate([&](const FFramePiece& P) { return P.Id == Id; });
	if (I < 0) return false;
	const FString Name = FindKind(FrameList[I].Kind) ? FindKind(FrameList[I].Kind)->Name : TEXT("piece");
	FrameList.RemoveAt(I);
	for (int32 M = FrameMeshes.Num() - 1; M >= 0; --M)
		if (AFreeframePiece* A = Cast<AFreeframePiece>(FrameMeshes[M])) if (A->PieceId == Id) { A->Destroy(); FrameMeshes.RemoveAt(M); }
	if (!Why.IsEmpty()) Say(FString::Printf(TEXT("%s: %s"), *Name, *Why));
	FrameSettle();
	return true;
}

void UVenturesFeature::FrameSettle()
{
	const TSet<FIntPoint> Have = FrameSupported();
	TArray<FString> Fall;
	for (const FFramePiece& P : FrameList)
	{
		const FKind* K = FindKind(P.Kind);
		if (K && !K->bSupport && !Have.Contains(FIntPoint(P.Gx, P.Gz)))
		{
			Say(FString::Printf(TEXT("A %s loses its support and comes down."), K->Name));
			Fall.Add(P.Id);
		}
	}
	for (const FString& Id : Fall)
	{
		const int32 I = FrameList.IndexOfByPredicate([&](const FFramePiece& P) { return P.Id == Id; });
		if (I >= 0) FrameList.RemoveAt(I);
		for (int32 M = FrameMeshes.Num() - 1; M >= 0; --M)
			if (AFreeframePiece* A = Cast<AFreeframePiece>(FrameMeshes[M])) if (A->PieceId == Id) { A->Destroy(); FrameMeshes.RemoveAt(M); }
	}
}

bool UVenturesFeature::FrameRepair(const FString& Id)
{
	FFramePiece* P = nullptr;
	for (FFramePiece& R : FrameList) if (R.Id == Id) { P = &R; break; }
	if (!P) return false;
	const FKind* Def = FindKind(P->Kind); if (!Def) return false;
	if (CountItem(Def->MatA) < 1) { Say(FString::Printf(TEXT("You need %s to repair that."), Def->MatA)); return false; }
	TakeItem(Def->MatA, 1);
	const int32 Heal = FMath::RoundToInt(14.f * (1.f + UAerondelRules::Perk(GetPS(), FName(TEXT("Construction")), FName(TEXT("repairSpeed")))));
	P->Hp = FMath::Min(Def->Hp, P->Hp + Heal);
	GiveXp(TEXT("Construction"), 10.f);
	Say(FString::Printf(TEXT("You repair the %s. (+10 Construction)"), Def->Name));
	FrameRebuild();
	return true;
}

bool UVenturesFeature::FrameDamage(float X, float Z, int32 N)
{
	FFramePiece* P = FrameAtMut(GxOf(X), GzOf(Z));
	if (!P) return false;
	P->Hp = FMath::Max(0, P->Hp - N);
	if (P->Hp <= 0) FrameRemove(P->Id, TEXT("It gives way."));
	return true;
}

void UVenturesFeature::FrameGo()
{
	if (!bFrameDeed && !FrameBuy()) return;
	TeleportPlayer(FVector2D(PlotX, PlotZ + 2.f));
}

void UVenturesFeature::TickFrame()
{
	if (!bFrameDeed || !GetWorld()) return;
	if (FrameHome() && FrameMeshes.Num() != FrameList.Num()) FrameRebuild();
}

void UVenturesFeature::TabFrame(FGPanelModel& M)
{
	const int32 Con = SkillLevel(TEXT("Construction"));
	if (!bFrameDeed)
	{
		M.Title = TEXT("Claimridge");
		M.Lines.Add(FString::Printf(TEXT("A wild plot north of Larkhaven for freeform building. Town houses are storage; the farm is for growing. The deed is %d coins and does not train Construction — placing pieces does."), DeedGold));
		VAct23(M, FString::Printf(TEXT("Claim the plot (%d coins)"), DeedGold), [this] { FrameBuy(); }, Coins() >= DeedGold);
		return;
	}
	M.Title = FString::Printf(TEXT("Freeframe · Construction %d · %d/%d pieces"), Con, FrameList.Num(), FrameCap());
	M.Lines.Add(TEXT("Build wood, thatch, stone, then reinforced, gated by Construction. Pieces need a path to a foundation or they fall. Building and repairing train Construction (this is the main grind toward 1000). Wooden pieces can burn; the fire stays on this plot. This is not a town house and not the farm."));
	FVector2D Pos; const bool bHere = PlayerWebPos(Pos) && InPlot(Pos.X, Pos.Y);
	if (bHere) M.Lines.Add(FString::Printf(TEXT("You are on the plot (cell %d, %d)."), GxOf(Pos.X), GzOf(Pos.Y)));
	else M.Lines.Add(TEXT("Travel to Claimridge to place pieces under your feet."));
	VAct23(M, TEXT("Travel to Claimridge"), [this] { FrameGo(); }, true, true);
	if (bHere)
	{
		for (const FKind& K : Kinds)
			VAct23(M, FString::Printf(TEXT("Place %s (Con %d)"), K.Name, K.Lvl), [this, Id = FString(K.Id)] { FramePlace(Id); }, Con >= K.Lvl && FrameList.Num() < FrameCap());
	}
	for (const FFramePiece& P : FrameList)
	{
		const FKind* K = FindKind(P.Kind);
		M.Lines.Add(FString::Printf(TEXT("%s at %d,%d — %d hp"), K ? K->Name : *P.Kind, P.Gx, P.Gz, P.Hp));
		VAct23(M, FString::Printf(TEXT("Repair %s"), K ? K->Name : *P.Id), [this, Id = P.Id] { FrameRepair(Id); });
	}
}
