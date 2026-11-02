#include "Misc/AutomationTest.h"
#include "Data/SkillDef.h"
#include "Combat/CombatMath.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAerondelXpCurveTest, "Aerondel.Skills.XpCurve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAerondelXpCurveTest::RunTest(const FString&)
{
	TestEqual(TEXT("Level 1 = 0"), UXpCurve::XpForLevel(1), 0);
	TestEqual(TEXT("Level 2 = 24"), UXpCurve::XpForLevel(2), 24);
	TestEqual(TEXT("Level 10 = 346"), UXpCurve::XpForLevel(10), 346);
	TestEqual(TEXT("Level 99 = 3,910,329"), UXpCurve::XpForLevel(99), 3910329);
	TestEqual(TEXT("Level 120 = 31,281,950"), UXpCurve::XpForLevel(120), 31281950);
	TestEqual(TEXT("23 xp is level 1"), UXpCurve::LevelForXp(23.f), 1);
	TestEqual(TEXT("24 xp is level 2"), UXpCurve::LevelForXp(24.f), 2);
	TestEqual(TEXT("Old L10 converts to new L10"), (int32)UXpCurve::ConvertXpV1(2514.f), 346);
	TestEqual(TEXT("Old 1000 xp converts like the web"), (int32)UXpCurve::ConvertXpV1(1000.f), 176);
	TestEqual(TEXT("Old L99 converts to new L99"), (int32)UXpCurve::ConvertXpV1(359720.f), 3910329);
	TestEqual(TEXT("200M xp caps at 99"), UXpCurve::LevelForXp(200000000.f), 99);
	TestEqual(TEXT("Max hit at 1 str, no bonus"), UCombatMath::MaxHit(1, 0), 1);
	return true;
}

#endif
