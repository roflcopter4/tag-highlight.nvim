#include "intern.h"

const mpack_mask m_masks[] = {
    { G_NIL,    M_NIL,       false, M_MASK_NIL,       0, "M_NIL"       },
    { G_BOOL,   M_TRUE,      false, M_MASK_TRUE,      0, "M_TRUE"      },
    { G_BOOL,   M_FALSE,     false, M_MASK_FALSE,     0, "M_FALSE"     },
    { G_STRING, M_STR_8,     false, M_MASK_STR_8,     0, "M_STR_8"     },
    { G_STRING, M_STR_16,    false, M_MASK_STR_16,    0, "M_STR_16"    },
    { G_STRING, M_STR_32,    false, M_MASK_STR_32,    0, "M_STR_32"    },
    { G_ARRAY,  M_ARRAY_16,  false, M_MASK_ARRAY_16,  0, "M_ARRAY_16"  },
    { G_ARRAY,  M_ARRAY_32,  false, M_MASK_ARRAY_32,  0, "M_ARRAY_32"  },
    { G_MAP,    M_MAP_16,    false, M_MASK_MAP_16,    0, "M_MAP_16"    },
    { G_MAP,    M_MAP_32,    false, M_MASK_MAP_32,    0, "M_MAP_32"    },
    { G_BIN,    M_BIN_8,     false, M_MASK_BIN_8,     0, "M_BIN_8"     },
    { G_BIN,    M_BIN_16,    false, M_MASK_BIN_16,    0, "M_BIN_16"    },
    { G_BIN,    M_BIN_32,    false, M_MASK_BIN_32,    0, "M_BIN_32"    },
    { G_INT,    M_INT_8,     false, M_MASK_INT_8,     0, "M_INT_8"     },
    { G_INT,    M_INT_16,    false, M_MASK_INT_16,    0, "M_INT_16"    },
    { G_INT,    M_INT_32,    false, M_MASK_INT_32,    0, "M_INT_32"    },
    { G_INT,    M_INT_64,    false, M_MASK_INT_64,    0, "M_INT_64"    },
    { G_UINT,   M_UINT_8,    false, M_MASK_UINT_8,    0, "M_UINT_8"    },
    { G_UINT,   M_UINT_16,   false, M_MASK_UINT_16,   0, "M_UINT_16"   },
    { G_UINT,   M_UINT_32,   false, M_MASK_UINT_32,   0, "M_UINT_32"   },
    { G_UINT,   M_UINT_64,   false, M_MASK_UINT_64,   0, "M_UINT_64"   },
    { G_EXT,    M_EXT_8,     false, M_MASK_EXT_8,     0, "M_EXT_8"     },
//  { G_EXT,    M_EXT_16,    false, M_MASK_EXT_16,    0, "M_EXT_16"    },
//  { G_EXT,    M_EXT_32,    false, M_MASK_EXT_32,    0, "M_EXT_32"    },
    { G_EXT,    M_FIXEXT_1,  false, M_MASK_FIXEXT_1,  0, "M_FIXEXT_1"  },
    { G_EXT,    M_FIXEXT_2,  false, M_MASK_FIXEXT_2,  0, "M_FIXEXT_2"  },
    { G_EXT,    M_FIXEXT_4,  false, M_MASK_FIXEXT_4,  0, "M_FIXEXT_4"  },
    { G_EXT,    M_FIXEXT_8,  false, M_MASK_FIXEXT_8,  0, "M_FIXEXT_8"  },
//  { G_EXT,    M_FIXEXT_16, false, M_MASK_FIXEXT_16, 0, "M_FIXEXT_16" },
    { G_STRING, M_FIXSTR_F,  true,  M_MASK_STR_F,     5, "M_FIXSTR_F"  },
    { G_ARRAY,  M_ARRAY_F,   true,  M_MASK_ARRAY_F,   4, "M_ARRAY_F"   },
    { G_MAP,    M_FIXMAP_F,  true,  M_MASK_MAP_F,     4, "M_FIXMAP_F"  },
    { G_PLINT,  M_POS_INT_F, true,  M_MASK_POS_INT_F, 7, "M_POS_INT_F" },
    { G_NLINT,  M_NEG_INT_F, true,  M_MASK_NEG_INT_F, 5, "M_NEG_INT_F" },
};

const size_t m_masks_len = (sizeof(m_masks) / sizeof(m_masks[0]));

const char *const m_type_names[9] = {
    "MPACK_UNINITIALIZED", "MPACK_BOOL",     "MPACK_NIL",
    "MPACK_SIGNED",        "MPACK_UNSIGNED", "MPACK_EXT",
    "MPACK_STRING",        "MPACK_ARRAY",    "MPACK_DICT",
};

const char *const m_expect_names[9] = {
    "E_MPACK_EXT", "E_MPACK_ARRAY", "E_MPACK_DICT", "E_MPACK_NIL", "E_BOOL",
    "E_NUM",       "E_STRING",      "E_STRLIST",    "E_DICT2ARR"
};

const char *const m_message_type_repr[4] = {"MES_REQUEST", "MES_RESPONSE",
                                            "MES_NOTIFICATION", "MES_ANY"};
