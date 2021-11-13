// Catch all to include all the display systems we currently
// support in code. Add to this file when creating a new
// display

#ifndef ALL_DISPLAYS_HPP
#define ALL_DISPLAYS_HPP

#include "DisplaySystem.hpp"
#ifdef OLED96in
#include "DisplayOLED96in.hpp"
#endif
#ifdef EINK154in
#include "DisplayEink154in.hpp"
#endif
#ifdef EINKET013TT1
#include "DisplayEinkET013TT1.hpp"
#endif
#ifdef SHARPMEM
#include "DisplaySharpMem.hpp"
#endif

#endif