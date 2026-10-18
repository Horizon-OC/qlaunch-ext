#pragma once
#include "MenuHost.hpp"

namespace HostApi {

    /// @brief Give the menu host API table.
    /// @return Host table with all menu functions plus qext.
    const MenuHostApi *GetHostApi();

} /* namespace HostApi */
