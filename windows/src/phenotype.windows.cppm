export module phenotype.windows;

export import phenotype;
import std;

#define PHENOTYPE_IMPORTS_STD_MODULE
#define PHENOTYPE_WINDOWS_IMPORTS_PHENOTYPE_MODULE
export {
#include "phenotype/windows.hpp"
}
#undef PHENOTYPE_WINDOWS_IMPORTS_PHENOTYPE_MODULE
#undef PHENOTYPE_IMPORTS_STD_MODULE
