export module phenotype.macos;

export import phenotype;
import std;

#define PHENOTYPE_IMPORTS_STD_MODULE
#define PHENOTYPE_MACOS_IMPORTS_PHENOTYPE_MODULE
export {
#include "phenotype/macos.hpp"
}
#undef PHENOTYPE_MACOS_IMPORTS_PHENOTYPE_MODULE
#undef PHENOTYPE_IMPORTS_STD_MODULE
