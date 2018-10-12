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

#define M_MASK_NIL       0xC0u                                                           
#define M_MASK_TRUE      0xC3u                                                           
#define M_MASK_FALSE     0xC2u                                                         
#define M_MASK_ARRAY_F   0x90u                                                         
#define M_MASK_ARRAY_16  0xDCu
#define M_MASK_ARRAY_32  0xDDu
#define M_MASK_MAP_F     0x80u
#define M_MASK_MAP_16    0xDEu
#define M_MASK_MAP_32    0xDFu
#define M_MASK_STR_F     0xA0u
#define M_MASK_STR_8     0xD9u
#define M_MASK_STR_16    0xDAu
#define M_MASK_STR_32    0xDBu
#define M_MASK_BIN_8     0xC4u
#define M_MASK_BIN_16    0xC5u
#define M_MASK_BIN_32    0xC6u
#define M_MASK_EXT_8     0xD4u
#define M_MASK_EXT_16    0xD5u
#define M_MASK_EXT_32    0xD6u
#define M_MASK_INT_8     0xD0u
#define M_MASK_INT_16    0xD1u
#define M_MASK_INT_32    0xD2u
#define M_MASK_INT_64    0xD3u
#define M_MASK_UINT_8    0xCCu
#define M_MASK_UINT_16   0xCDu
#define M_MASK_UINT_32   0xCEu
#define M_MASK_UINT_64   0xCFu
#define M_MASK_POS_INT_F 0x00u
#define M_MASK_NEG_INT_F 0xE0u

#define M_ARRAY_F_MAX 15u

#ifndef __cplusplus
typedef struct mpack_mask mpack_mask;
#endif

struct mpack_mask {
        const enum m_groups group;
        const enum m_types  type;
        const bool          fixed;
        const uint8_t       val;
        const uint8_t       shift;
        const char *const   repr;
};

extern const mpack_mask m_masks[];
extern const size_t      m_masks_len;


#ifdef __cplusplus
}
#endif

#endif /* src/mpack_code.h */
