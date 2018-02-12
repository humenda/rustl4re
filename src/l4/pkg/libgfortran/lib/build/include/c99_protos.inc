#ifndef C99_PROTOS_H
#define C99_PROTOS_H 1
#ifndef HAVE_ACOSF
#define HAVE_ACOSF 1
#endif
#if HAVE_ACOSH && !HAVE_ACOSHF
#define HAVE_ACOSHF 1
#endif
#ifndef HAVE_ASINF
#define HAVE_ASINF 1
#endif
#if HAVE_ASINH && !HAVE_ASINHF
#define HAVE_ASINHF 1
#endif
#ifndef HAVE_ATAN2F
#define HAVE_ATAN2F 1
#endif
#ifndef HAVE_ATANF
#define HAVE_ATANF 1
#endif
#if HAVE_ATANH && !HAVE_ATANHF
#define HAVE_ATANHF 1
#endif
#ifndef HAVE_CEILF
#define HAVE_CEILF 1
#endif
#ifndef HAVE_COPYSIGNF
#define HAVE_COPYSIGNF 1
#endif
#ifndef HAVE_COSF
#define HAVE_COSF 1
#endif
#ifndef HAVE_COSHF
#define HAVE_COSHF 1
#endif
#ifndef HAVE_EXPF
#define HAVE_EXPF 1
#endif
#ifndef HAVE_FABSF
#define HAVE_FABSF 1
#endif
#ifndef HAVE_FLOORF
#define HAVE_FLOORF 1
#endif
#ifndef HAVE_FLOORL
#define HAVE_FLOORL 1
#endif
#ifndef HAVE_FMODF
#define HAVE_FMODF 1
#endif
#ifndef HAVE_FMODL
#define HAVE_FMODL 1
#endif
#ifndef HAVE_FREXPF
#define HAVE_FREXPF 1
#endif
#ifndef HAVE_HYPOTF
#define HAVE_HYPOTF 1
#endif
#ifndef HAVE_LOGF
#define HAVE_LOGF 1
#endif
#ifndef HAVE_LOG10F
#define HAVE_LOG10F 1
#endif
#ifndef HAVE_SCALBN
#define HAVE_SCALBN 1
#endif
#ifndef HAVE_SCALBNF
#define HAVE_SCALBNF 1
#endif
#ifndef HAVE_SINF
#define HAVE_SINF 1
#endif
#ifndef HAVE_SINHF
#define HAVE_SINHF 1
#endif
#ifndef HAVE_SQRTF
#define HAVE_SQRTF 1
#endif
#ifndef HAVE_TANF
#define HAVE_TANF 1
#endif
#ifndef HAVE_TANHF
#define HAVE_TANHF 1
#endif
#ifndef HAVE_TRUNC
#define HAVE_TRUNC 1
#endif
#ifndef HAVE_TRUNCF
#define HAVE_TRUNCF 1
#endif
#ifndef HAVE_NEXTAFTERF
#define HAVE_NEXTAFTERF 1
#endif
#ifndef HAVE_POWF
#define HAVE_POWF 1
#endif
#ifndef HAVE_ROUND
#define HAVE_ROUND 1
#endif
#ifndef HAVE_ROUNDF
#define HAVE_ROUNDF 1
#endif
#if !defined(HAVE_ROUNDL)
#define HAVE_ROUNDL 1
#endif
#if !defined(HAVE_LROUNDF) && defined(HAVE_ROUNDF)
#define HAVE_LROUNDF 1
#endif
#if !defined(HAVE_LROUND) && defined(HAVE_ROUND)
#define HAVE_LROUND 1
#endif
#if !defined(HAVE_LROUNDL) && defined(HAVE_ROUNDL)
#define HAVE_LROUNDL 1
#endif
#if !defined(HAVE_LLROUNDF) && defined(HAVE_ROUNDF)
#define HAVE_LLROUNDF 1
#endif
#if !defined(HAVE_LLROUND) && defined(HAVE_ROUND)
#define HAVE_LLROUND 1
#endif
#if !defined(HAVE_LLROUNDL) && defined(HAVE_ROUNDL)
#define HAVE_LLROUNDL 1
#endif
#if defined(HAVE_J0) && ! defined(HAVE_J0F)
#define HAVE_J0F 1
#endif
#if defined(HAVE_J1) && !defined(HAVE_J1F)
#define HAVE_J1F 1
#endif
#if defined(HAVE_JN) && !defined(HAVE_JNF)
#define HAVE_JNF 1
#endif
#if defined(HAVE_Y0) && !defined(HAVE_Y0F)
#define HAVE_Y0F 1
#endif
#if defined(HAVE_Y1) && !defined(HAVE_Y1F)
#define HAVE_Y1F 1
#endif
#if defined(HAVE_YN) && !defined(HAVE_YNF)
#define HAVE_YNF 1
#endif
#if defined(HAVE_ERF) && !defined(HAVE_ERFF)
#define HAVE_ERFF 1
#endif
#if defined(HAVE_ERFC) && !defined(HAVE_ERFCF)
#define HAVE_ERFCF 1
#endif
#ifndef HAVE_LOG10L
#define HAVE_LOG10L 1
#endif
#if !defined(HAVE_CABSF)
#define HAVE_CABSF 1
#endif
#if !defined(HAVE_CABS)
#define HAVE_CABS 1
#endif
#if !defined(HAVE_CABSL) && defined(HAVE_HYPOTL)
#define HAVE_CABSL 1
#endif
#if !defined(HAVE_CARGF)
#define HAVE_CARGF 1
#endif
#if !defined(HAVE_CARG)
#define HAVE_CARG 1
#endif
#if !defined(HAVE_CARGL) && defined(HAVE_ATAN2L)
#define HAVE_CARGL 1
#endif
#if !defined(HAVE_CEXPF)
#define HAVE_CEXPF 1
#endif
#if !defined(HAVE_CEXP)
#define HAVE_CEXP 1
#endif
#if !defined(HAVE_CEXPL) && defined(HAVE_COSL) && defined(HAVE_SINL) && defined(HAVE_EXPL)
#define HAVE_CEXPL 1
#endif
#if !defined(HAVE_CLOGF)
#define HAVE_CLOGF 1
#endif
#if !defined(HAVE_CLOG)
#define HAVE_CLOG 1
#endif
#if !defined(HAVE_CLOGL) && defined(HAVE_LOGL) && defined(HAVE_CABSL) && defined(HAVE_CARGL)
#define HAVE_CLOGL 1
#endif
#if !defined(HAVE_CLOG10F)
#define HAVE_CLOG10F 1
#endif
#if !defined(HAVE_CLOG10)
#define HAVE_CLOG10 1
#endif
#if !defined(HAVE_CLOG10L) && defined(HAVE_LOG10L) && defined(HAVE_CABSL) && defined(HAVE_CARGL)
#define HAVE_CLOG10L 1
#endif
#if !defined(HAVE_CPOWF)
#define HAVE_CPOWF 1
#endif
#if !defined(HAVE_CPOW)
#define HAVE_CPOW 1
#endif
#if !defined(HAVE_CPOWL) && defined(HAVE_CEXPL) && defined(HAVE_CLOGL)
#define HAVE_CPOWL 1
#endif
#if !defined(HAVE_CSQRTF)
#define HAVE_CSQRTF 1 
#endif
#if !defined(HAVE_CSQRT)
#define HAVE_CSQRT 1
#endif
#if !defined(HAVE_CSQRTL) && defined(HAVE_COPYSIGNL) && defined(HAVE_SQRTL) && defined(HAVE_FABSL) && defined(HAVE_HYPOTL)
#define HAVE_CSQRTL 1
#endif
#if !defined(HAVE_CSINHF)
#define HAVE_CSINHF 1
#endif
#if !defined(HAVE_CSINH)
#define HAVE_CSINH 1
#endif
#if !defined(HAVE_CSINHL) && defined(HAVE_COSL) && defined(HAVE_COSHL) && defined(HAVE_SINL) && defined(HAVE_SINHL)
#define HAVE_CSINHL 1
#endif
#if !defined(HAVE_CCOSHF)
#define HAVE_CCOSHF 1
#endif
#if !defined(HAVE_CCOSH)
#define HAVE_CCOSH 1
#endif
#if !defined(HAVE_CCOSHL) && defined(HAVE_COSL) && defined(HAVE_COSHL) && defined(HAVE_SINL) && defined(HAVE_SINHL)
#define HAVE_CCOSHL 1
#endif
#if !defined(HAVE_CTANHF)
#define HAVE_CTANHF 1
#endif
#if !defined(HAVE_CTANH)
#define HAVE_CTANH 1
#endif
#if !defined(HAVE_CTANHL) && defined(HAVE_TANL) && defined(HAVE_TANHL)
#define HAVE_CTANHL 1
#endif
#if !defined(HAVE_CSINF)
#define HAVE_CSINF 1
#endif
#if !defined(HAVE_CSIN)
#define HAVE_CSIN 1
#endif
#if !defined(HAVE_CSINL) && defined(HAVE_COSL) && defined(HAVE_COSHL) && defined(HAVE_SINL) && defined(HAVE_SINHL)
#define HAVE_CSINL 1
#endif
#if !defined(HAVE_CCOSF)
#define HAVE_CCOSF 1
#endif
#if !defined(HAVE_CCOS)
#define HAVE_CCOS 1
#endif
#if !defined(HAVE_CCOSL) && defined(HAVE_COSL) && defined(HAVE_COSHL) && defined(HAVE_SINL) && defined(HAVE_SINHL)
#define HAVE_CCOSL 1
#endif
#if !defined(HAVE_CTANF)
#define HAVE_CTANF 1
#endif
#if !defined(HAVE_CTAN)
#define HAVE_CTAN 1
#endif
#if !defined(HAVE_CTANL) && defined(HAVE_TANL) && defined(HAVE_TANHL)
#define HAVE_CTANL 1
#endif
#if !defined(HAVE_CACOSF) && defined(HAVE_CLOGF) && defined(HAVE_CSQRTF)
#define HAVE_CACOSF 1
#endif
#if !defined(HAVE_CACOS) && defined(HAVE_CLOG) && defined(HAVE_CSQRT)
#define HAVE_CACOS 1
#endif
#if !defined(HAVE_CACOSL) && defined(HAVE_CLOGL) && defined(HAVE_CSQRTL)
#define HAVE_CACOSL 1
#endif
#if !defined(HAVE_CASINF) && defined(HAVE_CLOGF) && defined(HAVE_CSQRTF)
#define HAVE_CASINF 1
#endif
#if !defined(HAVE_CASIN) && defined(HAVE_CLOG) && defined(HAVE_CSQRT)
#define HAVE_CASIN 1
#endif
#if !defined(HAVE_CASINL) && defined(HAVE_CLOGL) && defined(HAVE_CSQRTL)
#define HAVE_CASINL 1
#endif
#if !defined(HAVE_CATANF) && defined(HAVE_CLOGF)
#define HAVE_CATANF 1
#endif
#if !defined(HAVE_CATAN) && defined(HAVE_CLOG)
#define HAVE_CATAN 1
#endif
#if !defined(HAVE_CATANL) && defined(HAVE_CLOGL)
#define HAVE_CATANL 1
#endif
#if !defined(HAVE_CASINHF) && defined(HAVE_CLOGF) && defined(HAVE_CSQRTF)
#define HAVE_CASINHF 1
#endif
#if !defined(HAVE_CASINH) && defined(HAVE_CLOG) && defined(HAVE_CSQRT)
#define HAVE_CASINH 1
#endif
#if !defined(HAVE_CASINHL) && defined(HAVE_CLOGL) && defined(HAVE_CSQRTL)
#define HAVE_CASINHL 1
#endif
#if !defined(HAVE_CACOSHF) && defined(HAVE_CLOGF) && defined(HAVE_CSQRTF)
#define HAVE_CACOSHF 1
#endif
#if !defined(HAVE_CACOSH) && defined(HAVE_CLOG) && defined(HAVE_CSQRT)
#define HAVE_CACOSH 1
#endif
#if !defined(HAVE_CACOSHL) && defined(HAVE_CLOGL) && defined(HAVE_CSQRTL)
#define HAVE_CACOSHL 1
#endif
#if !defined(HAVE_CATANHF) && defined(HAVE_CLOGF)
#define HAVE_CATANHF 1
#endif
#if !defined(HAVE_CATANH) && defined(HAVE_CLOG)
#define HAVE_CATANH 1
#endif
#if !defined(HAVE_CATANHL) && defined(HAVE_CLOGL)
#define HAVE_CATANHL 1
#endif
#if !defined(HAVE_TGAMMA)
#define HAVE_TGAMMA 1
#endif
#if !defined(HAVE_LGAMMA)
#define HAVE_LGAMMA 1
#endif
#if defined(HAVE_TGAMMA) && !defined(HAVE_TGAMMAF)
#define HAVE_TGAMMAF 1
#endif
#if defined(HAVE_LGAMMA) && !defined(HAVE_LGAMMAF)
#define HAVE_LGAMMAF 1
#endif
#endif  /* C99_PROTOS_H  */
