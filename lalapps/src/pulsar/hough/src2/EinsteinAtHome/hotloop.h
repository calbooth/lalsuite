      /* ---------- ATTENTION: this the "hot-loop", which will be 
       * executed many millions of times, so anything in here 
       * has a HUGE impact on the whole performance of the code.
       * 
       * DON'T touch *anything* in here unless you really know 
       * what you're doing !!
       *------------------------------------------------------------
       */

      Xalpha_l = Xalpha + k0 - freqIndex0;  /* first frequency-bin in sum */

      /* if no danger of denominator -> 0 */
      if (__builtin_expect((kappa_star > LD_SMALL4) && (kappa_star < 1.0 - LD_SMALL4), (0==0)))

	/* -------- NOTE: to understand the variants, read them in the order:
	 * - generic  (bottom-most after #else)
	 * - AUTOVECT (still plain C)
	 * - ALTIVEC  (uses vector intrinsics)
	 * - SSE      (Assembler)
	 * (in the code roughly from bottom to top)
	*/

	/* WARNING: all current optimized loops rely on current implementation of COMPLEX8 type */
#if (EAH_HOTLOOP_VARIANT == EAH_HOTLOOP_VARIANT_SSE_AKOS08)

	/** SSE version from Akos */

#ifdef __GNUC__
        {

          COMPLEX8 XSums __attribute__ ((aligned (16))); /* sums of Xa.re and Xa.im for SSE */
	  REAL4 kappa_s = kappa_star; /* single precision version of kappa_star */

	  static REAL4 *scd = &(sincosLUTdiff[0]);
	  static REAL4 *scb = &(sincosLUTbase[0]);
	  static REAL4 M1 = -1.0f;
	  static REAL8 sincos_adds = 402653184.0;
	  REAL8 tmp;
          REAL8 _lambda_alpha = -lambda_alpha;
          /* vector constants */
          /* having these not aligned will crash the assembler code */
#define ALIGNED_VECTOR(name) static REAL4 name[4] __attribute__ ((aligned (16)))
          ALIGNED_VECTOR(D2222) = {2.0f, 2.0f, 2.0f, 2.0f};
	  ALIGNED_VECTOR(D1100) = {1.0f, 1.0f, 0.0f, 0.0f};
	  ALIGNED_VECTOR(D3322) = {3.0f, 3.0f, 2.0f, 2.0f};
	  ALIGNED_VECTOR(D5544) = {5.0f, 5.0f, 4.0f, 4.0f};
	  ALIGNED_VECTOR(D7766) = {7.0f, 7.0f, 6.0f, 6.0f};
	  ALIGNED_VECTOR(Daabb) = {-1.0f, -1.0f, -2.0f, -2.0f};
	  ALIGNED_VECTOR(Dccdd) = {-3.0f, -3.0f, -4.0f, -4.0f};
	  ALIGNED_VECTOR(Deeff) = {-5.0f, -5.0f, -6.0f, -6.0f};
	  ALIGNED_VECTOR(Dgghh) = {-7.0f, -7.0f, -8.0f, -8.0f};

	  /* hand-coded SSE version from Akos */

	  /* one loop iteration as a macro */

#ifdef EAH_HOTLOOP_INTERLEAVED
/* Macros to interleave linear sin/cos calculation (in x87 opcodes)
   with SSE hotloop.*/

/* Version 1 : with trimming of input argument 
   to [0,2) */ 
#define LIN_SIN_COS_TRIM_P0A(alpha) \
		"fldl %[" #alpha "] \n\t"   /* st: alpha */ \
		"fistpll %[tmp] \n\t"	    /* tmp=(INT8)(round((alpha)) */ \
		"fld1 \n\t" 	            /* st: 1.0 */ \
		"fildll %[tmp] \n\t"        /* st: 1.0;(round((alpha))*/ 

#define LIN_SIN_COS_TRIM_P0B(alpha)\
		"fsubrp %%st,%%st(1) \n\t"  /* st: 1.0 -round(alpha) */  \
		"faddl %[" #alpha "] \n\t"  /* st: alpha -round(alpha)+1.0*/ \
		"faddl  %[sincos_adds]  \n\t" /* ..continue lin. sin/cos as lebow */ \
		"fstpl  %[tmp]    \n\t" 
/* Version 2 : assumes input argument is already trimmed */ 
		
#define LIN_SIN_COS_P0(alpha) \
		"fldl %[" #alpha "] \n\t"     /*st:alpha */\
		"faddl  %[sincos_adds]  \n\t" /*st:alpha+A */\
		"fstpl  %[tmp]    \n\t"
#define LIN_SIN_COS_P1 \
		"mov  %[tmp],%%eax \n\t"      /* alpha +A ->eax (ix)*/ \
                "mov  %%eax,%%edx  \n\t"      /* n  = ix & SINCOS_MASK2 */\
		"and  $0x3fff,%%eax \n\t"     	
#define LIN_SIN_COS_P2 \
		"mov  %%eax,%[tmp] \n\t"     \
		"mov  %[scd], %%eax \n\t"    \
		"and  $0xffffff,%%edx \n\t"   /* i  = ix & SINCOS_MASK1;*/
#define LIN_SIN_COS_P3 \
		"fildl %[tmp]\n\t" \
		"sar $0xe,%%edx \n\t"        /*  i  = i >> SINCOS_SHIFT;*/\
		"fld %%st  \n\t"   	     /* st: n; n; */
#define LIN_SIN_COS_P4 \
		"fmuls (%%eax,%%edx,4)   \n\t" \
		"mov  %[scb], %%edi \n\t" \
		"fadds (%%edi,%%edx,4)   \n\t" /*st:sincosLUTbase[i]+n*sincosLUTdiff[i]; n*/
#define LIN_SIN_COS_P5(sin)\
		"add $0x100,%%edx \n\t"   /*edx+=SINCOS_LUT_RES/4*/\
		"fstps %[" #sin "] \n\t"  /*(*sin)=sincosLUTbase[i]+n*sincosLUTdiff[i]*/\
		"fmuls (%%eax,%%edx,4)   \n\t"
#define LIN_SIN_COS_P6(cos) \
		"fadds (%%edi,%%edx,4)   \n\t" \
		"fstps %[" #cos "] \n\t" /*(*cos)=cosbase[i]+n*cosdiff[i];*/

	
#else
#define LIN_SIN_COS_TRIM_P0A(alpha) ""
#define LIN_SIN_COS_TRIM_P0B(alpha) ""
#define LIN_SIN_COS_P0(alpha) ""
#define LIN_SIN_COS_P1 ""
#define LIN_SIN_COS_P2 "" 
#define LIN_SIN_COS_P3 ""
#define LIN_SIN_COS_P4(sin) ""
#define LIN_SIN_COS_P5(cos) ""		

#ifndef LAL_NDEBUG
	  if ( local_sin_cos_2PI_LUT_trimmed ( &s_alpha, &c_alpha, kappa_star ) ) {
	    XLAL_ERROR ( "LocalXLALComputeFaFb", XLAL_EFUNC);
	  }
#else
	  local_sin_cos_2PI_LUT_trimmed ( &s_alpha, &c_alpha, kappa_star );
#endif	  

          SINCOS_TRIM_X (_lambda_alpha,_lambda_alpha);
#endif /* EAH_HOTLOOP_INTERLEAVED */
          __asm __volatile
	    (
		"movaps %[D7766],%%xmm0\n\t"
		"movaps %[D5544],%%xmm1	\n\t"
		"movups (%[Xa]),%%xmm2	\n\t"
		"movups 0x10(%[Xa]),%%xmm3 \n\t"
		"movss  %[kappa_s],%%xmm7\n\t"
		"shufps $0x0,%%xmm7,%%xmm7\n\t"
	LIN_SIN_COS_P0(kappa_star)
		"addps  %%xmm7,%%xmm0\n\t"
		"addps  %%xmm7,%%xmm1\n\t"
		"rcpps  %%xmm0,%%xmm0\n\t"
		"rcpps  %%xmm1,%%xmm1\n\t"
		"mulps  %%xmm2,%%xmm0\n\t"
		"mulps  %%xmm3,%%xmm1\n\t"
	LIN_SIN_COS_P1
		"addps  %%xmm1,%%xmm0\n\t"
		"movaps %[D3322],%%xmm2\n\t"
		"movaps %[Dccdd],%%xmm3\n\t"
		"movups 0x20(%[Xa]),%%xmm4\n\t"
		"movups 0x50(%[Xa]),%%xmm5\n\t"
	LIN_SIN_COS_P2
		"addps  %%xmm7,%%xmm2\n\t"
		"addps  %%xmm7,%%xmm3\n\t"
		"rcpps  %%xmm2,%%xmm2\n\t"
		"rcpps  %%xmm3,%%xmm3\n\t"
		"mulps  %%xmm4,%%xmm2\n\t"
		"mulps  %%xmm5,%%xmm3\n\t"
	LIN_SIN_COS_P3
		"addps  %%xmm3,%%xmm2\n\t"
		"movaps %[Deeff],%%xmm4\n\t"
		"movaps %[Dgghh],%%xmm5\n\t"
		"movups 0x60(%[Xa]),%%xmm1\n\t"
		"movups 0x70(%[Xa]),%%xmm6\n\t"
	LIN_SIN_COS_P4
		"addps  %%xmm7,%%xmm4\n\t"
		"addps  %%xmm7,%%xmm5\n\t"
		"rcpps  %%xmm4,%%xmm4\n\t"
		"rcpps  %%xmm5,%%xmm5\n\t"
		"mulps  %%xmm1,%%xmm4\n\t"
		"mulps  %%xmm6,%%xmm5\n\t"
	LIN_SIN_COS_P5(sin)
		"addps  %%xmm2,%%xmm0\n\t"
		"addps  %%xmm5,%%xmm4\n\t"
		"movaps %[D1100],%%xmm1\n\t"
		"movaps %[Daabb],%%xmm2\n\t"
	LIN_SIN_COS_P6(cos)
		"addps  %%xmm7,%%xmm1\n\t"
		"addps  %%xmm7,%%xmm2\n\t"
		"rcpps  %%xmm1,%%xmm5\n\t"
		"rcpps  %%xmm2,%%xmm6\n\t"
	LIN_SIN_COS_TRIM_P0A(_lambda_alpha)
		"addps  %%xmm4,%%xmm0\n\t"
		"movaps %[D2222],%%xmm3\n\t"
		"movaps %[D2222],%%xmm4\n\t"
		"mulps  %%xmm5,%%xmm1\n\t"
		"mulps  %%xmm6,%%xmm2\n\t"
	LIN_SIN_COS_TRIM_P0B(_lambda_alpha)
		"subps  %%xmm1,%%xmm3\n\t"
		"subps  %%xmm2,%%xmm4\n\t"
		"mulps  %%xmm3,%%xmm5\n\t"
		"mulps  %%xmm4,%%xmm6\n\t"
		"movups 0x30(%[Xa]),%%xmm1\n\t"
		"movups 0x40(%[Xa]),%%xmm2\n\t"
	LIN_SIN_COS_P1
		"mulps  %%xmm5,%%xmm1\n\t"
		"mulps  %%xmm6,%%xmm2\n\t"
		"addps  %%xmm1,%%xmm0\n\t"
		"addps  %%xmm2,%%xmm0\n\t"
	LIN_SIN_COS_P2
		"movhlps %%xmm0,%%xmm1\n\t"
		"addps  %%xmm1,%%xmm0\n\t"

/*	  
        c_alpha-=1.0f;
	  realXP = s_alpha * XSums.re - c_alpha * XSums.im;
	  imagXP = c_alpha * XSums.re + s_alpha * XSums.im;
*/

		"movss %[M1],%%xmm5 \n\t"
		"movaps %%xmm0,%%xmm3 \n\t"
		"shufps $1,%%xmm3,%%xmm3 \n\t"	
	LIN_SIN_COS_P3
		"movss %[cos],%%xmm2 \n\t"
		"movss %[sin],%%xmm1 \n\t"
		"addss %%xmm5,%%xmm2 \n\t"	
		"movss %%xmm2,%%xmm6 \n\t"	
	LIN_SIN_COS_P4
		"movss %%xmm1,%%xmm5  \n\t"
		"mulss %%xmm0,%%xmm1 \n\t"		
		"mulss %%xmm0,%%xmm2 \n\t"

	LIN_SIN_COS_P5(Qimag)
		"mulss %%xmm3,%%xmm5 \n\t"
		"mulss %%xmm3,%%xmm6 \n\t"
		"addss %%xmm5,%%xmm2 \n\t"
		"subss %%xmm6,%%xmm1 \n\t"
	LIN_SIN_COS_P6(Qreal)
		"MOVSS	%%xmm2,%[XPimag]   	\n\t"	/*  */
		"MOVSS	%%xmm1,%[XPreal]   	\n\t"	/*  */

	     /* interface */
	     :
	     /* output  (here: to memory)*/
	     [XPreal]      "=m" (realXP),
	     [XPimag]      "=m" (imagXP),
	     [Qreal]      "=m" (realQ),
	     [Qimag]      "=m" (imagQ),
	     [sin]	  "=m" (s_alpha),
	     [cos]	  "=m" (c_alpha),
	     [tmp]        "=m" (tmp)

	     :
	     /* input */
	     [Xa]          "r"  (Xalpha_l),
	     [kappa_s]     "m"  (kappa_s),
	     [kappa_star]  "m"  (kappa_star),
	     [_lambda_alpha] "m" (_lambda_alpha),
	     [scd]	   "m"  (scd),
	     [scb]	   "m"  (scb),
	     [sincos_adds] "m"  (sincos_adds),
	     [M1]	  "m" (M1),


	     /* vector constants */
	     [D2222]       "m"  (D2222[0]),
	     [D1100]       "m"  (D1100[0]),
	     [D3322]       "m"  (D3322[0]),
	     [D5544]       "m"  (D5544[0]),
	     [D7766]       "m"  (D7766[0]),
	     [Daabb]       "m"  (Daabb[0]),
	     [Dccdd]       "m"  (Dccdd[0]),
	     [Deeff]       "m"  (Deeff[0]),
	     [Dgghh]       "m"  (Dgghh[0])

	     :
	     /* clobbered registers */
	     "xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7","st","st(1)","st(2)","eax","edx","edi","cc"
	     );

	  /* moved the sin/cos call down here to avoid the store/forward stall of Core2s */

	  /* NOTE: sin[ 2pi (Dphi_alpha - k) ] = sin [ 2pi Dphi_alpha ], therefore
	   * the trig-functions need to be calculated only once!
	   * We choose the value sin[ 2pi(Dphi_alpha - kstar) ] because it is the 
	   * closest to zero and will pose no numerical difficulties !
	   */

	}
