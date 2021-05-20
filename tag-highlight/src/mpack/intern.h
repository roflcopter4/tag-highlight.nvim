#ifndef SRC_MPACK_CODE_H
#define SRC_MPACK_CODE_H

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
        M_UINT_16,   M_UINT_32,   M_UINT_64,   M_POS_INT_F, M_POS_INT_E, M_NEG_INT_F,
        M_NEG_INT_E, M_EXT_8,     M_EXT_16,    M_EXT_32,    M_EXT_F1,    M_EXT_F2,
        M_EXT_F4,
};

#define M_MASK_NIL       0xC0U
#define M_MASK_TRUE      0xC3U
#define M_MASK_FALSE     0xC2U
#define M_MASK_ARRAY_F   0x90U
#define M_MASK_ARRAY_16  0xDCU
#define M_MASK_ARRAY_32  0xDDU
#define M_MASK_MAP_F     0x80U
#define M_MASK_MAP_16    0xDEU
#define M_MASK_MAP_32    0xDFU
#define M_MASK_STR_F     0xA0U
#define M_MASK_STR_8     0xD9U
#define M_MASK_STR_16    0xDAU
#define M_MASK_STR_32    0xDBU
#define M_MASK_BIN_8     0xC4U
#define M_MASK_BIN_16    0xC5U
#define M_MASK_BIN_32    0xC6U
#define M_MASK_EXT_8     0xD4U
#define M_MASK_EXT_16    0xD5U
#define M_MASK_EXT_32    0xD6U
#define M_MASK_INT_8     0xD0U
#define M_MASK_INT_16    0xD1U
#define M_MASK_INT_32    0xD2U
#define M_MASK_INT_64    0xD3U
#define M_MASK_UINT_8    0xCCU
#define M_MASK_UINT_16   0xCDU
#define M_MASK_UINT_32   0xCEU
#define M_MASK_UINT_64   0xCFU
#define M_MASK_POS_INT_F 0x00U
#define M_MASK_NEG_INT_F 0xE0U

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
