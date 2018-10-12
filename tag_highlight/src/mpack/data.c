#include "intern.h"

const mpack_mask m_masks[] = {
    { G_NIL,    M_NIL,       false, 0xC0u,  0, "M_NIL"       },
    { G_BOOL,   M_TRUE,      false, 0xC3u,  0, "M_TRUE"      },
    { G_BOOL,   M_FALSE,     false, 0xC2u,  0, "M_FALSE"     },
    { G_STRING, M_STR_8,     false, 0xD9u,  0, "M_STR_8"     },
    { G_STRING, M_STR_16,    false, 0xDAu,  0, "M_STR_16"    },
    { G_STRING, M_STR_32,    false, 0xDBu,  0, "M_STR_32"    },
    { G_ARRAY,  M_ARRAY_16,  false, 0xDCu,  0, "M_ARRAY_16"  },
    { G_ARRAY,  M_ARRAY_32,  false, 0xDDu,  0, "M_ARRAY_32"  },
    { G_MAP,    M_MAP_16,    false, 0xDEu,  0, "M_MAP_16"    },
    { G_MAP,    M_MAP_32,    false, 0xDFu,  0, "M_MAP_32"    },
    { G_BIN,    M_BIN_8,     false, 0xC4u,  0, "M_BIN_8"     },
    { G_BIN,    M_BIN_16,    false, 0xC5u,  0, "M_BIN_16"    },
    { G_BIN,    M_BIN_32,    false, 0xC6u,  0, "M_BIN_32"    },
    { G_INT,    M_INT_8,     false, 0xD0u,  0, "M_INT_8"     },
    { G_INT,    M_INT_16,    false, 0xD1u,  0, "M_INT_16"    },
    { G_INT,    M_INT_32,    false, 0xD2u,  0, "M_INT_32"    },
    { G_INT,    M_INT_64,    false, 0xD3u,  0, "M_INT_64"    },
    { G_UINT,   M_UINT_8,    false, 0xCCu,  0, "M_UINT_8"    },
    { G_UINT,   M_UINT_16,   false, 0xCDu,  0, "M_UINT_16"   },
    { G_UINT,   M_UINT_32,   false, 0xCEu,  0, "M_UINT_32"   },
    { G_UINT,   M_UINT_64,   false, 0xCFu,  0, "M_UINT_64"   },
//  { G_EXT,    M_EXT_8,     false, 0xC7u,  0, "M_EXT_8"     },
//  { G_EXT,    M_EXT_16,    false, 0xC8u,  0, "M_EXT_16"    },
//  { G_EXT,    M_EXT_32,    false, 0xC9u,  0, "M_EXT_32"    },
    { G_EXT,    M_EXT_F1,    false, 0xD4u,  0, "M_EXT_F1"    },
    { G_EXT,    M_EXT_F2,    false, 0xD5u,  0, "M_EXT_F2"    },
    { G_EXT,    M_EXT_F4,    false, 0xD6u,  0, "M_EXT_F4"    },
    { G_STRING, M_FIXSTR_F,  true,  0xA0u,  5, "M_FIXSTR_F"  },
    { G_ARRAY,  M_ARRAY_F,   true,  0x90u,  4, "M_ARRAY_F"   },
    { G_MAP,    M_FIXMAP_F,  true,  0x80u,  4, "M_FIXMAP_F"  },
    { G_PLINT,  M_POS_INT_F, true,  0x00u,  7, "M_POS_INT_F" },
    { G_NLINT,  M_NEG_INT_F, true,  0xE0u,  5, "M_NEG_INT_F" },
};

const size_t m_masks_len = (sizeof(m_masks) / sizeof(m_masks[0]));

const char *const m_type_names[] = {
    "MPACK_UNINITIALIZED", "MPACK_BOOL",     "MPACK_NIL",
    "MPACK_SIGNED",        "MPACK_UNSIGNED", "MPACK_EXT",
    "MPACK_STRING",        "MPACK_ARRAY",    "MPACK_DICT",
};

const char *const m_expect_names[] = {
    "E_MPACK_EXT", "E_MPACK_ARRAY", "E_MPACK_DICT", "E_MPACK_NIL", "E_BOOL",
    "E_NUM",       "E_STRING",      "E_STRLIST",    "E_DICT2ARR"
};

const char *const m_message_type_repr[4] = {"MES_REQUEST", "MES_RESPONSE",
                                            "MES_NOTIFICATION", "MES_ANY"};