#else /* __GNUC__ */
	{
	  __declspec(align(16)) static struct { REAL4 a,b,c,d; } v0011 = {0.0, 0.0, 1.0, 1.0};
	  __declspec(align(16)) static struct { REAL4 a,b,c,d; } v2222 = {2.0, 2.0, 2.0, 2.0};
  	  __declspec(align(16)) COMPLEX8 STn; 
	
	  REAL4 kappa_m = kappa_max; /* single precision version of kappa_max */
	      
	  /* prelude */
	  __asm {
 	      mov      esi , Xalpha_l                 /* Xal = Xalpha_l         */
	      movss    xmm2, kappa_m                  /* pn[0] = kappa_max      */
	      movlps   xmm1, MMWORD PTR [esi]         /* STnV = Xal ...         */
	      movhps   xmm1, MMWORD PTR [esi+8]       /* ... continued          */
	      shufps   xmm2, xmm2, 0                  /* pn[3]=pn[2]=pn[1]=pn[0]*/
	      movaps   xmm4, XMMWORD PTR v2222        /* xmm4 = V2222           */
	      subps    xmm2, XMMWORD PTR v0011        /* pn[2]-=1.0; pn[3]-=1.0 */
	      movaps   xmm0, xmm2                     /* qn = pn                */
	      };

	  /* one loop iteration as a macro */
#define VEC_LOOP_AV(a,b)\
	  { \
	      __asm movlps   xmm3, MMWORD PTR [esi+a] /* Xai = Xal[a]  ...*/\
	      __asm movhps   xmm3, MMWORD PTR [esi+b] /* ... continued    */\
	      __asm subps    xmm2, xmm4		      /* pn   -= V2222    */\
	      __asm mulps    xmm3, xmm0		      /* Xai  *= qn       */\
	      __asm mulps    xmm1, xmm2		      /* STnV *= pn       */\
	      __asm mulps    xmm0, xmm2		      /* qn   *= pn       */\
	      __asm addps    xmm1, xmm3		      /* STnV += Xai      */\
	      }

	  /* seven macro calls i.e. loop iterations */
	  VEC_LOOP_AV(16,24);
	  VEC_LOOP_AV(32,40);
	  VEC_LOOP_AV(48,56);
	  VEC_LOOP_AV(64,72);
	  VEC_LOOP_AV(80,88);
	  VEC_LOOP_AV(96,104);
	  VEC_LOOP_AV(112,120);

	  /* four divisions and summing in SSE, then write out the result */
	  __asm {
	      divps    xmm1, xmm0                     /* STnV      /= qn       */
	      movhlps  xmm4, xmm1                     /* / STnV[0] += STnV[2] \ */
	      addps    xmm4, xmm1                     /* \ STnV[1] += STnV[3] / */
	      movlps   STn, xmm4                      /* STn = STnV */
	      };

	  /* NOTE: sin[ 2pi (Dphi_alpha - k) ] = sin [ 2pi Dphi_alpha ], therefore
	   * the trig-functions need to be calculated only once!
	   * We choose the value sin[ 2pi(Dphi_alpha - kstar) ] because it is the 
	   * closest to zero and will pose no numerical difficulties !
	   */
#ifndef LAL_NDEBUG
	  if ( local_sin_cos_2PI_LUT_trimmed ( &s_alpha, &c_alpha, kappa_star ) ) {
	    XLAL_ERROR ( "LocalXLALComputeFaFb", XLAL_EFUNC);
	  }
#else
	  local_sin_cos_2PI_LUT_trimmed ( &s_alpha, &c_alpha, kappa_star );
#endif
	  c_alpha -= 1.0f;

	  realXP = s_alpha * STn.re - c_alpha * STn.im;
	  imagXP = c_alpha * STn.re + s_alpha * STn.im;
	     
	}
