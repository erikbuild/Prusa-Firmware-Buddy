#pragma once

#include <option/has_indx.h>

#if HAS_INDX()
    #include <selftest_result_impl_indx.hpp>
using SelftestResult = SelftestResultImplIndx;
#else
    #include <selftest_result_impl.hpp>
using SelftestResult = SelftestResultImpl;
#endif
