#ifndef SRC_MPACK_CODE_H
#define SRC_MPACK_CODE_H
#pragma once

#include "mpack.h"


#ifdef __cplusplus
extern "C" {
#endif

enum m_groups { G_NIL, G_BOOL, G_ARRAY, G_MAP, G_STRING, G_BIN, G_INT, G_UINT, G_PLINT, G_NLINT, G_EXT };

enum m_types {
        M_NIL,       M_TRUE,      M_FALSE,     M_ARRAY_F,   M_ARRAY_E,   M_ARRAY_16,
        M_ARRAY_32,  M_FIXMAP_F,  M_FIXMAP_E,  M_MAP_16,    M_MAP_32,    M_FIXSTR_F,
        M_FIXSTR_E,  M_STR_8,     M_STR_16,    M_STR_32,    M_BIN_8,     M_BIN_16,
        M_BIN_32,    M_INT_8,     M_INT_16,    M_INT_32,    M_INT_64,    M_UINT_8,
        M_UINT_16,   M_UINT_32,   M_UINT_64, 
        M_POS_INT_F, M_POS_INT_E, M_NEG_INT_F, M_NEG_INT_E,
        M_FIXEXT_1, M_FIXEXT_2, M_FIXEXT_4, M_FIXEXT_8, M_FIXEXT_16,
        M_EXT_8, M_EXT_16, M_EXT_32,
};

enum m_masks {
      M_MASK_NIL       = 0xC0U,
      M_MASK_TRUE      = 0xC3U,
      M_MASK_FALSE     = 0xC2U,

      M_MASK_ARRAY_16  = 0xDCU,
      M_MASK_ARRAY_32  = 0xDDU,

      M_MASK_MAP_16    = 0xDEU,
      M_MASK_MAP_32    = 0xDFU,

      M_MASK_STR_8     = 0xD9U,
      M_MASK_STR_16    = 0xDAU,
      M_MASK_STR_32    = 0xDBU,

      M_MASK_BIN_8     = 0xC4U,
      M_MASK_BIN_16    = 0xC5U,
      M_MASK_BIN_32    = 0xC6U,

      M_MASK_FIXEXT_1  = 0xD4U,
      M_MASK_FIXEXT_2  = 0xD5U,
      M_MASK_FIXEXT_4  = 0xD6U,
      M_MASK_FIXEXT_8  = 0xD7U,
      M_MASK_FIXEXT_16 = 0xD8U,
      M_MASK_EXT_8     = 0xC7U,
      M_MASK_EXT_16    = 0xC8U,
      M_MASK_EXT_32    = 0xC9U,

      M_MASK_INT_8     = 0xD0U,
      M_MASK_INT_16    = 0xD1U,
      M_MASK_INT_32    = 0xD2U,
      M_MASK_INT_64    = 0xD3U,

      M_MASK_UINT_8    = 0xCCU,
      M_MASK_UINT_16   = 0xCDU,
      M_MASK_UINT_32   = 0xCEU,
      M_MASK_UINT_64   = 0xCFU,

      M_MASK_FLOAT_32  = 0xCAU,
      M_MASK_FLOAT_64  = 0xCBU,

      M_MASK_MAP_F     = 0x80U,
      M_MASK_ARRAY_F   = 0x90U,
      M_MASK_STR_F     = 0xA0U,
      M_MASK_POS_INT_F = 0x00U,
      M_MASK_NEG_INT_F = 0xE0U,
};

P99_DECLARE_STRUCT(mpack_mask);
struct mpack_mask {
        const enum m_groups group;
        const enum m_types  type;
        const bool          fixed;
        const uint8_t       val;
        const uint8_t       shift;
        const char *const   repr;
};

extern const mpack_mask m_masks[];
extern const size_t     m_masks_len;


#ifdef __cplusplus
}
#endif

#endif /* src/mpack_code.h */