#endif /* __GNUC__ */




#elif (EAH_HOTLOOP_VARIANT == EAH_HOTLOOP_VARIANT_SSE)

	/** SSE version from Akos */

#ifdef __GNUC__
        {

          COMPLEX8 XSums __attribute__ ((aligned (16))); /* sums of Xa.re and Xa.im for SSE */
	  REAL4 kappa_m = kappa_max; /* single precision version of kappa_max */

	  static REAL4 *scd 		  =  &(sincosLUTdiff[0]);
	  static REAL4 *scb 		  =  &(sincosLUTbase[0]);
	  static REAL4 M1 = -1.0f;
	  static REAL8 sincos_adds = 402653184.0;
	  REAL8 tmp;
          REAL8 _lambda_alpha = -lambda_alpha;
          /* vector constants */
          /* having these not aligned will crash the assembler code */
          static REAL4 V0011[4] __attribute__ ((aligned (16))) = { 0,0,1,1 };
          static REAL4 V2222[4] __attribute__ ((aligned (16))) = { 2,2,2,2 };
	  
	  /* hand-coded SSE version from Akos */

	  /* one loop iteration as a macro */

#define VEC_LOOP_AV(a)\
	     "MOVUPS " #a "(%[Xa]),%%xmm3   	\n\t" \
	     "SUBPS	%%xmm4,%%xmm2   	\n\t" \
	     "MULPS	%%xmm0,%%xmm3   	\n\t" \
	     "MULPS	%%xmm2,%%xmm1   	\n\t" \
	     "MULPS	%%xmm2,%%xmm0   	\n\t" \
	     "ADDPS	%%xmm3,%%xmm1   	\n\t" 


#ifdef EAH_HOTLOOP_INTERLEAVED

#define LIN_SIN_COS_TRIM_P0A(alpha) \
		"fldl %[" #alpha "] \n\t" \
		"fistpll %[tmp] \n\t" \
		"fld1 \n\t" 	\
		"fildll %[tmp] \n\t" 

#define LIN_SIN_COS_TRIM_P0B(alpha)\
		"fsubrp %%st,%%st(1) \n\t" \
		"faddl %[" #alpha "] \n\t" \
		"faddl  %[sincos_adds]  \n\t" \
		"fstpl  %[tmp]    \n\t" 
		
#define LIN_SIN_COS_P0(alpha) \
		"fldl %[" #alpha "] \n\t" \
		"faddl  %[sincos_adds]  \n\t" \
		"fstpl  %[tmp]    \n\t" 

#define LIN_SIN_COS_P1 \
		"mov  %[tmp],%%eax \n\t"  \
                "mov  %%eax,%%edx  \n\t" \
		"and  $0x3fff,%%eax \n\t" 
#define LIN_SIN_COS_P2 \
		"mov  %%eax,%[tmp] \n\t"   \
		"mov  %[scd], %%eax \n\t"\
		"and  $0xffffff,%%edx \n\t" \
		"fildl %[tmp]\n\t" 
#define LIN_SIN_COS_P3 \
		"sar $0xe,%%edx \n\t" \
		"fld %%st  \n\t"   \
		"fmuls (%%eax,%%edx,4)   \n\t" \
		"mov  %[scb], %%edi \n\t"
#define LIN_SIN_COS_P4(sin)\
		"fadds (%%edi,%%edx,4)   \n\t" \
		"add $0x100,%%edx \n\t"   \
		"fstps %[" #sin "] \n\t" \
		"fmuls (%%eax,%%edx,4)   \n\t"
#define LIN_SIN_COS_P5(cos) \
		"fadds (%%edi,%%edx,4)   \n\t" \
		"fstps %[" #cos "] \n\t"		

	
#else
#define LIN_SIN_COS_TRIM_P0A(alpha) ""
#define LIN_SIN_COS_TRIM_P0B(alpha) ""
#define LIN_SIN_COS_P0(alpha) ""
#define LIN_SIN_COS_P1 ""
#define LIN_SIN_COS_P2 "" 
#define LIN_SIN_COS_P3 ""
#define LIN_SIN_COS_P4(sin) ""
#define LIN_SIN_COS_P5(cos) ""		

#ifndef LAL_NDEBUG
	  if ( local_sin_cos_2PI_LUT_trimmed ( &s_alpha, &c_alpha, kappa_star ) ) {
	    XLAL_ERROR ( "LocalXLALComputeFaFb", XLAL_EFUNC);
	  }
#else
	  local_sin_cos_2PI_LUT_trimmed ( &s_alpha, &c_alpha, kappa_star );
#endif	  

          SINCOS_TRIM_X (_lambda_alpha,_lambda_alpha);
#endif
          __asm __volatile
	    (
	     /* -------------------------------------------------------------------; */
	     /* Prepare common divisor method for 4 values ( two Re-Im pair ) */
	     /*  Im1, Re1, Im0, Re0 */
	     "MOVAPS	%[V0011],%%xmm6   	\n\t"	
	     "MOVSS	%[kappa_m],%%xmm2   	\n\t"	/* X2:  -   -   -   C */
	     "MOVUPS	(%[Xa]),%%xmm1   	\n\t"	/* X1: Y01 X01 Y00 X00 */
	     "SHUFPS	$0,%%xmm2,%%xmm2   	\n\t"	/* X2:  C   C   C   C */
	     "MOVAPS	%[V2222],%%xmm4   	\n\t"	/* X7:  2   2   2   2 */
	     "SUBPS	%%xmm6,%%xmm2   	\n\t"	/* X2: C-1 C-1  C   C */
	     /* -------------------------------------------------------------------; */
	     "MOVAPS	%%xmm2,%%xmm0   	\n\t"	/* X0: C-1 C-1  C   C */
	     /* -------------------------------------------------------------------; */
	     /* xmm0: collected denumerators -> a new element will multiply by this */
	     /* xmm1: collected numerators -> we will divide it by the denumerator last */
	     /* xmm2: current denumerator ( counter type ) */
	     /* xmm3: current numerator ( current Re,Im elements ) */
	     /* -------------------------------------------------------------------; */

	     /* seven "loop iterations" (unrolled) */
LIN_SIN_COS_P0(kappa_star)
	     VEC_LOOP_AV(16)
LIN_SIN_COS_P1
	     VEC_LOOP_AV(32)
LIN_SIN_COS_P2
	     VEC_LOOP_AV(48)
LIN_SIN_COS_P3
	     VEC_LOOP_AV(64)
LIN_SIN_COS_P4(sin)
	     VEC_LOOP_AV(80)
LIN_SIN_COS_P5(cos)
	     VEC_LOOP_AV(96)

LIN_SIN_COS_TRIM_P0A(_lambda_alpha)


#if EAH_HOTLOOP_DIVS == EAH_HOTLOOP_DIVS_RECIPROCAL
	     "movhlps %%xmm6,%%xmm6     \n\t"
#endif
	     "movss %[M1] , %%xmm5 \n\t"
	     VEC_LOOP_AV(112)


#if EAH_HOTLOOP_DIVS == EAH_HOTLOOP_DIVS_RECIPROCAL
	     "movhlps %%xmm0,%%xmm2   	\n\t"
	     "mulss %%xmm0,%%xmm2	\n\t"

#ifdef EAH_HOTLOOP_RENR 
	     "RCPSS %%xmm2,%%xmm6	\n\t"
	     "MULSS %%xmm6,%%xmm2	\n\t"
	     "MULSS %%xmm6,%%xmm2	\n\t"
	LIN_SIN_COS_TRIM_P0B(_lambda_alpha)
	     "ADDSS %%xmm6,%%xmm6	\n\t"
	     "SUBSS %%xmm2,%%xmm6	\n\t"
#else
	     "divss %%xmm2,%%xmm6	\n\t"
	LIN_SIN_COS_TRIM_P0B(_lambda_alpha)

#endif

	     "shufps $78,%%xmm0,%%xmm0  \n\t"
	     "mulps  %%xmm1,%%xmm0      \n\t"
	     "movhlps %%xmm0,%%xmm4	\n\t"
	     "addps %%xmm0,%%xmm4	\n\t"

	     "shufps $160,%%xmm6,%%xmm6   \n\t"
	     "mulps %%xmm6,%%xmm4	\n\t"		
#else
	     /* -------------------------------------------------------------------; */
	     /* Four divisions at once ( two for real parts and two for imaginary parts ) */
#ifdef EAH_HOTLOOP_RENR 
	     "RCPPS %%xmm0,%%xmm6	\n\t"
	     "MULPS %%xmm6,%%xmm0	\n\t"
	     "MULPS %%xmm6,%%xmm0	\n\t"
	LIN_SIN_COS_TRIM_P0B(_lambda_alpha)
	     "ADDPS %%xmm6,%%xmm6	\n\t"
	     "SUBPS %%xmm0,%%xmm6	\n\t"
	     "MULPS %%xmm6,%%xmm1	\n\t"
#else
	     "DIVPS	%%xmm0,%%xmm1   	\n\t"	/* X1: Y0G X0G Y1F X1F */
	LIN_SIN_COS_TRIM_P0B(_lambda_alpha)
#endif
	     /* -------------------------------------------------------------------; */
	     /* So we have to add the two real and two imaginary parts */
	     "MOVHLPS   %%xmm1,%%xmm4	        \n\t"	/* X4:  -   -  Y0G X0G */
	     "ADDPS	%%xmm1,%%xmm4   	\n\t"	/* X4:  -   -  YOK XOK */
	     /* -------------------------------------------------------------------; */
#endif

/*	  
        c_alpha-=1.0f;
	  realXP = s_alpha * XSums.re - c_alpha * XSums.im;
	  imagXP = c_alpha * XSums.re + s_alpha * XSums.im;
*/

		"movaps %%xmm4,%%xmm3 \n\t"
		"shufps $1,%%xmm3,%%xmm3 \n\t"
		"movss %[cos],%%xmm2 \n\t"
	LIN_SIN_COS_P1	
		"movss %[sin],%%xmm1 \n\t"
		"addss %%xmm5,%%xmm2\n\t"	
		"movss %%xmm2,%%xmm6 \n\t"
	LIN_SIN_COS_P2	
		"movss %%xmm1,%%xmm5  \n\t"
		"mulss %%xmm4,%%xmm1 \n\t"		
		"mulss %%xmm4,%%xmm2 \n\t"
	LIN_SIN_COS_P3
		"mulss %%xmm3,%%xmm5 \n\t"
		"mulss %%xmm3,%%xmm6 \n\t"
	LIN_SIN_COS_P4(Qimag)		
		"addss %%xmm5,%%xmm2 \n\t"
		"subss %%xmm6,%%xmm1 \n\t"
	LIN_SIN_COS_P5(Qreal)
		"MOVss	%%xmm2,%[XPimag]   	\n\t"	/*  */
		"MOVss	%%xmm1,%[XPreal]   	\n\t"	/*  */

	     /* interface */
	     :
	     /* output  (here: to memory)*/
	     [XPreal]      "=m" (realXP),
	     [XPimag]      "=m" (imagXP),
	     [Qreal]      "=m" (realQ),
	     [Qimag]      "=m" (imagQ),
	     [tmp]        "=m" (tmp),
	     [sin]	  "=m" (s_alpha),
	     [cos]	  "=m" (c_alpha)

	     :
	     /* input */
	     [Xa]          "r"  (Xalpha_l),
	     [kappa_m]     "m"  (kappa_m),
	     [kappa_star]  "m"  (kappa_star),
	     [_lambda_alpha] "m" (_lambda_alpha),
	     [scd]	   "m"  (scd),
	     [scb]	   "m"  (scb),
	     [sincos_adds]       "m"  (sincos_adds),
	     [M1]	  "m" (M1),


	     /* vector constants */
	     [V0011]       "m"  (V0011[0]),
	     [V2222]       "m"  (V2222[0])

#ifndef IGNORE_XMM_REGISTERS
	     :
	     /* clobbered registers */
	     "xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7","st","st(1)","st(2)","eax","edx","edi","cc"
#endif
	     );

	  /* moved the sin/cos call down here to avoid the store/forward stall of Core2s */

	  /* NOTE: sin[ 2pi (Dphi_alpha - k) ] = sin [ 2pi Dphi_alpha ], therefore
	   * the trig-functions need to be calculated only once!
	   * We choose the value sin[ 2pi(Dphi_alpha - kstar) ] because it is the 
	   * closest to zero and will pose no numerical difficulties !
	   */

	}
