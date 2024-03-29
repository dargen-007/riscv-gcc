/* { dg-do compile  } */
/* { dg-require-effective-target arm_v8_1m_mve_ok } */
/* { dg-add-options arm_v8_1m_mve } */
/* { dg-additional-options "-O2" } */

#include "arm_mve.h"

int32_t
foo (int16x8_t a, mve_pred16_t p)
{
  return vaddvq_p_s16 (a, p);
}

/* { dg-final { scan-assembler "vaddvt.s16"  }  } */

int32_t
foo1 (int16x8_t a, mve_pred16_t p)
{
  return vaddvq_p (a, p);
}

/* { dg-final { scan-assembler "vaddvt.s16"  }  } */
