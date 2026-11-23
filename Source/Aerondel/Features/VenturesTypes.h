#pragma once
#include "CoreMinimal.h"

class AAerondelMob;

// Plain data records used by UVenturesFeature (split out of VenturesFeature.h; no UObject reflection here).

/** One Overflare ultimate per weapon family (web adapt/overflare.js ULTS). */
struct FVentureUlt { const TCHAR* Key; const TCHAR* Name; int32 Hits; float Bonus; float Radius; };

/** A saved Lookplate (Kitbash Wardrobe): slot -> look item, slot -> dye name. Up to 8 plates, including weapons. */
struct FVentureOutfit { bool bUsed = false; TMap<FString, FString> Looks; TMap<FString, FString> Dyes; };

/** Townstead furniture in one owned house. */
struct FSteadFurn { bool bChest = false; bool bBed = false; TArray<FString> Hooks; };

/** Town Tie record for one townsfolk (rank 0-5). */
struct FTieRec { int32 Rank = 0; int64 GiftAt = 0; FString TalkAt; bool bHeart = false; bool bErrand = false; };

struct FGreenPlot { FString Crop; int64 PlantedAt = 0; FString Season; bool bCover = false; };
struct FGreenAnimal { FString Type; int64 FedAt = 0; bool bReady = false; };
struct FGreenDecor { FString Id; float X = 0.f; float Z = 0.f; float Rot = 0.f; };

struct FFacetTrail { TWeakObjectPtr<AAerondelMob> Mob; int64 DueMs = 0; };

/** Totals from filled Leyweave nodes (web A.leyFx). */
struct FLeyFx { float LeyGain = 0.f; float WhirlR = 0.f; float GrowSpeed = 0.f; float BuildSpeed = 0.f; float CryCd = 0.f; int32 Stun = 0; int32 CarveWin = 0; };

/** A Hordewake / Ashspire reward card. */
struct FRunPick { FString T, Id, From, Name, Desc; };

struct FKinRec
{
	FString Id, Species, Name;
	int32 Level = 1, Xp = 0, Loyalty = 20, Hp = 14;
	int64 FaintUntil = 0;
};

struct FFramePiece
{
	FString Id, Kind, Tier;
	int32 Gx = 0, Gz = 0, Rot = 0, Hp = 0;
};

struct FLeyFire
{
	FString Kind, Src;
	float Heat = 1.f, X = 0.f, Z = 0.f;
};

struct FKinLive
{
	bool bOn = false;
	FString Id;
	int32 Hp = 0;
	int64 NextMs = 0;
	float Acc = 0.f;
	TWeakObjectPtr<AActor> Pet;
};

/** Hidden Reliquary cache site (web metres). */
struct FVentureRelic { int32 Index = 0; FVector2D Pos = FVector2D::ZeroVector; int32 Tier = 1; };
