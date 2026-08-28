#include "ElunaHandleRegistry.h"

int main()
{
    Eluna::HandleRegistry handles;
    constexpr std::uint64_t playerGuid = 0x1234;

    if (handles.Activate(0) != 0)
        return 1;

    std::uint64_t const firstGeneration = handles.Activate(playerGuid);
    if (firstGeneration == 0 || !handles.IsCurrent(playerGuid, firstGeneration))
        return 1;

    handles.Invalidate(playerGuid);
    if (handles.IsCurrent(playerGuid, firstGeneration))
        return 1;

    std::uint64_t const secondGeneration = handles.Activate(playerGuid);
    if (secondGeneration == firstGeneration || !handles.IsCurrent(playerGuid, secondGeneration) ||
        handles.IsCurrent(playerGuid, firstGeneration))
        return 1;

    handles.InvalidateAll();
    return handles.IsCurrent(playerGuid, secondGeneration) ? 1 : 0;
}
