#include "clang.h"
#include "intern.h"

#ifndef ARRSIZ
#  define ARRSIZ(ARR) (sizeof(ARR) / sizeof((ARR)[0]))
#endif

//const char *const gcc_sys_dirs[] = GCC_ALL_INCLUDE_DIRECTORIES;
const char *const gcc_sys_dirs[] = {"D:/msys64/mingw64/lib/gcc/x86_64-w64-mingw32/8.2.0/include", "D:/msys64/mingw64/lib/gcc/x86_64-w64-mingw32/8.2.0/include-fixed"};
const size_t      n_gcc_sys_dirs = ARRSIZ(gcc_sys_dirs);

const char *const idx_entity_kind_repr[] = {
    "Unexposed",
    "Typedef",
    "Function",
    "Variable",
    "Field",
    "EnumConstant",
    "ObjCClass",
    "ObjCProtocol",
    "ObjCCategory",
    "ObjCInstanceMethod",
    "ObjCClassMethod",
    "ObjCProperty",
    "ObjCIvar",
    "Enum",
    "Struct",
    "Union",
    "CXXClass",
    "CXXNamespace",
    "CXXNamespaceAlias",
    "CXXStaticVariable",
    "CXXStaticMethod",
    "CXXInstanceMethod",
    "CXXConstructor",
    "CXXDestructor",
    "CXXConversionFunction",
    "CXXTypeAlias",
    "CXXInterface",
};
const size_t idx_entity_kind_num = ARRSIZ(idx_entity_kind_repr);

pthread_cond_t libclang_cond = PTHREAD_COND_INITIALIZER;
