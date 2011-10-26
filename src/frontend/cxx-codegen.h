#ifndef CXX_CODEGEN_H
#define CXX_CODEGEN_H

#include <stdio.h>
#include "cxx-ast.h"
#include "cxx-macros.h"
#include "libmcxx-common.h"

MCXX_BEGIN_DECLS

// This function is deprecated, call codegen_to_str instead
LIBMCXX_EXTERN const char* c_cxx_codegen_to_str(nodecl_t node) DEPRECATED;

MCXX_END_DECLS

#endif // CXX_CODEGEN_H
