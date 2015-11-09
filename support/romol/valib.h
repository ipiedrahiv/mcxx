#ifndef VALIB_H
#define VALIB_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_VECTOR_LENGTH 64

typedef
union valib_vector_t {
#define INTVEC(bytes) \
    int##bytes##_t i##bytes[MAX_VECTOR_LENGTH]; \
    uint##bytes##_t ui##bytes[MAX_VECTOR_LENGTH];
    INTVEC(8)
    INTVEC(16)
    INTVEC(32)
    INTVEC(64)
#undef INTVEC

    float f[MAX_VECTOR_LENGTH];
    double d[MAX_VECTOR_LENGTH];
    long double ld[MAX_VECTOR_LENGTH];
#if 0
    float128_t f128[MAX_VECTOR_LENGTH];
#endif
} valib_vector_t;

typedef union valib_mask_t
{
    char m[MAX_VECTOR_LENGTH / 8 + !!(MAX_VECTOR_LENGTH % 8)];
} valib_mask_t;

// FIXME - Temporary workaround until we implement the missing bits in vector lowering
static valib_vector_t _force_vector_to_appear;
static valib_mask_t _force_mask_to_appear;

// Aliases to document the directionality
typedef valib_vector_t VDestRegId;
typedef valib_vector_t VSrcRegId;
typedef valib_mask_t MaskDestRegId;
typedef valib_mask_t MaskSrcRegId;

// -------------------------------------------------------
// Vector arithmetic (unmasked)
// -------------------------------------------------------

#define ARITH_BINOP(op) \
void valib_##op##_db_db_db (VDestRegId dest, VSrcRegId src1, VSrcRegId src2); \
void valib_##op##_fl_fl_fl (VDestRegId dest, VSrcRegId src1, VSrcRegId src2); \
\
void valib_##op##_int64_int64_int64 (VDestRegId dest, VSrcRegId src1, VSrcRegId src2); \
void valib_##op##_uint64_uint64_uint64 (VDestRegId dest, VSrcRegId src1, VSrcRegId src2); \
\
void valib_##op##_int32_int32_int32 (VDestRegId dest, VSrcRegId src1, VSrcRegId src2); \
void valib_##op##_uint32_uint32_uint32 (VDestRegId dest, VSrcRegId src1, VSrcRegId src2); \
\
void valib_##op##_int16_int16_int16 (VDestRegId dest, VSrcRegId src1, VSrcRegId src2); \
void valib_##op##_uint16_uint16_uint16 (VDestRegId dest, VSrcRegId src1, VSrcRegId src2); \
\
void valib_##op##_int8_int8_int8 (VDestRegId dest, VSrcRegId src1, VSrcRegId src2); \
void valib_##op##_uint8_uint8_uint8 (VDestRegId dest, VSrcRegId src1, VSrcRegId src2);

// VDEST <- VSRC1 (op) VSRC2
ARITH_BINOP(add)
ARITH_BINOP(sub)
ARITH_BINOP(mul)
ARITH_BINOP(div)

#define ARITH_UNOP(op) \
void valib_##op##_db_db (VDestRegId dest, VSrcRegId src); \
void valib_##op##_fl_fl (VDestRegId dest, VSrcRegId src); \
\
void valib_##op##_int64_int64 (VDestRegId dest, VSrcRegId src); \
void valib_##op##_uint64_uint64 (VDestRegId dest, VSrcRegId src); \
\
void valib_##op##_int32_int32 (VDestRegId dest, VSrcRegId src); \
void valib_##op##_uint32_uint32 (VDestRegId dest, VSrcRegId src); \
\
void valib_##op##_int16_int16 (VDestRegId dest, VSrcRegId src); \
void valib_##op##_uint16_uint16 (VDestRegId dest, VSrcRegId src); \
\
void valib_##op##_int8_int8 (VDestRegId dest, VSrcRegId src); \
void valib_##op##_uint8_uint8 (VDestRegId dest, VSrcRegId src);

// VDEST <- (op) VSRC
ARITH_UNOP(neg)

// -------------------------------------------------------
// Vector arithmetic (masked)
// -------------------------------------------------------

