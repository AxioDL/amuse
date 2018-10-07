#pragma once

/* Properly forward math defines to std:: */
#ifdef __SWITCH__

#include <cmath>
#include <cfloat>

#undef exp2
#undef log2
#undef fabs

namespace std
{
    using ::exp2;
    using ::log2;
    using ::fabs;
}
#endif

