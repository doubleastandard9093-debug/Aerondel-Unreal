#pragma once
#include "CoreMinimal.h"

class UGameDatabase;

/**
 * Builds game content from Content/Data/*.json (exported from the Aerondel Lite WebGPU build by
 * Tools/export_webgpu_data.mjs). Defs are created as transient objects inside the database; anything already
 * authored as a data asset with the same Id wins and is left untouched, so designers can override any row.
 *
 * Ids: display names converted with AerondelId::FromName ("Giant Spider" -> giant_spider).
 * Scale: hitpoints, damage and food heals are already x10 in the web data (HealScale), so they are used as-is.
 */
struct AERONDEL_API FJsonContentLoader
{
	/** <Project>/Content/Data/ (staged as non-UFS files, see DefaultGame.ini). */
	static FString DefaultDataDir();
	/** Returns the number of defs created. Safe to call more than once (existing ids are reused). */
	static int32 MergeInto(UGameDatabase* Db, const FString& Dir);
};