#define ARITH_BINOP_MASK(op) \
void valib_##op##m_db_db_db (VDestRegId dest, VSrcRegId src1, VSrcRegId src2, MaskSrcRegId mr); \
void valib_##op##m_fl_fl_fl (VDestRegId dest, VSrcRegId src1, VSrcRegId src2, MaskSrcRegId mr); \
\
void valib_##op##m_int64_int64_int64 (VDestRegId dest, VSrcRegId src1, VSrcRegId src2, MaskSrcRegId mr); \
void valib_##op##m_uint64_uint64_uint64 (VDestRegId dest, VSrcRegId src1, VSrcRegId src2, MaskSrcRegId mr); \
\
void valib_##op##m_int32_int32_int32 (VDestRegId dest, VSrcRegId src1, VSrcRegId src2, MaskSrcRegId mr); \
void valib_##op##m_uint32_uint32_uint32 (VDestRegId dest, VSrcRegId src1, VSrcRegId src2, MaskSrcRegId mr); \
\
void valib_##op##m_int16_int16_int16 (VDestRegId dest, VSrcRegId src1, VSrcRegId src2, MaskSrcRegId mr); \
void valib_##op##m_uint16_uint16_uint16 (VDestRegId dest, VSrcRegId src1, VSrcRegId src2, MaskSrcRegId mr); \
\
void valib_##op##m_int8_int8_int8 (VDestRegId dest, VSrcRegId src1, VSrcRegId src2, MaskSrcRegId mr); \
void valib_##op##m_uint8_uint8_uint8 (VDestRegId dest, VSrcRegId src1, VSrcRegId src2, MaskSrcRegId mr);

// VDEST <- VSRC1 (op[mask]) VSRC2
// (FIXME: what are the values of VDEST that correspond to unmasked elements?)
ARITH_BINOP_MASK(add)
ARITH_BINOP_MASK(sub)
ARITH_BINOP_MASK(mul)
ARITH_BINOP_MASK(div)

#define ARITH_UNOP_MASK(op) \
void valib_##op##m_db_db (VDestRegId dest, VSrcRegId src, MaskSrcRegId mr); \
void valib_##op##m_fl_fl (VDestRegId dest, VSrcRegId src, MaskSrcRegId mr); \
\
void valib_##op##m_int64_int64 (VDestRegId dest, VSrcRegId src, MaskSrcRegId mr); \
void valib_##op##m_uint64_uint64 (VDestRegId dest, VSrcRegId src, MaskSrcRegId mr); \
\
void valib_##op##m_int32_int32 (VDestRegId dest, VSrcRegId src, MaskSrcRegId mr); \
void valib_##op##m_uint32_uint32 (VDestRegId dest, VSrcRegId src, MaskSrcRegId mr); \
\
void valib_##op##m_int16_int16 (VDestRegId dest, VSrcRegId src, MaskSrcRegId mr); \
void valib_##op##m_uint16_uint16 (VDestRegId dest, VSrcRegId src, MaskSrcRegId mr); \
\
void valib_##op##m_int8_int8 (VDestRegId dest, VSrcRegId src, MaskSrcRegId mr); \
void valib_##op##m_uint8_uint8 (VDestRegId dest, VSrcRegId src, MaskSrcRegId mr);

ARITH_UNOP_MASK(neg)

// -------------------------------------------------------
// Mixed vector/scalar arithmetic (unmasked)
// -------------------------------------------------------

#define ARITH_BINOP_SCALAR(op) \
void valib_##op##s_db_db_db (VDestRegId dest, VSrcRegId src1, double src2, MaskSrcRegId mr); \
void valib_##op##s_fl_fl_fl (VDestRegId dest, VSrcRegId src1, float src2, MaskSrcRegId mr); \
\
void valib_##op##s_int64_int64_int64 (VDestRegId dest, VSrcRegId src1, int64_t src2, MaskSrcRegId mr); \
void valib_##op##s_uint64_uint64_uint64 (VDestRegId dest, VSrcRegId src1, uint64_t src2, MaskSrcRegId mr); \
\
void valib_##op##s_int32_int32_int32 (VDestRegId dest, VSrcRegId src1, int32_t src2, MaskSrcRegId mr); \
void valib_##op##s_uint32_uint32_uint32 (VDestRegId dest, VSrcRegId src1, uint32_t src2, MaskSrcRegId mr); \
\
void valib_##op##s_int16_int16_int16 (VDestRegId dest, VSrcRegId src1, int16_t src2, MaskSrcRegId mr); \
void valib_##op##s_uint16_uint16_uint16 (VDestRegId dest, VSrcRegId src1, uint16_t src2, MaskSrcRegId mr); \
\
void valib_##op##s_int8_int8_int8 (VDestRegId dest, VSrcRegId src1, int8_t src2, MaskSrcRegId mr); \
void valib_##op##s_uint8_uint8_uint8 (VDestRegId dest, VSrcRegId src1, int8_t src2, MaskSrcRegId mr);

