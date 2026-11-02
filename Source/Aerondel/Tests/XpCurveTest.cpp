#include "Misc/AutomationTest.h"
#include "Data/SkillDef.h"
#include "Combat/CombatMath.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAerondelXpCurveTest, "Aerondel.Skills.XpCurve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAerondelXpCurveTest::RunTest(const FString&)
{
	TestEqual(TEXT("Level 1 = 0"), (int32)UXpCurve::XpForLevel(1), 0);
	TestEqual(TEXT("Level 2 = 24"), (int32)UXpCurve::XpForLevel(2), 24);
	TestEqual(TEXT("Level 10 = 346"), (int32)UXpCurve::XpForLevel(10), 346);
	TestEqual(TEXT("Level 99 = 3,910,329"), (int32)UXpCurve::XpForLevel(99), 3910329);
	TestTrue(TEXT("Level 1000 needs more than int32"), UXpCurve::XpForLevel(1000) > (int64)MAX_int32);
	TestEqual(TEXT("23 xp is level 1"), UXpCurve::LevelForXp(23.f), 1);
	TestEqual(TEXT("24 xp is level 2"), UXpCurve::LevelForXp(24.f), 2);
	TestEqual(TEXT("Old L10 converts to new L10"), (int32)UXpCurve::ConvertXpV1(2514.f), 346);
	TestEqual(TEXT("Old 1000 xp converts like the web"), (int32)UXpCurve::ConvertXpV1(1000.f), 176);
	TestEqual(TEXT("Old L99 converts to new L99"), (int32)UXpCurve::ConvertXpV1(359720.f), 3910329);
	TestEqual(TEXT("200M xp caps at 99 for non-combat"), UXpCurve::LevelForXp(200000000.f), 99);
	TestEqual(TEXT("Combat cap is 1000"), UXpCurve::LevelForXp(1.0e12f, UXpCurve::CombatMaxLevel), 1000);
	TestEqual(TEXT("Max hit at 1 str, no bonus"), UCombatMath::MaxHit(1, 0), 1);
	return true;
}

#endif