#else /* __GNUC__ */
	{
	  __declspec(align(16)) static struct { REAL4 a,b,c,d; } v0011 = {0.0, 0.0, 1.0, 1.0};
	  __declspec(align(16)) static struct { REAL4 a,b,c,d; } v2222 = {2.0, 2.0, 2.0, 2.0};
  	  __declspec(align(16)) COMPLEX8 STn; 
	
	  REAL4 kappa_m = kappa_max; /* single precision version of kappa_max */
	      
	  /* prelude */
	  __asm {
 	      mov      esi , Xalpha_l                 /* Xal = Xalpha_l         */
	      movss    xmm2, kappa_m                  /* pn[0] = kappa_max      */
	      movlps   xmm1, MMWORD PTR [esi]         /* STnV = Xal ...         */
	      movhps   xmm1, MMWORD PTR [esi+8]       /* ... continued          */
	      shufps   xmm2, xmm2, 0                  /* pn[3]=pn[2]=pn[1]=pn[0]*/
	      movaps   xmm4, XMMWORD PTR v2222        /* xmm4 = V2222           */
	      subps    xmm2, XMMWORD PTR v0011        /* pn[2]-=1.0; pn[3]-=1.0 */
	      movaps   xmm0, xmm2                     /* qn = pn                */
	      };

	  /* one loop iteration as a macro */
#define VEC_LOOP_AV(a,b)\
	  { \
	      __asm movlps   xmm3, MMWORD PTR [esi+a] /* Xai = Xal[a]  ...*/\
	      __asm movhps   xmm3, MMWORD PTR [esi+b] /* ... continued    */\
	      __asm subps    xmm2, xmm4		      /* pn   -= V2222    */\
	      __asm mulps    xmm3, xmm0		      /* Xai  *= qn       */\
	      __asm mulps    xmm1, xmm2		      /* STnV *= pn       */\
	      __asm mulps    xmm0, xmm2		      /* qn   *= pn       */\
	      __asm addps    xmm1, xmm3		      /* STnV += Xai      */\
	      }

	  /* seven macro calls i.e. loop iterations */
	  VEC_LOOP_AV(16,24);
	  VEC_LOOP_AV(32,40);
	  VEC_LOOP_AV(48,56);
	  VEC_LOOP_AV(64,72);
	  VEC_LOOP_AV(80,88);
	  VEC_LOOP_AV(96,104);
	  VEC_LOOP_AV(112,120);

	  /* four divisions and summing in SSE, then write out the result */
	  __asm {
	      divps    xmm1, xmm0                     /* STnV      /= qn       */
	      movhlps  xmm4, xmm1                     /* / STnV[0] += STnV[2] \ */
	      addps    xmm4, xmm1                     /* \ STnV[1] += STnV[3] / */
	      movlps   STn, xmm4                      /* STn = STnV */
	      };

	  /* NOTE: sin[ 2pi (Dphi_alpha - k) ] = sin [ 2pi Dphi_alpha ], therefore
	   * the trig-functions need to be calculated only once!
	   * We choose the value sin[ 2pi(Dphi_alpha - kstar) ] because it is the 
	   * closest to zero and will pose no numerical difficulties !
	   */
#ifndef LAL_NDEBUG
	  if ( local_sin_cos_2PI_LUT_trimmed ( &s_alpha, &c_alpha, kappa_star ) ) {
	    XLAL_ERROR ( "LocalXLALComputeFaFb", XLAL_EFUNC);
	  }
#else
	  local_sin_cos_2PI_LUT_trimmed ( &s_alpha, &c_alpha, kappa_star );
#endif
	  c_alpha -= 1.0f;

	  realXP = s_alpha * STn.re - c_alpha * STn.im;
	  imagXP = c_alpha * STn.re + s_alpha * STn.im;
	     
	}
