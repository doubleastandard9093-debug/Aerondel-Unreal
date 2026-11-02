#pragma once
#include "CoreMinimal.h"

/** Content ids are lower_snake_case: "Bronze axe" -> bronze_axe, "Herbalist's amulet" -> herbalists_amulet. */
namespace AerondelId
{
	inline FName FromName(const FString& Name)
	{
		FString Out; Out.Reserve(Name.Len());
		bool bUnderscore = false;
		for (TCHAR C : Name)
		{
			if (C == TEXT('\'')) continue;
			if (FChar::IsAlnum(C)) { Out.AppendChar(FChar::ToLower(C)); bUnderscore = false; }
			else if (!bUnderscore && Out.Len() > 0) { Out.AppendChar(TEXT('_')); bUnderscore = true; }
		}
		while (Out.EndsWith(TEXT("_"))) Out.LeftChopInline(1);
		return FName(*Out);
	}
}
