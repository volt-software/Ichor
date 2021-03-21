#ifndef PRIVATE_H
#define PRIVATE_H

typedef short     word;   /* 16 bit signed int  */
typedef long      longword; /* 32 bit signed int  */

typedef unsigned short    uword;    /* unsigned word  */
typedef unsigned long   ulongword;  /* unsigned longword  */

struct gsm_state {

  word    dp0[  280  ];

  word    z1;   /* preprocessing.c, Offset_com. */
  longword  L_z2;   /*                  Offset_com. */
  int   mp;   /*                  Preemphasis */

  word    u[ 8 ];   /* short_term_aly_filter.c  */
  word    LARpp[ 2 ][ 8 ];  /*                              */
  word    j;    /*                              */

  word    nrp; /* 40 */ /* long_term.c, synthesis */
  word    v[ 9 ];   /* short_term.c, synthesis  */
  word    msr;    /* decoder.c, Postprocessing  */

  char    verbose;  /* only used if !NDEBUG   */
  char    fast;   /* only used if FAST    */

};


#define MIN_WORD  ((-32767)-1)
#define MAX_WORD  ( 32767)

#define MIN_LONGWORD  ((-2147483647)-1)
#define MAX_LONGWORD  ( 2147483647)

#define SASR(x, by) ((x) >> (by))

/*   Table 4.3a  Decision level of the LTP gain quantizer
*/
/*  bc          0         1   2      3      */
word gsm_enc_DLB[ 4 ] = {  6554,    16384,  26214,     32767  };


/*   Table 4.5   Normalized inverse mantissa used to compute xM/xmax
*/
/* i      0        1    2      3      4      5     6      7   */
word gsm_enc_NRFAC[ 8 ] = { 29128, 26215, 23832, 21846, 20165, 18725, 17476, 16384 };


/*   Table 4.6   Normalized direct mantissa used to compute xM/xmax
*/
/* i                  0      1       2      3      4      5      6      7   */
word gsm_enc_FAC[ 8 ] = { 18431, 20479, 22527, 24575, 26623, 28671, 30719, 32767 };
#endif /* PRIVATE_H */
