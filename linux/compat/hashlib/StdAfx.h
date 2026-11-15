// Forward the Windows-style "StdAfx.h" include to HashLib's stdafx.h (Linux is case-sensitive).
#include "../../../HashLib/stdafx.h"

// HashLib relies on the min/max macros <windows.h> defines without NOMINMAX.
// Templates that accept mixed argument types stand in for them here.
#include <type_traits>
template< class A, class B > inline typename std::common_type< A, B >::type min(const A& a, const B& b) { return b < a ? b : a; }
template< class A, class B > inline typename std::common_type< A, B >::type max(const A& a, const B& b) { return a < b ? b : a; }
