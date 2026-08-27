#include "ElunaUtility.h"
#include "Timer.h"

uint32 ElunaUtil::GetCurrTime()
{
    return WorldTimer::getMSTime();
}

uint32 ElunaUtil::GetTimeDiff(uint32 oldMSTime)
{
    return WorldTimer::getMSTimeDiffToNow(oldMSTime);
}
