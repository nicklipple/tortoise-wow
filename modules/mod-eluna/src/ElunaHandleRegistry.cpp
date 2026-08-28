#include "ElunaHandleRegistry.h"

namespace Eluna
{
    std::uint64_t HandleRegistry::NextGeneration()
    {
        ++_nextGeneration;
        if (_nextGeneration == 0)
            ++_nextGeneration;

        return _nextGeneration;
    }

    std::uint64_t HandleRegistry::Activate(std::uint64_t guid)
    {
        if (guid == 0)
            return 0;

        std::uint64_t const generation = NextGeneration();
        _generations[guid] = generation;
        return generation;
    }

    void HandleRegistry::Invalidate(std::uint64_t guid)
    {
        if (guid == 0)
            return;

        _generations[guid] = NextGeneration();
    }

    void HandleRegistry::InvalidateAll()
    {
        _generations.clear();
    }

    bool HandleRegistry::IsCurrent(std::uint64_t guid, std::uint64_t generation) const
    {
        if (guid == 0 || generation == 0)
            return false;

        auto const itr = _generations.find(guid);
        return itr != _generations.end() && itr->second == generation;
    }
}