// VDEST <- VSRC1 (op) scalar_SRC2
ARITH_BINOP_SCALAR(add)
ARITH_BINOP_SCALAR(sub)
ARITH_BINOP_SCALAR(mul)
ARITH_BINOP_SCALAR(div)

// -------------------------------------------------------
// Mixed vector/scalar arithmetic (masked)
// -------------------------------------------------------

#define ARITH_BINOP_SCALAR_MASK(op) \
void valib_##op##sm_db_db_db (VDestRegId dest, VSrcRegId src1, double src2, MaskSrcRegId mr); \
void valib_##op##sm_fl_fl_fl (VDestRegId dest, VSrcRegId src1, float src2, MaskSrcRegId mr); \
\
void valib_##op##sm_int64_int64_int64 (VDestRegId dest, VSrcRegId src1, int64_t src2, MaskSrcRegId mr); \
void valib_##op##sm_uint64_uint64_uint64 (VDestRegId dest, VSrcRegId src1, uint64_t src2, MaskSrcRegId mr); \
\
void valib_##op##sm_int32_int32_int32 (VDestRegId dest, VSrcRegId src1, int32_t src2, MaskSrcRegId mr); \
void valib_##op##sm_uint32_uint32_uint32 (VDestRegId dest, VSrcRegId src1, uint32_t src2, MaskSrcRegId mr); \
\
void valib_##op##sm_int16_int16_int16 (VDestRegId dest, VSrcRegId src1, int16_t src2, MaskSrcRegId mr); \
void valib_##op##sm_uint16_uint16_uint16 (VDestRegId dest, VSrcRegId src1, uint16_t src2, MaskSrcRegId mr); \
\
void valib_##op##sm_int8_int8_int8 (VDestRegId dest, VSrcRegId src1, int8_t src2, MaskSrcRegId mr); \
void valib_##op##sm_uint8_uint8_uint8 (VDestRegId dest, VSrcRegId src1, int8_t src2, MaskSrcRegId mr);

// VDEST <- VSRC1 (op[mask]) scalar_SRC2
// (FIXME: what are the values of VDEST that correspond to unmasked elements?)
ARITH_BINOP_SCALAR_MASK(add)
ARITH_BINOP_SCALAR_MASK(sub)
ARITH_BINOP_SCALAR_MASK(mul)
ARITH_BINOP_SCALAR_MASK(div)

// -------------------------------------------------------
// Bitwise operations (vector)
// -------------------------------------------------------

#define ELEMENTWISE_BIT_BINOP(op) \
void valib_##op##_int64_int64_int64 (VDestRegId dest, VSrcRegId src1, VSrcRegId src2); \
void valib_##op##_uint64_uint64_uint64 (VDestRegId dest, VSrcRegId src1, VSrcRegId src2); \
\
void valib_##op##_int32_int32_int32 (VDestRegId dest, VSrcRegId src1, VSrcRegId src2); \
void valib_##op##_uint32_uint32_uint32 (VDestRegId dest, VSrcRegId src1, VSrcRegId src2); \
\
void valib_##op##_int16_int16_int16 (VDestRegId dest, VSrcRegId src1, VSrcRegId src2); \
void valib_##op##_uint16_uint16_uint16 (VDestRegId dest, VSrcRegId src1, VSrcRegId src2); \
\
void valib_##op##_int8_int8_int8 (VDestRegId dest, VSrcRegId src1, VSrcRegId src2); \
void valib_##op##_uint8_uint8_uint8 (VDestRegId dest, VSrcRegId src1, VSrcRegId src2);

ELEMENTWISE_BIT_BINOP(lsl)
ELEMENTWISE_BIT_BINOP(lsr)
ELEMENTWISE_BIT_BINOP(asr)

