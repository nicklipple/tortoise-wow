#pragma once

#include <cstdint>
#include <unordered_map>

namespace Eluna
{
    // Generation state is deliberately world-thread-only. A handle is valid
    // only while its GUID maps to the same generation that it captured.
    class HandleRegistry final
    {
    public:
        std::uint64_t Activate(std::uint64_t guid);
        void Invalidate(std::uint64_t guid);
        void InvalidateAll();
        bool IsCurrent(std::uint64_t guid, std::uint64_t generation) const;

    private:
        std::uint64_t NextGeneration();

        std::uint64_t _nextGeneration = 0;
        std::unordered_map<std::uint64_t, std::uint64_t> _generations;
    };
}
