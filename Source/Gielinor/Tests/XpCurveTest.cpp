#include "Misc/AutomationTest.h"
#include "Data/SkillDef.h"
#include "Combat/CombatMath.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGielinorXpCurveTest, "Gielinor.Skills.XpCurve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGielinorXpCurveTest::RunTest(const FString&)
{
	TestEqual(TEXT("Level 1 = 0"), UXpCurve::XpForLevel(1), 0);
	TestEqual(TEXT("Level 2 = 83"), UXpCurve::XpForLevel(2), 83);
	TestEqual(TEXT("Level 10 = 1154"), UXpCurve::XpForLevel(10), 1154);
	TestEqual(TEXT("Level 99 = 13,034,431"), UXpCurve::XpForLevel(99), 13034431);
	TestEqual(TEXT("82 xp is level 1"), UXpCurve::LevelForXp(82.f), 1);
	TestEqual(TEXT("83 xp is level 2"), UXpCurve::LevelForXp(83.f), 2);
	TestEqual(TEXT("200M xp caps at 99"), UXpCurve::LevelForXp(200000000.f), 99);
	TestEqual(TEXT("Max hit at 1 str, no bonus"), UCombatMath::MaxHit(1, 0), 1);
	return true;
}

#endif