#define BITWISE_BINOP(op) \
void valib_##op(VDestRegId dest, VSrcRegId src1, VSrcRegId src2);

BITWISE_BINOP(or)
BITWISE_BINOP(and)
BITWISE_BINOP(xor)
BITWISE_BINOP(lsl)
BITWISE_BINOP(lsr)
BITWISE_BINOP(asr)

#define BITWISE_UNOP(op) \
void valib_##op##b (VDestRegId dest, VSrcRegId src1, VSrcRegId src2);

BITWISE_UNOP(not)

// -------------------------------------------------------
// Relational operators
// -------------------------------------------------------
#define RELATIONAL_OP(op) \
void valib_##op##_db_db (MaskSrcRegId mrdest, VSrcRegId src1, VSrcRegId src2); \
void valib_##op##_fl_fl (MaskSrcRegId mrdest, VSrcRegId src1, VSrcRegId src2); \
\
void valib_##op##_int64_int64 (MaskSrcRegId mrdest, VSrcRegId src1, VSrcRegId src2); \
void valib_##op##_uint64_uint64 (MaskSrcRegId mrdest, VSrcRegId src1, VSrcRegId src2); \
\
void valib_##op##_int32_int32 (MaskSrcRegId mrdest, VSrcRegId src1, VSrcRegId src2); \
void valib_##op##_uint32_uint32 (MaskSrcRegId mrdest, VSrcRegId src1, VSrcRegId src2); \
\
void valib_##op##_int16_int16 (MaskSrcRegId mrdest, VSrcRegId src1, VSrcRegId src2); \
void valib_##op##_uint16_uint16 (MaskSrcRegId mrdest, VSrcRegId src1, VSrcRegId src2); \
\
void valib_##op##_int8_int8 (MaskSrcRegId mrdest, VSrcRegId src1, VSrcRegId src2); \
void valib_##op##_uint8_uint8 (MaskSrcRegId mrdest, VSrcRegId src1, VSrcRegId src2);

RELATIONAL_OP(eq)
RELATIONAL_OP(ne)
RELATIONAL_OP(lt)
RELATIONAL_OP(le)
RELATIONAL_OP(gt)
RELATIONAL_OP(ge)

// -------------------------------------------------------
// Load/Store
// -------------------------------------------------------

// DEST <- *ADDRESS
void valib_ld_db ( VDestRegId dest, double * address);
void valib_ld_fl ( VDestRegId dest, float * address);
void valib_ld_int64 ( VDestRegId dest, int64_t * address);
void valib_ld_int32 ( VDestRegId dest, int32_t * address);
void valib_ld_int16 ( VDestRegId dest, int16_t * address);
void valib_ld_int8 ( VDestRegId dest, int8_t * address);

void valib_ldm_db ( VDestRegId dest, double * address, MaskSrcRegId msrc);
void valib_ldm_fl ( VDestRegId dest, float * address, MaskSrcRegId msrc);
void valib_ldm_int64 ( VDestRegId dest, int64_t * address, MaskSrcRegId msrc);
void valib_ldm_int32 ( VDestRegId dest, int32_t * address, MaskSrcRegId msrc);
void valib_ldm_int16 ( VDestRegId dest, int16_t * address, MaskSrcRegId msrc);
void valib_ldm_int8 ( VDestRegId dest, int8_t * address, MaskSrcRegId msrc);

// *ADDRESS <- SRC
void valib_st_db ( VSrcRegId src, double * address);
void valib_st_fl ( VSrcRegId src, float * address);
void valib_st_int64 ( VSrcRegId src, int64_t * address);
void valib_st_int32 ( VSrcRegId src, int32_t * address);
void valib_st_int16 ( VSrcRegId src, int16_t * address);
void valib_st_int8 ( VSrcRegId src, int8_t * address);

void valib_stm_db ( VSrcRegId src, double * address, MaskSrcRegId msrc);
void valib_stm_fl ( VSrcRegId src, float * address, MaskSrcRegId msrc);
void valib_stm_int64 ( VSrcRegId src, int64_t * address, MaskSrcRegId msrc);
void valib_stm_int32 ( VSrcRegId src, int32_t * address, MaskSrcRegId msrc);
void valib_stm_int16 ( VSrcRegId src, int16_t * address, MaskSrcRegId msrc);
void valib_stm_int8 ( VSrcRegId src, int8_t * address, MaskSrcRegId msrc);

