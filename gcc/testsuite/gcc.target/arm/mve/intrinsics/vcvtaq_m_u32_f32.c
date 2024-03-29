/* { dg-do compile  } */
/* { dg-require-effective-target arm_v8_1m_mve_fp_ok } */
/* { dg-add-options arm_v8_1m_mve_fp } */
/* { dg-additional-options "-O2" } */

#include "arm_mve.h"

uint32x4_t
foo (uint32x4_t inactive, float32x4_t a, mve_pred16_t p)
{
  return vcvtaq_m_u32_f32 (inactive, a, p);
}

/* { dg-final { scan-assembler "vpst" } } */
/* { dg-final { scan-assembler "vcvtat.u32.f32"  }  } */

uint32x4_t
foo1 (uint32x4_t inactive, float32x4_t a, mve_pred16_t p)
{
  return vcvtaq_m (inactive, a, p);
}

/* { dg-final { scan-assembler "vpst" } } */
