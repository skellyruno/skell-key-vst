#include "../Source/ProgramInfo.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstring>
#include <cstdio>

int main() {
    assert(skellkey::host::programCount == 1);
    assert(std::strcmp(skellkey::host::programName(0), "Default") == 0);
    assert(std::strcmp(skellkey::host::programName(-1), "") == 0);
    assert(std::strcmp(skellkey::host::programName(1), "") == 0);
    std::puts("Skell Key host program contract: PASS");
}