// -------------------------------------------------------
// Gather/Scatter
// -------------------------------------------------------

/* TBD */

// -------------------------------------------------------
// Vector reductions
// -------------------------------------------------------

#define VECTOR_RED(op) \
double valib_red_##op##_db (VSrcRegId src); \
float valib_red_##op##_fl (VSrcRegId src); \
\
int64_t valib_red_##op##_int64 (VSrcRegId src); \
uint64_t valib_red_##op##_uint64 (VSrcRegId src); \
\
int32_t valib_red_##op##_int32 (VSrcRegId src); \
uint32_t valib_red_##op##_uint32 (VSrcRegId src); \
\
int16_t valib_red_##op##_int16 (VSrcRegId src); \
uint16_t valib_red_##op##_uint16 (VSrcRegId src); \
\
int8_t valib_red_##op##_int8 (VSrcRegId src); \
uint8_t valib_red_##op##_uint8 (VSrcRegId src);

VECTOR_RED(add)
VECTOR_RED(sub)
VECTOR_RED(max)
VECTOR_RED(min)

// -------------------------------------------------------
// Vector architectural state
// -------------------------------------------------------

uint32_t valib_get_vector_length(void);
void     valib_set_vector_length(uint32_t);

uint32_t valib_get_max_vector_length(void);

// -------------------------------------------------------
// Vector value movement
// -------------------------------------------------------

void valib_mov(VDestRegId dest, VSrcRegId src);
void valib_movm(VDestRegId dest, VSrcRegId src, MaskSrcRegId mr);

// -------------------------------------------------------
// Scalar to vector promotion
// -------------------------------------------------------

void valib_set_db (VDestRegId dest, double src);
void valib_set_fl (VDestRegId dest, float src);
void valib_set_int64 (VDestRegId dest, int64_t src);
void valib_set_uint64 (VDestRegId dest, uint64_t src);
void valib_set_int32 (VDestRegId dest, int32_t src);
void valib_set_uint32 (VDestRegId dest, uint32_t src);
void valib_set_int16 (VDestRegId dest, int16_t src);
void valib_set_uint16 (VDestRegId dest, uint16_t src);
void valib_set_int8 (VDestRegId dest, int8_t src);
void valib_set_uint8 (VDestRegId dest, uint8_t src);

// -------------------------------------------------------
// Vector conversions (unmasked)
// -------------------------------------------------------
// valib_cvm_SRC_DEST

// Integer conversions/promotions

void valib_cv_int8_int16(VDestRegId dest, VDestRegId src);
void valib_cv_int8_int32(VDestRegId dest, VDestRegId src);
void valib_cv_int8_int64(VDestRegId dest, VDestRegId src);

void valib_cv_int16_int8(VDestRegId dest, VDestRegId src);
void valib_cv_int16_int32(VDestRegId dest, VDestRegId src);
void valib_cv_int16_int64(VDestRegId dest, VDestRegId src);

void valib_cv_int32_int8(VDestRegId dest, VDestRegId src);
void valib_cv_int32_int16(VDestRegId dest, VDestRegId src);
void valib_cv_int32_int64(VDestRegId dest, VDestRegId src);

void valib_cv_int64_int8(VDestRegId dest, VDestRegId src);
void valib_cv_int64_int16(VDestRegId dest, VDestRegId src);
void valib_cv_int64_int32(VDestRegId dest, VDestRegId src);

void valib_cv_uint8_uint16(VDestRegId dest, VDestRegId src);
void valib_cv_uint8_uint32(VDestRegId dest, VDestRegId src);
void valib_cv_uint8_uint64(VDestRegId dest, VDestRegId src);

void valib_cv_uint16_uint8(VDestRegId dest, VDestRegId src);
void valib_cv_uint16_uint32(VDestRegId dest, VDestRegId src);
void valib_cv_uint16_uint64(VDestRegId dest, VDestRegId src);

void valib_cv_uint32_uint8(VDestRegId dest, VDestRegId src);
void valib_cv_uint32_uint16(VDestRegId dest, VDestRegId src);
void valib_cv_uint32_uint64(VDestRegId dest, VDestRegId src);