#endif /* __GNUC__ */

#elif (EAH_HOTLOOP_VARIANT == EAH_HOTLOOP_VARIANT_ALTIVEC)

	{
#ifndef LAL_NDEBUG
	  if ( local_sin_cos_2PI_LUT_trimmed ( &s_alpha, &c_alpha, kappa_star ) ) {
	  XLAL_ERROR ( "LocalXLALComputeFaFb", XLAL_EFUNC);
	  }
#else
	  local_sin_cos_2PI_LUT_trimmed ( &s_alpha, &c_alpha, kappa_star );
#endif
	  c_alpha -= 1.0f;
	  {
	    REAL4 *Xalpha_kR4 = (REAL4*)(Xalpha_l);
	    
	    float STn[4] __attribute__ ((aligned (16))); /* aligned for vector output */
	    /* the vectors actually become registers in the AVUnit */
	    vector unsigned char perm;      /* permutation pattern for unaligned memory access */
	    vector float load0, load1, load2;    /* temp registers for unaligned memory access */
	    vector float XaiV   /* xmm3 */;                     /* SFT data loaded from memory */
	    vector float STnV   /* xmm1 */;                            /* sums up the dividend */
	    vector float V0000             = {0,0,0,0};                /* zero vector constant */
	    vector float V2222  /* xmm4 */ = {2,2,2,2};                     /* vector constant */
	    vector float pnV    /* xmm2 */ = {((float)(kappa_max)),
					      ((float)(kappa_max)),
					      ((float)(kappa_max - 1)),
					      ((float)(kappa_max - 1)) };
	    vector float qnV    /* xmm0 */ = pnV;  /* common divisor, initally = 1.0 * pnV */
	    /*    this column above (^) lists the corresponding register in the SSE version */
	    
	    vector float tV;  /* temporary vector used for Newton-Rhapson iterarion */

	    /* init the memory access (load0,load1) */
	    load0   = vec_ld  (0,(Xalpha_kR4));
	    perm    = vec_lvsl(0,(Xalpha_kR4));
	    load1   = vec_ld  (0,(Xalpha_kR4+4));

	    /* first "iteration" & initialization */
	    XaiV    = vec_perm(load0,load1,perm);
	    qnV     = vec_re(pnV);
	    STnV    = vec_madd(XaiV, qnV, V0000);

	    /* use a reciprocal estimate as a replacement for a division.
	       in our case this is only valid for the "outer" elements of the kernel loop */
#define VEC_LOOP_RE(n,a,b)\
	    pnV     = vec_sub(pnV,V2222);\
	    perm    = vec_lvsl(0,(Xalpha_kR4+(n)));\
            load##b = vec_ld(0,(Xalpha_kR4+(n)+4));\
	    XaiV    = vec_perm(load##a,load##b,perm);\
	    qnV     = vec_re(pnV);\
	    STnV    = vec_madd(XaiV, qnV, STnV);  /* STnV = XaiV * qnV + STnV */

	    /* refine the reciprocal estimate to by a Newton-Rhapson iteration.
	       re1(x) = re0(x) * (2 - x * re0(x))
	       (see http://en.wikipedia.org/wiki/Division_(digital)#Newton-Raphson_division)
	       this should give as much precision as a normal float division */
#define VEC_LOOP_RE_NR(n,a,b)\
	    pnV     = vec_sub(pnV,V2222);\
	    perm    = vec_lvsl(0,(Xalpha_kR4+(n)));\
            load##b = vec_ld(0,(Xalpha_kR4+(n)+4));\
	    XaiV    = vec_perm(load##a,load##b,perm);\
	    qnV     = vec_re(pnV);\
            tV      = vec_madd(qnV,pnV,V0000);\
            tV      = vec_sub(V2222,tV);\
            qnV     = vec_madd(qnV,tV,V0000);\
	    STnV    = vec_madd(XaiV, qnV, STnV);

	    VEC_LOOP_RE(4,1,2);
	    VEC_LOOP_RE_NR(8,2,0);
	    VEC_LOOP_RE_NR(12,0,1);
	    VEC_LOOP_RE_NR(16,1,2);
	    VEC_LOOP_RE_NR(20,2,0);
	    VEC_LOOP_RE(24,0,1);
	    VEC_LOOP_RE(28,1,0);

	    /* output the vector */
	    vec_st(STnV,0,STn);

	    /* combine the sums */
	    {
	      REAL4 U_alpha = STn[0] + STn[2];
	      REAL4 V_alpha = STn[1] + STn[3];

	      realXP = s_alpha * U_alpha - c_alpha * V_alpha;
	      imagXP = c_alpha * U_alpha + s_alpha * V_alpha;
	    }
	  }
	}

#elif (EAH_HOTLOOP_VARIANT == EAH_HOTLOOP_VARIANT_AUTOVECT)

	/* designed for four vector elemens (ve) as there are in SSE and AltiVec */
	/* vectorizes with gcc-4.2.3 and gcc-4.1.3 */

	{
	  /* the initialization already handles the first elements,
	     thus there are only 7 loop iterations left */
	  UINT4 l;
	  UINT4 ve;
	  REAL4 *Xal   = (REAL4*)Xalpha_l;
	  REAL4 STn[4] = {Xal[0],Xal[1],Xal[2],Xal[3]};
	  REAL4 pn[4]  = {kappa_max, kappa_max, kappa_max-1.0f, kappa_max-1.0f};
	  REAL4 qn[4];
	  
	  for ( ve = 0; ve < 4; ve++)
	    qn[ve] = pn[ve];
	  
	  for ( l = 1; l < DTERMS; l ++ ) {
	    Xal += 4;
	    for ( ve = 0; ve < 4; ve++) {
	      pn[ve] -= 2.0f;
	      STn[ve] = pn[ve] * STn[ve] + qn[ve] * Xal[ve];
	      qn[ve] *= pn[ve];
	    }
	  }
	  
	  {
#ifndef LAL_NDEBUG
	    if ( local_sin_cos_2PI_LUT_trimmed ( &s_alpha, &c_alpha, kappa_star ) ) {
	      XLAL_ERROR ( "LocalXLALComputeFaFb", XLAL_EFUNC);
	    }
#else
	    local_sin_cos_2PI_LUT_trimmed ( &s_alpha, &c_alpha, kappa_star );
#endif
	    c_alpha -= 1.0f;
	  }

	  /* combine the partial sums: */
	  {
#if EAH_HOTLOOP_DIVS == EAH_HOTLOOP_DIVS_RECIPROCAL
	    /* if the division is to be done outside the SIMD unit */
	    
	    REAL4 r_qn  = 1.0 / (qn[0] * qn[2]);
	    REAL4 U_alpha = (STn[0] * qn[2] + STn[2] * qn[0]) * r_qn;
	    REAL4 V_alpha = (STn[1] * qn[3] + STn[3] * qn[1]) * r_qn;
	    
	    realXP = s_alpha * U_alpha - c_alpha * V_alpha;
	    imagXP = c_alpha * U_alpha + s_alpha * V_alpha;
	    
#else /* EAH_HOTLOOP_DIVS */
	    /* if the division can and should be done inside the SIMD unit */
	    
	    REAL4 U_alpha, V_alpha;
	    
	    for ( ve = 0; ve < 4; ve++)
	      STn[ve] /= qn[ve];
	    
	    U_alpha = (STn[0] + STn[2]);
	    V_alpha = (STn[1] + STn[3]);
	    
	    realXP = s_alpha * U_alpha - c_alpha * V_alpha;
	    imagXP = c_alpha * U_alpha + s_alpha * V_alpha;

#endif /* EAH_HOTLOOP_DIVS */	    
	  }
	}

#else /* EAH_HOTLOOP_VARIANT */

	/* NOTE: sin[ 2pi (Dphi_alpha - k) ] = sin [ 2pi Dphi_alpha ], therefore
	 * the trig-functions need to be calculated only once!
	 * We choose the value sin[ 2pi(Dphi_alpha - kstar) ] because it is the 
	 * closest to zero and will pose no numerical difficulties !
	 */
	{ 
	  /* improved hotloop algorithm by Fekete Akos: 
	   * take out repeated divisions into a single common denominator,
	   * plus use extra cleverness to compute the nominator efficiently...
	   */
	  REAL4 Sn = (*Xalpha_l).re;
	  REAL4 Tn = (*Xalpha_l).im;
	  REAL4 pn = kappa_max;
	  REAL4 qn = pn;
	  REAL4 U_alpha, V_alpha;

	  /* 2*DTERMS iterations */
	  UINT4 l;
	  for ( l = 1; l < 2*DTERMS; l ++ )
	    {
	      Xalpha_l ++;
	      
	      pn = pn - 1.0f;   		  /* p_(n+1) */
	      Sn = pn * Sn + qn * (*Xalpha_l).re; /* S_(n+1) */
	      Tn = pn * Tn + qn * (*Xalpha_l).im; /* T_(n+1) */
	      qn *= pn; 			  /* q_(n+1) */
	    } /* for l < 2*DTERMS */

#if EAH_HOTLOOP_DIVS == EAH_HOTLOOP_DIVS_RECIPROCAL
	  { /* could hardly be slower than two divisions */
	    REAL4 r_qn = 1.0 / qn;
	    U_alpha = Sn * r_qn;
	    V_alpha = Tn * r_qn;
	  }
#else /* EAH_HOTLOOP_DIVS */
	  U_alpha = Sn / qn;
	  V_alpha = Tn / qn;
#endif /* EAH_HOTLOOP_DIVS */

#ifndef LAL_NDEBUG
	  if ( local_sin_cos_2PI_LUT_trimmed ( &s_alpha, &c_alpha, kappa_star ) ) {
	    XLAL_ERROR ( "LocalXLALComputeFaFb", XLAL_EFUNC);
	  }
#else
	  local_sin_cos_2PI_LUT_trimmed ( &s_alpha, &c_alpha, kappa_star );
#endif
	  c_alpha -= 1.0f;
	
 	  realXP = s_alpha * U_alpha - c_alpha * V_alpha;
 	  imagXP = c_alpha * U_alpha + s_alpha * V_alpha;
	}

#endif /* EAH_HOTLOOP_VARIANT */



      /* if |remainder| > LD_SMALL4 */
      else
	{ /* otherwise: lim_{rem->0}P_alpha,k  = 2pi delta_{k,kstar} */
 	  UINT4 ind0;
   	  if ( kappa_star <= LD_SMALL4 )
	    ind0 = DTERMS - 1;
   	  else
	    ind0 = DTERMS;
 	  realXP = TWOPI_FLOAT * Xalpha_l[ind0].re;
 	  imagXP = TWOPI_FLOAT * Xalpha_l[ind0].im;
#ifdef EAH_HOTLOOP_INTERLEAVED
	REAL8 _lambda_alpha = -lambda_alpha;
	SINCOS_TRIM_X (_lambda_alpha,_lambda_alpha);
#ifndef LAL_NDEBUG
	if ( local_sin_cos_2PI_LUT_trimmed ( &imagQ, &realQ, _lambda_alpha ) ) {
	  XLAL_ERROR ( "LocalXLALComputeFaFb", XLAL_EFUNC);
	}
#else
	local_sin_cos_2PI_LUT_trimmed ( &imagQ, &realQ, _lambda_alpha );
#endif
#endif
	} /* if |remainder| <= LD_SMALL4 */

      {
#ifndef EAH_HOTLOOP_INTERLEAVED
	REAL8 _lambda_alpha = -lambda_alpha;
	SINCOS_TRIM_X (_lambda_alpha,_lambda_alpha);
#ifndef LAL_NDEBUG
	if ( local_sin_cos_2PI_LUT_trimmed ( &imagQ, &realQ, _lambda_alpha ) ) {
	  XLAL_ERROR ( "LocalXLALComputeFaFb", XLAL_EFUNC);
	}
#else
	local_sin_cos_2PI_LUT_trimmed ( &imagQ, &realQ, _lambda_alpha );
#endif
#endif
      }
