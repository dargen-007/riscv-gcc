/* { dg-do compile  } */
/* { dg-require-effective-target arm_v8_1m_mve_ok } */
/* { dg-add-options arm_v8_1m_mve } */
/* { dg-additional-options "-O2" } */

#include "arm_mve.h"

uint16x8_t
foo (uint16x8_t a, uint32x4_t b)
{
  return vqrshrnbq_n_u32 (a, b, 1);
}

/* { dg-final { scan-assembler "vqrshrnb.u32"  }  } */

uint16x8_t
foo1 (uint16x8_t a, uint32x4_t b)
{
  return vqrshrnbq (a, b, 1);
}

/* { dg-final { scan-assembler "vqrshrnb.u32"  }  } */