void valib_cv_uint64_uint8(VDestRegId dest, VDestRegId src);
void valib_cv_uint64_uint16(VDestRegId dest, VDestRegId src);
void valib_cv_uint64_uint32(VDestRegId dest, VDestRegId src);

void valib_cv_uint8_int16(VDestRegId dest, VDestRegId src);
void valib_cv_uint8_int32(VDestRegId dest, VDestRegId src);
void valib_cv_uint8_int64(VDestRegId dest, VDestRegId src);

void valib_cv_uint16_int8(VDestRegId dest, VDestRegId src);
void valib_cv_uint16_int32(VDestRegId dest, VDestRegId src);
void valib_cv_uint16_int64(VDestRegId dest, VDestRegId src);

void valib_cv_uint32_int8(VDestRegId dest, VDestRegId src);
void valib_cv_uint32_int16(VDestRegId dest, VDestRegId src);
void valib_cv_uint32_int64(VDestRegId dest, VDestRegId src);

void valib_cv_uint64_int8(VDestRegId dest, VDestRegId src);
void valib_cv_uint64_int16(VDestRegId dest, VDestRegId src);
void valib_cv_uint64_int32(VDestRegId dest, VDestRegId src);

void valib_cv_int8_uint16(VDestRegId dest, VDestRegId src);
void valib_cv_int8_uint32(VDestRegId dest, VDestRegId src);
void valib_cv_int8_uint64(VDestRegId dest, VDestRegId src);

void valib_cv_int16_uint8(VDestRegId dest, VDestRegId src);
void valib_cv_int16_uint32(VDestRegId dest, VDestRegId src);
void valib_cv_int16_uint64(VDestRegId dest, VDestRegId src);

void valib_cv_int32_uint8(VDestRegId dest, VDestRegId src);
void valib_cv_int32_uint16(VDestRegId dest, VDestRegId src);
void valib_cv_int32_uint64(VDestRegId dest, VDestRegId src);

void valib_cv_int64_uint8(VDestRegId dest, VDestRegId src);
void valib_cv_int64_uint16(VDestRegId dest, VDestRegId src);
void valib_cv_int64_uint32(VDestRegId dest, VDestRegId src);

// Floating conversions/promotions

void valib_cv_int8_fl(VDestRegId dest, VDestRegId src);
void valib_cv_int16_fl(VDestRegId dest, VDestRegId src);
void valib_cv_int32_fl(VDestRegId dest, VDestRegId src);
void valib_cv_int64_fl(VDestRegId dest, VDestRegId src);

void valib_cv_int8_db(VDestRegId dest, VDestRegId src);
void valib_cv_int16_db(VDestRegId dest, VDestRegId src);
void valib_cv_int32_db(VDestRegId dest, VDestRegId src);
void valib_cv_int64_db(VDestRegId dest, VDestRegId src);

void valib_cv_fl_int8(VDestRegId dest, VDestRegId src);
void valib_cv_fl_int16(VDestRegId dest, VDestRegId src);
void valib_cv_fl_int32(VDestRegId dest, VDestRegId src);
void valib_cv_fl_int64(VDestRegId dest, VDestRegId src);

void valib_cv_db_int8(VDestRegId dest, VDestRegId src);
void valib_cv_db_int16(VDestRegId dest, VDestRegId src);
void valib_cv_db_int32(VDestRegId dest, VDestRegId src);
void valib_cv_db_int64(VDestRegId dest, VDestRegId src);

void valib_cv_uint8_fl(VDestRegId dest, VDestRegId src);
void valib_cv_uint16_fl(VDestRegId dest, VDestRegId src);
void valib_cv_uint32_fl(VDestRegId dest, VDestRegId src);
void valib_cv_uint64_fl(VDestRegId dest, VDestRegId src);

void valib_cv_uint8_db(VDestRegId dest, VDestRegId src);
void valib_cv_uint16_db(VDestRegId dest, VDestRegId src);
void valib_cv_uint32_db(VDestRegId dest, VDestRegId src);
void valib_cv_uint64_db(VDestRegId dest, VDestRegId src);

void valib_cv_fl_uint8(VDestRegId dest, VDestRegId src);
void valib_cv_fl_uint16(VDestRegId dest, VDestRegId src);
void valib_cv_fl_uint32(VDestRegId dest, VDestRegId src);
void valib_cv_fl_uint64(VDestRegId dest, VDestRegId src);

