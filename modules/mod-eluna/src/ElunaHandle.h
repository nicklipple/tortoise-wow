#pragma once

#include <cstdint>

namespace Eluna
{
    // A Lua wrapper identifies the public type it was created for. The type
    // hierarchy is represented by the Lua metatables; the native payload never
    // stores a Tortoise object pointer.
    enum class HandleType : std::uint8_t
    {
        Object = 0,
        WorldObject = 1,
        Unit = 2,
        Player = 3,
    };

    struct ObjectHandle
    {
        std::uint64_t guid = 0;
        std::uint64_t generation = 0;
        std::uint8_t type = static_cast<std::uint8_t>(HandleType::Object);
    };
}
