# Making a menu

Refer to the existing menu to see how it is built and linked.
You have a good amount of capability as a menu, and can control the entire lifecycle

You have two options for creating menu/custom interfaces as well:
1. Use the existing NekoMenu as a host (easier)
2. Write a completely custom menu implementation (harder)

For most cases I reccomend using NekoMenu as it provides a rendering and sound API, alongside management and is very customizable

But if you want to make your own menu you can refer to NekoMenu code or look at the menu interface

## Making your own menu

Rendering:
You have many options for rendering:
neko3d
SDL3 with neko3d backend (2D via SDL, 3D via neko3d)
Software (libnx framebuffer)

Input:
You can use SDL3 or libnx for input

Sound:
Again, SDL3 or libnx

Programming language:
C++, but bindings for Lua and JavaScript are planned in future

## Making your own extention to NekoMenu
TODO

## Heap management
For full operation, your menu must fit within 62MB heap.
To allocate memory you must use the qext_* functions, as they are the only ones capable of properly managing and relocating themselves on heap layout differences

## Standard compliance
libnx and libc are fully linked, so you can freely use their functions.
FFMpeg, SDL3, Neko3D (deko3D Dynamic), xxhash, JSON for modern C++ and miniIni are dynamically linked as well.
For libstdc++, it's more complicated.
Due to agonizing link times with the entire lib linked, only parts of it are linked.
These include:
- **Strings/streams:** `<string>` (`std::string`), `<sstream>`, `<iostream>`, `<fstream>`, `<iomanip>`
- **Containers:** `<vector>` (incl. `int`, `unsigned char`, `uint64_t`, `std::string`), `<array>`, `<deque>`, `<list>`, `<queue>`, `<stack>`, `<map>`, `<set>`, `<unordered_map>`, `<unordered_set>`
- **Algorithms/utilities:** `<algorithm>` (`sort`/`find`/`stable_sort`/`copy`/`back_inserter`), `<numeric>` (`accumulate`), `<iterator>`, `<utility>` (`pair`), `<tuple>`
- **Memory/callables:** `<memory>` (`shared_ptr`/`weak_ptr`/`make_shared`), `<functional>` (`std::function`)
- **Atomics/chrono:** `<atomic>` (`int`, `bool`, `unsigned`, `uint32_t`, `uint64_t`, fences/CAS/`fetch_*`), `<chrono>` (`system_clock`/`steady_clock`, `duration_cast` ms/us, `this_thread::sleep_for`)
- **Threads:** `<mutex>`/`<lock_guard>`/`<unique_lock>`, `<condition_variable>`, `<thread>`, `<future>`/`<promise>`/`std::async`
- **Other:** `<random>` (`mt19937`, distributions), `<bitset>`, `<complex>`, `<valarray>`, `<optional>`, `<variant>`, `<any>`, `<regex>`, `<filesystem>` (`path`/`exists`/`file_size`), `<type_traits>`/`<limits>`
If you need something else, either reimplement it yourself or you must recompile loader