void valib_cv_db_uint8(VDestRegId dest, VDestRegId src);
void valib_cv_db_uint16(VDestRegId dest, VDestRegId src);
void valib_cv_db_uint32(VDestRegId dest, VDestRegId src);
void valib_cv_db_uint64(VDestRegId dest, VDestRegId src);

void valib_cv_db_fl(VDestRegId dest, VDestRegId src);
void valib_cv_fl_db(VDestRegId dest, VDestRegId src);

// -------------------------------------------------------
// Vector conversions (masked)
// -------------------------------------------------------
// valib_cvm_SRC_DEST

// Integer conversions/promotions

void valib_cvm_int8_int16(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_int8_int32(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_int8_int64(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);

void valib_cvm_int16_int8(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_int16_int32(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_int16_int64(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);

void valib_cvm_int32_int8(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_int32_int16(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_int32_int64(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);

void valib_cvm_int64_int8(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_int64_int16(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_int64_int32(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);

void valib_cvm_uint8_uint16(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_uint8_uint32(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_uint8_uint64(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);

void valib_cvm_uint16_uint8(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_uint16_uint32(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_uint16_uint64(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);

void valib_cvm_uint32_uint8(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_uint32_uint16(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_uint32_uint64(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);

void valib_cvm_uint64_uint8(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_uint64_uint16(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_uint64_uint32(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);

void valib_cvm_uint8_int16(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_uint8_int32(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_uint8_int64(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);

void valib_cvm_uint16_int8(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_uint16_int32(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_uint16_int64(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);

void valib_cvm_uint32_int8(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_uint32_int16(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_uint32_int64(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);

void valib_cvm_uint64_int8(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_uint64_int16(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_uint64_int32(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);

void valib_cvm_int8_uint16(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_int8_uint32(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_int8_uint64(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);

void valib_cvm_int16_uint8(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_int16_uint32(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_int16_uint64(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);

void valib_cvm_int32_uint8(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_int32_uint16(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_int32_uint64(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);

void valib_cvm_int64_uint8(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_int64_uint16(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_int64_uint32(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);

// Floating conversions/promotions

void valib_cvm_int8_fl(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_int16_fl(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_int32_fl(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_int64_fl(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);

void valib_cvm_int8_db(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_int16_db(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_int32_db(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_int64_db(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);

void valib_cvm_fl_int8(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_fl_int16(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_fl_int32(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_fl_int64(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);

void valib_cvm_db_int8(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_db_int16(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_db_int32(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_db_int64(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);

void valib_cvm_uint8_fl(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_uint16_fl(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_uint32_fl(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_uint64_fl(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);

void valib_cvm_uint8_db(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_uint16_db(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_uint32_db(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_uint64_db(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);

void valib_cvm_fl_uint8(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_fl_uint16(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_fl_uint32(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_fl_uint64(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);

void valib_cvm_db_uint8(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_db_uint16(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_db_uint32(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_db_uint64(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);

void valib_cvm_db_fl(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);
void valib_cvm_fl_db(VDestRegId dest, VDestRegId src, MaskSrcRegId mask);

// -------------------------------------------------------
// Mask operations
// -------------------------------------------------------

void valib_mask_mov(MaskDestRegId dest, MaskSrcRegId src);

void valib_mask_all(MaskDestRegId dest);
void valib_mask_none(MaskDestRegId dest);

bool valib_mask_is_zero(MaskSrcRegId src);

void valib_mask_or(MaskDestRegId dest, MaskSrcRegId src1, MaskSrcRegId src2);
void valib_mask_and(MaskDestRegId dest, MaskSrcRegId src1, MaskSrcRegId src2);
void valib_mask_xor(MaskDestRegId dest, MaskSrcRegId src1, MaskSrcRegId src2);

int32_t valib_mask_clz(MaskSrcRegId src); // count leading zeros
int32_t valib_mask_ctz(MaskSrcRegId src); // count trailing zeros
int32_t valib_mask_popcount(MaskSrcRegId src);

// -------------------------------------------------------
// Insert/extract
// -------------------------------------------------------

/* TBD. Probably not needed */

#endif // VALIB_H
