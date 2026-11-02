#pragma once
// Skill gates shared by relic dungeons and Delving floors (web js/gates.js).
#include "CoreMinimal.h"
#include "Features/RelicryData.h"

namespace AerondelGates
{
	inline const RelicryData::FGate* Find(const FString& Skill)
	{
		for (const RelicryData::FGate& G : RelicryData::Gates) if (Skill == G.Skill) return &G;
		return nullptr;
	}
	/** Level needed: grows with dungeon level and gate depth; gateEase perks shave levels off. */
	inline int32 GateLevel(int32 DLvl, int32 Depth, float Ease = 0.f)
	{
		return FMath::Clamp(FMath::RoundToInt(DLvl * 0.75f + Depth * 2 - Ease * 20.f), 1, 96);
	}
	/** Draw N distinct gate skills, weighted 1/(1+seen)^2 toward skills tested least. */
	inline TArray<FString> Draw(const TMap<FString, int32>& Seen, int32 N)
	{
		TArray<FString> Left; for (const RelicryData::FGate& G : RelicryData::Gates) Left.Add(G.Skill);
		TArray<FString> Pick;
		while (Pick.Num() < N && Left.Num() > 0)
		{
			TArray<float> W; float Tot = 0.f;
			for (const FString& S : Left) { const float V = 1.f / FMath::Square(1.f + Seen.FindRef(S)); W.Add(V); Tot += V; }
			float R = FMath::FRand() * Tot; int32 i = 0;
			while (i < Left.Num() - 1 && (R -= W[i]) > 0.f) ++i;
			Pick.Add(Left[i]); Left.RemoveAt(i);
		}
		return Pick;
	}
}
