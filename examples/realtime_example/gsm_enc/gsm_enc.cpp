/* gsm_enc_encode.c */
/*
   Copyright 1992 by Jutta Degener and Carsten Bormann, Technische
   Universitaet Berlin.  See the accompanying file "COPYRIGHT" for
   details.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.
*/

#include "private.h"

/*
    Prototypes from add.c
*/

extern word gsm_enc_div   ( word num, word denum );

extern word gsm_enc_sub   ( word a, word b );

extern word gsm_enc_norm  ( longword a );

extern word gsm_enc_asl   ( word a, int n );

extern word gsm_enc_asr   ( word a, int n );

/*
    Inlined functions from add.h
*/

#define GSM_MULT_R(a, b) /* word a, word b, !(a == b == MIN_WORD) */  \
  (SASR( ((longword)(a) * (longword)(b) + 16384), 15 ))

# define GSM_MULT(a,b)   /* word a, word b, !(a == b == MIN_WORD) */  \
  (SASR( ((longword)(a) * (longword)(b)), 15 ))

# define GSM_L_ADD(a, b)  \
  ( (a) <  0 ? ( (b) >= 0 ? (a) + (b) \
     : (utmp = (ulongword)-((a) + 1) + (ulongword)-((b) + 1)) \
       >= MAX_LONGWORD ? MIN_LONGWORD : -(longword)utmp-2 )   \
  : ((b) <= 0 ? (a) + (b)   \
            : (utmp = (ulongword)(a) + (ulongword)(b)) >= (ulongword)MAX_LONGWORD \
        ? MAX_LONGWORD : a + b))

#define GSM_ADD(a, b) \
  ((ulongword)((ltmp = (longword)(a) + (longword)(b)) - MIN_WORD) > \
    MAX_WORD - MIN_WORD ? (ltmp > 0 ? MAX_WORD : MIN_WORD) : ltmp)

# define GSM_SUB(a, b)  \
  ((ltmp = (longword)(a) - (longword)(b)) >= MAX_WORD \
  ? MAX_WORD : ltmp <= MIN_WORD ? MIN_WORD : ltmp)

# define GSM_ABS(a) ((a) < 0 ? ((a) == MIN_WORD ? MAX_WORD : -(a)) : (a))

#define saturate(x)   \
  ((x) < MIN_WORD ? MIN_WORD : (x) > MAX_WORD ? MAX_WORD: (x))

/* Use these if necessary:

  # define GSM_MULT_R(a, b) gsm_enc_mult_r(a, b)
  # define GSM_MULT(a, b)   gsm_enc_mult(a, b)
  # define GSM_L_MULT(a, b) gsm_enc_L_mult(a, b)

  # define GSM_L_ADD(a, b)  gsm_enc_L_add(a, b)
  # define GSM_ADD(a, b)    gsm_enc_add(a, b)
  # define GSM_SUB(a, b)    gsm_enc_sub(a, b)

  # define GSM_ABS(a)   gsm_enc_abs(a)
*/

/*
    More prototypes from implementations..
*/
extern void gsm_enc_Gsm_Coder (
  struct gsm_state   *S,
  word   *s,  /* [ 0..159 ] samples   IN  */
  word   *LARc, /* [ 0..7 ] LAR coefficients  OUT */
  word   *Nc, /* [ 0..3 ] LTP lag   OUT   */
  word   *bc, /* [ 0..3 ] coded LTP gain  OUT   */
  word   *Mc, /* [ 0..3 ] RPE grid selection  OUT     */
  word   *xmaxc,/* [ 0..3 ] Coded maximum amplitude OUT */
  word   *xMc /* [ 13*4 ] normalized RPE samples OUT  */ );

extern void gsm_enc_Gsm_Long_Term_Predictor (   /* 4x for 160 samples */
  word   *d,  /* [ 0..39 ]   residual signal  IN  */
  word   *dp, /* [ -120..-1 ] d'    IN  */
  word   *e,  /* [ 0..40 ]      OUT */
  word   *dpp,  /* [ 0..40 ]      OUT */
  word   *Nc, /* correlation lag    OUT */
  word   *bc  /* gain factor      OUT */ );

extern void gsm_enc_Gsm_LPC_Analysis (
  word *s,   /* 0..159 signals  IN/OUT  */
  word *LARc );   /* 0..7   LARc's  OUT */

extern void gsm_enc_Gsm_Preprocess (
  struct gsm_state *S,
  word *s, word *so );

extern void gsm_enc_Gsm_Short_Term_Analysis_Filter (
  struct gsm_state *S,
  word   *LARc, /* coded log area ratio [ 0..7 ]  IN  */
  word   *d /* st res. signal [ 0..159 ]  IN/OUT  */ );

void gsm_enc_Gsm_RPE_Encoding (
  word     *e,            /* -5..-1 ][ 0..39 ][ 40..44     IN/OUT  */
  word     *xmaxc,        /*                              OUT */
  word     *Mc,           /*                              OUT */
  word     *xMc );       /* [ 0..12 ]                      OUT */


/**************** end #include "private.h" **********************************/

/*
    Interface
*/

typedef struct gsm_state  *gsm;
typedef short       gsm_signal;   /* signed 16 bit */
typedef unsigned char   gsm_byte;
typedef gsm_byte    gsm_frame[ 33 ];    /* 33 * 8 bits   */

#define GSM_MAGIC 0xD         /* 13 kbit/s RPE-LTP */

#define GSM_PATCHLEVEL  6
#define GSM_MINOR 0
#define GSM_MAJOR 1

#include "data.h"

extern void gsm_enc_encode  ( gsm, gsm_signal *, gsm_byte * );

extern int  gsm_enc_explode ( gsm, gsm_byte *, gsm_signal * );
extern void gsm_enc_implode ( gsm, gsm_signal *, gsm_byte * );


/******************* end #include "gsm.h" **********************************/

#define SAMPLES 20

/*
  Forward declaration of global variables
*/

struct gsm_state gsm_enc_state;
gsm gsm_enc_state_ptr;
volatile int gsm_enc_result;


/* add.c */

word gsm_enc_sub ( word a, word b )
{
  longword diff = ( longword )a - ( longword )b;
  return saturate( diff );
}


unsigned char gsm_enc_bitoff[ 256 ] = {
  8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

void gsm_enc_encode ( gsm s, gsm_signal *source, gsm_byte *c )
{
  word    LARc[ 8 ], Nc[ 4 ], Mc[ 4 ], bc[ 4 ], xmaxc[ 4 ], xmc[ 13 * 4 ];

  gsm_enc_Gsm_Coder( s, source, LARc, Nc, bc, Mc, xmaxc, xmc );


  /*  variable  size

    GSM_MAGIC 4

    LARc[ 0 ]   6
    LARc[ 1 ]   6
    LARc[ 2 ]   5
    LARc[ 3 ]   5
    LARc[ 4 ]   4
    LARc[ 5 ]   4
    LARc[ 6 ]   3
    LARc[ 7 ]   3

    Nc[ 0 ]   7
    bc[ 0 ]   2
    Mc[ 0 ]   2
    xmaxc[ 0 ]  6
    xmc[ 0 ]    3
    xmc[ 1 ]    3
    xmc[ 2 ]    3
    xmc[ 3 ]    3
    xmc[ 4 ]    3
    xmc[ 5 ]    3
    xmc[ 6 ]    3
    xmc[ 7 ]    3
    xmc[ 8 ]    3
    xmc[ 9 ]    3
    xmc[ 10 ]   3
    xmc[ 11 ]   3
    xmc[ 12 ]   3

    Nc[ 1 ]   7
    bc[ 1 ]   2
    Mc[ 1 ]   2
    xmaxc[ 1 ]  6
    xmc[ 13 ]   3
    xmc[ 14 ]   3
    xmc[ 15 ]   3
    xmc[ 16 ]   3
    xmc[ 17 ]   3
    xmc[ 18 ]   3
    xmc[ 19 ]   3
    xmc[ 20 ]   3
    xmc[ 21 ]   3
    xmc[ 22 ]   3
    xmc[ 23 ]   3
    xmc[ 24 ]   3
    xmc[ 25 ]   3

    Nc[ 2 ]   7
    bc[ 2 ]   2
    Mc[ 2 ]   2
    xmaxc[ 2 ]  6
    xmc[ 26 ]   3
    xmc[ 27 ]   3
    xmc[ 28 ]   3
    xmc[ 29 ]   3
    xmc[ 30 ]   3
    xmc[ 31 ]   3
    xmc[ 32 ]   3
    xmc[ 33 ]   3
    xmc[ 34 ]   3
    xmc[ 35 ]   3
    xmc[ 36 ]   3
    xmc[ 37 ]   3
    xmc[ 38 ]   3

    Nc[ 3 ]   7
    bc[ 3 ]   2
    Mc[ 3 ]   2
    xmaxc[ 3 ]  6
    xmc[ 39 ]   3
    xmc[ 40 ]   3
    xmc[ 41 ]   3
    xmc[ 42 ]   3
    xmc[ 43 ]   3
    xmc[ 44 ]   3
    xmc[ 45 ]   3
    xmc[ 46 ]   3
    xmc[ 47 ]   3
    xmc[ 48 ]   3
    xmc[ 49 ]   3
    xmc[ 50 ]   3
    xmc[ 51 ]   3
  */


  *c++ =   ( ( GSM_MAGIC & 0xF ) << 4 ) /* 1 */
           | ( ( LARc[ 0 ] >> 2 ) & 0xF );
  *c++ =   ( ( LARc[ 0 ] & 0x3 ) << 6 )
           | ( LARc[ 1 ] & 0x3F );
  *c++ =   ( ( LARc[ 2 ] & 0x1F ) << 3 )
           | ( ( LARc[ 3 ] >> 2 ) & 0x7 );
  *c++ =   ( ( LARc[ 3 ] & 0x3 ) << 6 )
           | ( ( LARc[ 4 ] & 0xF ) << 2 )
           | ( ( LARc[ 5 ] >> 2 ) & 0x3 );
  *c++ =   ( ( LARc[ 5 ] & 0x3 ) << 6 )
           | ( ( LARc[ 6 ] & 0x7 ) << 3 )
           | ( LARc[ 7 ] & 0x7 );
  *c++ =   ( ( Nc[ 0 ] & 0x7F ) << 1 )
           | ( ( bc[ 0 ] >> 1 ) & 0x1 );
  *c++ =   ( ( bc[ 0 ] & 0x1 ) << 7 )
           | ( ( Mc[ 0 ] & 0x3 ) << 5 )
           | ( ( xmaxc[ 0 ] >> 1 ) & 0x1F );
  *c++ =   ( ( xmaxc[ 0 ] & 0x1 ) << 7 )
           | ( ( xmc[ 0 ] & 0x7 ) << 4 )
           | ( ( xmc[ 1 ] & 0x7 ) << 1 )
           | ( ( xmc[ 2 ] >> 2 ) & 0x1 );
  *c++ =   ( ( xmc[ 2 ] & 0x3 ) << 6 )
           | ( ( xmc[ 3 ] & 0x7 ) << 3 )
           | ( xmc[ 4 ] & 0x7 );
  *c++ =   ( ( xmc[ 5 ] & 0x7 ) << 5 )  /* 10 */
           | ( ( xmc[ 6 ] & 0x7 ) << 2 )
           | ( ( xmc[ 7 ] >> 1 ) & 0x3 );
  *c++ =   ( ( xmc[ 7 ] & 0x1 ) << 7 )
           | ( ( xmc[ 8 ] & 0x7 ) << 4 )
           | ( ( xmc[ 9 ] & 0x7 ) << 1 )
           | ( ( xmc[ 10 ] >> 2 ) & 0x1 );
  *c++ =   ( ( xmc[ 10 ] & 0x3 ) << 6 )
           | ( ( xmc[ 11 ] & 0x7 ) << 3 )
           | ( xmc[ 12 ] & 0x7 );
  *c++ =   ( ( Nc[ 1 ] & 0x7F ) << 1 )
           | ( ( bc[ 1 ] >> 1 ) & 0x1 );
  *c++ =   ( ( bc[ 1 ] & 0x1 ) << 7 )
           | ( ( Mc[ 1 ] & 0x3 ) << 5 )
           | ( ( xmaxc[ 1 ] >> 1 ) & 0x1F );
  *c++ =   ( ( xmaxc[ 1 ] & 0x1 ) << 7 )
           | ( ( xmc[ 13 ] & 0x7 ) << 4 )
           | ( ( xmc[ 14 ] & 0x7 ) << 1 )
           | ( ( xmc[ 15 ] >> 2 ) & 0x1 );
  *c++ =   ( ( xmc[ 15 ] & 0x3 ) << 6 )
           | ( ( xmc[ 16 ] & 0x7 ) << 3 )
           | ( xmc[ 17 ] & 0x7 );
  *c++ =   ( ( xmc[ 18 ] & 0x7 ) << 5 )
           | ( ( xmc[ 19 ] & 0x7 ) << 2 )
           | ( ( xmc[ 20 ] >> 1 ) & 0x3 );
  *c++ =   ( ( xmc[ 20 ] & 0x1 ) << 7 )
           | ( ( xmc[ 21 ] & 0x7 ) << 4 )
           | ( ( xmc[ 22 ] & 0x7 ) << 1 )
           | ( ( xmc[ 23 ] >> 2 ) & 0x1 );
  *c++ =   ( ( xmc[ 23 ] & 0x3 ) << 6 )
           | ( ( xmc[ 24 ] & 0x7 ) << 3 )
           | ( xmc[ 25 ] & 0x7 );
  *c++ =   ( ( Nc[ 2 ] & 0x7F ) << 1 )  /* 20 */
           | ( ( bc[ 2 ] >> 1 ) & 0x1 );
  *c++ =   ( ( bc[ 2 ] & 0x1 ) << 7 )
           | ( ( Mc[ 2 ] & 0x3 ) << 5 )
           | ( ( xmaxc[ 2 ] >> 1 ) & 0x1F );
  *c++ =   ( ( xmaxc[ 2 ] & 0x1 ) << 7 )
           | ( ( xmc[ 26 ] & 0x7 ) << 4 )
           | ( ( xmc[ 27 ] & 0x7 ) << 1 )
           | ( ( xmc[ 28 ] >> 2 ) & 0x1 );
  *c++ =   ( ( xmc[ 28 ] & 0x3 ) << 6 )
           | ( ( xmc[ 29 ] & 0x7 ) << 3 )
           | ( xmc[ 30 ] & 0x7 );
  *c++ =   ( ( xmc[ 31 ] & 0x7 ) << 5 )
           | ( ( xmc[ 32 ] & 0x7 ) << 2 )
           | ( ( xmc[ 33 ] >> 1 ) & 0x3 );
  *c++ =   ( ( xmc[ 33 ] & 0x1 ) << 7 )
           | ( ( xmc[ 34 ] & 0x7 ) << 4 )
           | ( ( xmc[ 35 ] & 0x7 ) << 1 )
           | ( ( xmc[ 36 ] >> 2 ) & 0x1 );
  *c++ =   ( ( xmc[ 36 ] & 0x3 ) << 6 )
           | ( ( xmc[ 37 ] & 0x7 ) << 3 )
           | ( xmc[ 38 ] & 0x7 );
  *c++ =   ( ( Nc[ 3 ] & 0x7F ) << 1 )
           | ( ( bc[ 3 ] >> 1 ) & 0x1 );
  *c++ =   ( ( bc[ 3 ] & 0x1 ) << 7 )
           | ( ( Mc[ 3 ] & 0x3 ) << 5 )
           | ( ( xmaxc[ 3 ] >> 1 ) & 0x1F );
  *c++ =   ( ( xmaxc[ 3 ] & 0x1 ) << 7 )
           | ( ( xmc[ 39 ] & 0x7 ) << 4 )
           | ( ( xmc[ 40 ] & 0x7 ) << 1 )
           | ( ( xmc[ 41 ] >> 2 ) & 0x1 );
  *c++ =   ( ( xmc[ 41 ] & 0x3 ) << 6 ) /* 30 */
           | ( ( xmc[ 42 ] & 0x7 ) << 3 )
           | ( xmc[ 43 ] & 0x7 );
  *c++ =   ( ( xmc[ 44 ] & 0x7 ) << 5 )
           | ( ( xmc[ 45 ] & 0x7 ) << 2 )
           | ( ( xmc[ 46 ] >> 1 ) & 0x3 );
  *c++ =   ( ( xmc[ 46 ] & 0x1 ) << 7 )
           | ( ( xmc[ 47 ] & 0x7 ) << 4 )
           | ( ( xmc[ 48 ] & 0x7 ) << 1 )
           | ( ( xmc[ 49 ] >> 2 ) & 0x1 );
  *c++ =   ( ( xmc[ 49 ] & 0x3 ) << 6 )
           | ( ( xmc[ 50 ] & 0x7 ) << 3 )
           | ( xmc[ 51 ] & 0x7 );

}

/* decode.c */
/*
    4.3 FIXED POINT IMPLEMENTATION OF THE RPE-LTP DECODER
*/

/* code.c */
void gsm_enc_Gsm_Coder (

  struct gsm_state   *S,

  word   *s,  /* [ 0..159 ] samples       IN  */

  /*
     The RPE-LTD coder works on a frame by frame basis.  The length of
     the frame is equal to 160 samples.  Some computations are done
     once per frame to produce at the output of the coder the
     LARc[ 1..8 ] parameters which are the coded LAR coefficients and
     also to realize the inverse filtering operation for the entire
     frame (160 samples of signal d[ 0..159 ]).  These parts produce at
     the output of the coder:
  */

  word   *LARc, /* [ 0..7 ] LAR coefficients    OUT */

  /*
     Procedure 4.2.11 to 4.2.18 are to be executed four times per
     frame.  That means once for each sub-segment RPE-LTP analysis of
     40 samples.  These parts produce at the output of the coder:
  */

  word   *Nc, /* [ 0..3 ] LTP lag     OUT   */
  word   *bc, /* [ 0..3 ] coded LTP gain    OUT   */
  word   *Mc, /* [ 0..3 ] RPE grid selection    OUT     */
  word   *xmaxc,/* [ 0..3 ] Coded maximum amplitude OUT */
  word   *xMc /* [ 13*4 ] normalized RPE samples  OUT */
)
{
  int k;
  word   *dp  = S->dp0 + 120; /* [  -120...-1  ] */
  word   *dpp = dp;   /* [  0...39  ]  */

  static word e [ 50 ] = {0};

  word  so[ 160 ];

  gsm_enc_Gsm_Preprocess      ( S, s, so );
  gsm_enc_Gsm_LPC_Analysis    ( so, LARc );
  gsm_enc_Gsm_Short_Term_Analysis_Filter  ( S, LARc, so );

  _Pragma( "loopbound min 4 max 4" )
  for ( k = 0; k <= 3; k++, xMc += 13 ) {

    gsm_enc_Gsm_Long_Term_Predictor (
      so + k * 40, /* d      [ 0..39 ] IN */
      dp,   /* dp  [ -120..-1 ] IN  */
      e + 5,    /* e      [ 0..39 ] OUT */
      dpp,    /* dpp    [ 0..39 ] OUT */
      Nc++,
      bc++ );

    gsm_enc_Gsm_RPE_Encoding  (
      e + 5,  /* e     ][ 0..39 ][  IN/OUT */
      xmaxc++, Mc++, xMc );
    /*
       gsm_enc_Gsm_Update_of_reconstructed_short_time_residual_signal
            ( dpp, e + 5, dp );
    */

    {
      int i;
      longword ltmp;
      _Pragma( "loopbound min 40 max 40" )
      for ( i = 0; i <= 39; i++ )
        dp[  i  ] = GSM_ADD( e[ 5 + i ], dpp[ i ] );
    }

    dp  += 40;
    dpp += 40;

  }
//  //(void)memcpy( (char *)S->dp0, (char *)(S->dp0 + 160),
//  //  120 * sizeof(*S->dp0) );
}

/* rpe.c */
/*  4.2.13 .. 4.2.17  RPE ENCODING SECTION
*/

/* 4.2.13 */

void gsm_enc_Weighting_filter (
  word   *e,    /* signal [ -5..0.39.44 ] IN  */
  word     *x   /* signal [ 0..39 ] OUT */
)
/*
    The coefficients of the weighting filter are stored in a table
    (see table 4.4).  The following scaling is used:

    H[ 0..10 ] = integer( real_H[  0..10 ] * 8192 );
*/
{
  /* word     wt[  50  ]; */

  longword  L_result;
  int   k /* , i */ ;

  /*  Initialization of a temporary working array wt[ 0...49 ]
  */

  /* for (k =  0; k <=  4; k++) wt[ k ] = 0;
     for (k =  5; k <= 44; k++) wt[ k ] = *e++;
     for (k = 45; k <= 49; k++) wt[ k ] = 0;

      (e[ -5..-1 ] and e[ 40..44 ] are allocated by the caller,
      are initially zero and are not written anywhere.)
  */
  e -= 5;

  /*  Compute the signal x[ 0..39 ]
  */
  _Pragma( "loopbound min 40 max 40" )
  for ( k = 0; k <= 39; k++ ) {

    L_result = 8192 >> 1;

    /* for (i = 0; i <= 10; i++) {
        L_temp   = GSM_L_MULT( wt[ k+i ], gsm_enc_H[ i ] );
        L_result = GSM_L_ADD( L_result, L_temp );
       }
    */

#undef  STEP
#define STEP( i, H )  (e[  k + i  ] * (longword)H)

    /*  Every one of these multiplications is done twice --
        but I don't see an elegant way to optimize this.
        Do you?
    */

    #ifdef  STUPID_COMPILER
    L_result += STEP( 0,  -134 ) ;
    L_result += STEP( 1,  -374 )  ;
    /* + STEP(  2,  0    )  */
    L_result += STEP( 3,  2054 ) ;
    L_result += STEP( 4,  5741 ) ;
    L_result += STEP( 5,  8192 ) ;
    L_result += STEP( 6,  5741 ) ;
    L_result += STEP( 7,  2054 ) ;
    /* + STEP(  8,  0    )  */
    L_result += STEP( 9,  -374 ) ;
    L_result += STEP( 10,   -134 ) ;
    #else
    L_result +=
      STEP( 0,  -134 )
      + STEP( 1,  -374 )
      /* + STEP(  2,  0    )  */
      + STEP( 3,  2054 )
      + STEP( 4,  5741 )
      + STEP( 5,  8192 )
      + STEP( 6,  5741 )
      + STEP( 7,  2054 )
      /* + STEP(  8,  0    )  */
      + STEP( 9,  -374 )
      + STEP( 10,  -134 )
      ;
    #endif
    /* L_result = GSM_L_ADD( L_result, L_result ); (* scaling(x2) *)
       L_result = GSM_L_ADD( L_result, L_result ); (* scaling(x4) *)

       x[ k ] = SASR( L_result, 16 );
    */

    /* 2 adds vs. >>16 => 14, minus one shift to compensate for
       those we lost when replacing L_MULT by '*'.
    */

    L_result = SASR( L_result, 13 );
    x[ k ] =  (  L_result < MIN_WORD ? MIN_WORD
               : ( L_result > MAX_WORD ? MAX_WORD : L_result ) );
  }
}

/* 4.2.14 */

void gsm_enc_RPE_grid_selection (
  word     *x,    /* [ 0..39 ]    IN  */
  word     *xM,   /* [ 0..12 ]    OUT */
  word     *Mc_out  /*      OUT */
)
/*
    The signal x[ 0..39 ] is used to select the RPE grid which is
    represented by Mc.
*/
{
  /* word temp1;  */
  int   /* m, */  i;
  longword  L_result, L_temp;
  longword    EM; /* xxx should be L_EM? */
  word      Mc;

  longword    L_common_0_3;

  Mc = 0;

#undef  STEP
#define STEP( m, i )    L_temp = SASR( x[ m + 3 * i ], 2 ); \
  L_result += L_temp * L_temp;

  /* common part of 0 and 3 */

  L_result = 0;
  STEP( 0, 1 );
  STEP( 0, 2 );
  STEP( 0, 3 );
  STEP( 0, 4 );
  STEP( 0, 5 );
  STEP( 0, 6 );
  STEP( 0, 7 );
  STEP( 0, 8 );
  STEP( 0, 9 );
  STEP( 0, 10 );
  STEP( 0, 11 );
  STEP( 0, 12 );
  L_common_0_3 = L_result;

  /* i = 0 */

  STEP( 0, 0 );
  L_result <<= 1; /* implicit in L_MULT */
  EM = L_result;

  /* i = 1 */

  L_result = 0;
  STEP( 1, 0 );
  STEP( 1, 1 );
  STEP( 1, 2 );
  STEP( 1, 3 );
  STEP( 1, 4 );
  STEP( 1, 5 );
  STEP( 1, 6 );
  STEP( 1, 7 );
  STEP( 1, 8 );
  STEP( 1, 9 );
  STEP( 1, 10 );
  STEP( 1, 11 );
  STEP( 1, 12 );
  L_result <<= 1;
  if ( L_result > EM ) {
    Mc = 1;
    EM = L_result;
  }

  /* i = 2 */

  L_result = 0;
  STEP( 2, 0 );
  STEP( 2, 1 );
  STEP( 2, 2 );
  STEP( 2, 3 );
  STEP( 2, 4 );
  STEP( 2, 5 );
  STEP( 2, 6 );
  STEP( 2, 7 );
  STEP( 2, 8 );
  STEP( 2, 9 );
  STEP( 2, 10 );
  STEP( 2, 11 );
  STEP( 2, 12 );
  L_result <<= 1;
  if ( L_result > EM ) {
    Mc = 2;
    EM = L_result;
  }

  /* i = 3 */

  L_result = L_common_0_3;
  STEP( 3, 12 );
  L_result <<= 1;
  if ( L_result > EM )
    Mc = 3;

  /**/

  /*  Down-sampling by a factor 3 to get the selected xM[ 0..12 ]
      RPE sequence.
  */
  _Pragma( "loopbound min 13 max 13" )
  for ( i = 0; i <= 12; i ++ ) xM[ i ] = x[ Mc + 3 * i ];
  *Mc_out = Mc;

}

/* 4.12.15 */

void gsm_enc_APCM_quantization_xmaxc_to_exp_mant (
  word    xmaxc,    /* IN   */
  word     *exp_out,  /* OUT  */
  word     *mant_out )  /* OUT  */
{
  word  exp, mant;

  /* Compute exponent and mantissa of the decoded version of xmaxc
  */
  exp = 0;
  if ( xmaxc > 15 ) exp = SASR( xmaxc, 3 ) - 1;
  mant = xmaxc - ( exp << 3 );

  if ( mant == 0 ) {
    exp  = -4;
    mant = 7;
  } else {
    _Pragma( "loopbound min 0 max 3" )
    while ( mant <= 7 ) {
      mant = mant << 1 | 1;
      exp--;
    }
    mant -= 8;
  }

  *exp_out  = exp;
  *mant_out = mant;

}

void gsm_enc_APCM_quantization (
  word     *xM,   /* [ 0..12 ]    IN  */

  word     *xMc,    /* [ 0..12 ]    OUT */
  word     *mant_out, /*      OUT */
  word     *exp_out,  /*      OUT */
  word     *xmaxc_out /*      OUT */
)
{
  int i, itest;

  word  xmax, xmaxc, temp, temp1, temp2;
  word  exp, mant;


  /*  Find the maximum absolute value xmax of xM[ 0..12 ].
  */

  xmax = 0;

  _Pragma( "loopbound min 13 max 13" )
  for ( i = 0; i <= 12; i++ ) {
    temp = xM[ i ];
    temp = GSM_ABS( temp );
    if ( temp > xmax ) xmax = temp;
  }

  /*  Qantizing and coding of xmax to get xmaxc.
  */

  exp   = 0;
  temp  = SASR( xmax, 9 );
  itest = 0;

  _Pragma( "loopbound min 6 max 6" )
  for ( i = 0; i <= 5; i++ ) {

    itest |= ( temp <= 0 );
    temp = SASR( temp, 1 );

    if ( itest == 0 ) exp++;  // exp = add (exp, 1)
  }

  temp = exp + 5;

  //xmaxc = gsm_enc_add( SASR(xmax, temp), exp << 3 );
  xmaxc = saturate( ( SASR( xmax, temp ) + ( exp << 3 ) ) );

  /*   Quantizing and coding of the xM[ 0..12 ] RPE sequence
       to get the xMc[ 0..12 ]
  */

  gsm_enc_APCM_quantization_xmaxc_to_exp_mant( xmaxc, &exp, &mant );

  /*  This computation uses the fact that the decoded version of xmaxc
      can be calculated by using the exponent and the mantissa part of
      xmaxc (logarithmic table).
      So, this method avoids any division and uses only a scaling
      of the RPE samples by a function of the exponent.  A direct
      multiplication by the inverse of the mantissa (NRFAC[ 0..7 ]
      found in table 4.5) gives the 3 bit coded version xMc[ 0..12 ]
      of the RPE samples.
  */


  /* Direct computation of xMc[ 0..12 ] using table 4.5
  */


  temp1 = 6 - exp;    /* normalization by the exponent */
  temp2 = gsm_enc_NRFAC[  mant  ];    /* inverse mantissa      */

  _Pragma( "loopbound min 13 max 13" )
  for ( i = 0; i <= 12; i++ ) {

    temp = xM[ i ] << temp1;
    temp = GSM_MULT( temp, temp2 );
    temp = SASR( temp, 12 );
    xMc[ i ] = temp + 4;    /* see note below */
  }

  /*  NOTE: This equation is used to make all the xMc[ i ] positive.
  */

  *mant_out  = mant;
  *exp_out   = exp;
  *xmaxc_out = xmaxc;

}

/* 4.2.16 */

void gsm_enc_APCM_inverse_quantization (
  word   *xMc,  /* [ 0..12 ]      IN  */
  word    mant,
  word    exp,
  word   *xMp ) /* [ 0..12 ]      OUT   */
/*
    This part is for decoding the RPE sequence of coded xMc[ 0..12 ]
    samples to obtain the xMp[ 0..12 ] array.  Table 4.6 is used to get
    the mantissa of xmaxc (FAC[ 0..7 ]).
*/
{
  int i;
  word  temp, temp1, temp2, temp3;
  longword  ltmp;

  temp1 = gsm_enc_FAC[  mant  ];  /* see 4.2-15 for mant */
  temp2 = gsm_enc_sub( 6, exp );  /* see 4.2-15 for exp  */
  temp3 = gsm_enc_asl( 1, gsm_enc_sub( temp2, 1 ) );

  _Pragma( "loopbound min 13 max 13" )
  for ( i = 13; i--; ) {

    /* temp = gsm_enc_sub( *xMc++ << 1, 7 ); */
    temp = ( *xMc++ << 1 ) - 7;       /* restore sign   */

    temp <<= 12;        /* 16 bit signed  */
    temp = GSM_MULT_R( temp1, temp );
    temp = GSM_ADD( temp, temp3 );
    *xMp++ = gsm_enc_asr( temp, temp2 );
  }
}

/* 4.2.17 */

void gsm_enc_RPE_grid_positioning (
  word    Mc,   /* grid position  IN  */
  word   *xMp,    /* [ 0..12 ]    IN  */
  word   *ep    /* [ 0..39 ]    OUT */
)
/*
    This procedure computes the reconstructed long term residual signal
    ep[ 0..39 ] for the LTP analysis filter.  The inputs are the Mc
    which is the grid position selection and the xMp[ 0..12 ] decoded
    RPE samples which are upsampled by a factor of 3 by inserting zero
    values.
*/
{
  int i = 13;

  //
  // TODO: rewritten Duff's device for WCET analysis!
  //
  switch ( Mc ) {
    case 3:
      *ep++ = 0;
    case 2:
      *ep++ = 0;
    case 1:
      *ep++ = 0;
    case 0:
      *ep++ = *xMp++;
      i--;
  }

  _Pragma( "loopbound min 12 max 12" )
  do {
    *ep++ = 0;
    *ep++ = 0;
    *ep++ = *xMp++;
  } while ( --i );

  _Pragma( "loopbound min 0 max 3" )
  while ( ++Mc < 4 ) *ep++ = 0;

}
/*
  {
  int i = 13;

  //
  //TODO: removed for WCET analysis
  //_Pragma("marker outside")
  switch (Mc) {
    case 3: *ep++ = 0;
    case 2:
      _Pragma("loopbound min 13 max 13")
      do {
          ep++ = 0;
         case 1:         *ep++ = 0;
         case 0:
             //_Pragma("marker inside")
              ep++ = *xMp++;
       } while (--i);
  }

  //_Pragma("flowrestriction 1*inside <=  13*outside")

  _Pragma("loopbound min 0 max 3")
  while (++Mc < 4) *ep++ = 0;

  }
*/

/* 4.2.18 */

/*  This procedure adds the reconstructed long term residual signal
    ep[ 0..39 ] to the estimated signal dpp[ 0..39 ] from the long term
    analysis filter to compute the reconstructed short term residual
    signal dp[ -40..-1 ]; also the reconstructed short term residual
    array dp[ -120..-41 ] is updated.
*/

#if 0 /* Has been inlined in code.c */
void gsm_enc_Gsm_Update_of_reconstructed_short_time_residual_signal P3( ( dpp,
    ep, dp ),
    word   *dpp,    /* [ 0...39 ] IN  */
    word   *ep,   /* [ 0...39 ] IN  */
    word   *dp )  /* [ -120...-1 ]  IN/OUT  */
{
  int     k;

  _Pragma( "loopbound min 80 max 80" )
  for ( k = 0; k <= 79; k++ )
    dp[  -120 + k  ] = dp[  -80 + k  ];

  _Pragma( "loopbound min 40 max 40" )
  for ( k = 0; k <= 39; k++ )
    dp[  -40 + k  ] = gsm_enc_add( ep[ k ], dpp[ k ] );
}
#endif  /* Has been inlined in code.c */

void gsm_enc_Gsm_RPE_Encoding (

  word   *e,    /* -5..-1 ][ 0..39 ][ 40..44  IN/OUT  */
  word   *xmaxc,  /*        OUT */
  word   *Mc,   /*          OUT */
  word   *xMc )   /* [ 0..12 ]      OUT */
{
  word  x[ 40 ];
  word  xM[ 13 ], xMp[ 13 ];
  word  mant, exp;

  gsm_enc_Weighting_filter( e, x );
  gsm_enc_RPE_grid_selection( x, xM, Mc );

  gsm_enc_APCM_quantization( xM, xMc, &mant, &exp, xmaxc );
  gsm_enc_APCM_inverse_quantization(  xMc,  mant,  exp, xMp );

  gsm_enc_RPE_grid_positioning( *Mc, xMp, e );

}

/* long_term.c */
#ifdef  USE_TABLE_MUL

unsigned int umul_table[  513  ][  256  ];

# define umul(x9, x15)  \
  ((int)(umul_table[ x9 ][ x15 & 0x0FF ] + (umul_table[ x9 ][  x15 >> 8  ] << 8)))

# define table_mul(a, b)  \
  ( (a < 0)  ? ((b < 0) ? umul(-a, -b) : -umul(-a, b))  \
         : ((b < 0) ? -umul(a, -b) :  umul(a, b)))

#endif /* USE_TABLE_MUL */



/*
    4.2.11 .. 4.2.12 LONG TERM PREDICTOR (LTP) SECTION
*/


/*
   This procedure computes the LTP gain (bc) and the LTP lag (Nc)
   for the long term analysis filter.   This is done by calculating a
   maximum of the cross-correlation function between the current
   sub-segment short term residual signal d[ 0..39 ] (output of
   the short term analysis filter; for simplification the index
   of this array begins at 0 and ends at 39 for each sub-segment of the
   RPE-LTP analysis) and the previous reconstructed short term
   residual signal dp[  -120 .. -1  ].  A dynamic scaling must be
   performed to avoid overflow.
*/

/* This procedure exists in four versions.  First, the two integer
   versions with or without table-multiplication (as one function);
   then, the two floating point versions (as another function), with
   or without scaling.
*/

#ifndef  USE_FLOAT_MUL

void gsm_enc_Calculation_of_the_LTP_parameters (
  word   *d,    /* [ 0..39 ]  IN  */
  word   *dp,   /* [ -120..-1 ] IN  */
  word     *bc_out, /*    OUT */
  word     *Nc_out  /*    OUT */
)
{
  int   k, lambda;
  word    Nc, bc;
  word    wt[ 40 ];

  longword  L_max, L_power;
  word    R, S, dmax, scal;
  word  temp;

  /*  Search of the optimum scaling of d[ 0..39 ].
  */
  dmax = 0;

  _Pragma( "loopbound min 40 max 40" )
  for ( k = 0; k <= 39; k++ ) {
    temp = d[ k ];
    temp = GSM_ABS( temp );
    if ( temp > dmax ) dmax = temp;
  }

  temp = 0;
  if ( dmax != 0 )
    temp = gsm_enc_norm( ( longword )dmax << 16 );

  if ( temp > 6 ) scal = 0;
  else scal = 6 - temp;


  /*  Initialization of a working array wt
  */

  _Pragma( "loopbound min 40 max 40" )
  for ( k = 0; k <= 39; k++ ) wt[ k ] = SASR( d[ k ], scal );

  /* Search for the maximum cross-correlation and coding of the LTP lag
  */
  L_max = 0;
  Nc    = 40; /* index for the maximum cross-correlation */

  _Pragma( "loopbound min 81 max 81" )
  for ( lambda = 40; lambda <= 120; lambda++ ) {

# undef STEP
    # ifdef USE_TABLE_MUL
#   define STEP(k) (table_mul(wt[ k ], dp[ k - lambda ]))
    # else
#   define STEP(k) (wt[ k ] * dp[ k - lambda ])
    # endif

    longword L_result;

    L_result  = STEP( 0 )  ;
    L_result += STEP( 1 ) ;
    L_result += STEP( 2 )  ;
    L_result += STEP( 3 ) ;
    L_result += STEP( 4 )  ;
    L_result += STEP( 5 )  ;
    L_result += STEP( 6 )  ;
    L_result += STEP( 7 )  ;
    L_result += STEP( 8 )  ;
    L_result += STEP( 9 )  ;
    L_result += STEP( 10 ) ;
    L_result += STEP( 11 ) ;
    L_result += STEP( 12 ) ;
    L_result += STEP( 13 ) ;
    L_result += STEP( 14 ) ;
    L_result += STEP( 15 ) ;
    L_result += STEP( 16 ) ;
    L_result += STEP( 17 ) ;
    L_result += STEP( 18 ) ;
    L_result += STEP( 19 ) ;
    L_result += STEP( 20 ) ;
    L_result += STEP( 21 ) ;
    L_result += STEP( 22 ) ;
    L_result += STEP( 23 ) ;
    L_result += STEP( 24 ) ;
    L_result += STEP( 25 ) ;
    L_result += STEP( 26 ) ;
    L_result += STEP( 27 ) ;
    L_result += STEP( 28 ) ;
    L_result += STEP( 29 ) ;
    L_result += STEP( 30 ) ;
    L_result += STEP( 31 ) ;
    L_result += STEP( 32 ) ;
    L_result += STEP( 33 ) ;
    L_result += STEP( 34 ) ;
    L_result += STEP( 35 ) ;
    L_result += STEP( 36 ) ;
    L_result += STEP( 37 ) ;
    L_result += STEP( 38 ) ;
    L_result += STEP( 39 ) ;

    if ( L_result > L_max ) {

      Nc    = lambda;
      L_max = L_result;
    }
  }

  *Nc_out = Nc;

  L_max <<= 1;

  /*  Rescaling of L_max
  */
  L_max = L_max >> ( 6 - scal ); /* sub(6, scal) */

  /*   Compute the power of the reconstructed short term residual
       signal dp[ .. ]
  */
  L_power = 0;
  _Pragma( "loopbound min 40 max 40" )
  for ( k = 0; k <= 39; k++ ) {

    longword L_temp;

    L_temp   = SASR( dp[ k - Nc ], 3 );
    L_power += L_temp * L_temp;
  }
  L_power <<= 1;  /* from L_MULT */

  /*  Normalization of L_max and L_power
  */

  if ( L_max <= 0 )  {
    *bc_out = 0;
    return;
  }
  if ( L_max >= L_power ) {
    *bc_out = 3;
    return;
  }

  temp = gsm_enc_norm( L_power );

  R = SASR( L_max   << temp, 16 );
  S = SASR( L_power << temp, 16 );

  /*  Coding of the LTP gain
  */

  /*  Table 4.3a must be used to obtain the level DLB[ i ] for the
      quantization of the LTP gain b to get the coded version bc.
  */
  _Pragma( "loopbound min 3 max 3" )
  for ( bc = 0; bc <= 2; bc++ )
    /* Replaced by macro function. */
    //if (R <= gsm_enc_mult(S, gsm_enc_DLB[ bc ]))
    if ( R <= GSM_MULT( S, gsm_enc_DLB[ bc ] ) )
      break;

  *bc_out = bc;
}

#else /* USE_FLOAT_MUL */

void gsm_enc_Calculation_of_the_LTP_parameters (
  word   *d,    /* [ 0..39 ]  IN  */
  word   *dp,   /* [ -120..-1 ] IN  */
  word     *bc_out, /*    OUT */
  word     *Nc_out  /*    OUT */
)
{
  int   k, lambda;
  word    Nc, bc;

  float   wt_float[ 40 ];
  float   dp_float_base[ 120 ], * dp_float = dp_float_base + 120;

  longword  L_max, L_power;
  word    R, S, dmax, scal;
  word  temp;

  /*  Search of the optimum scaling of d[ 0..39 ].
  */
  dmax = 0;

  _Pragma( "loopbound min 40 max 40" )
  for ( k = 0; k <= 39; k++ ) {
    temp = d[ k ];
    temp = GSM_ABS( temp );
    if ( temp > dmax ) dmax = temp;
  }

  temp = 0;
  if ( dmax == 0 ) scal = 0;
  else
    temp = gsm_enc_norm( ( longword )dmax << 16 );

  if ( temp > 6 ) scal = 0;
  else scal = 6 - temp;

  /*  Initialization of a working array wt
  */

  _Pragma( "loopbound min 40 max 40" )
  for ( k =    0; k < 40; k++ ) wt_float[ k ] =  SASR( d[ k ], scal );
  _Pragma( "loopbound min 120 max 120" )
  for ( k = -120; k <  0; k++ ) dp_float[ k ] =  dp[ k ];

  /* Search for the maximum cross-correlation and coding of the LTP lag
  */
  L_max = 0;
  Nc    = 40; /* index for the maximum cross-correlation */

  _Pragma( "loopbound min 9 max 9" )
  for ( lambda = 40; lambda <= 120; lambda += 9 ) {

    /*  Calculate L_result for l = lambda .. lambda + 9.
    */
    float *lp = dp_float - lambda;

    float W;
    float a = lp[ -8 ], b = lp[ -7 ], c = lp[ -6 ],
          d = lp[ -5 ], e = lp[ -4 ], f = lp[ -3 ],
          g = lp[ -2 ], h = lp[ -1 ];
    float  E;
    float  S0 = 0, S1 = 0, S2 = 0, S3 = 0, S4 = 0,
           S5 = 0, S6 = 0, S7 = 0, S8 = 0;

#   undef STEP
#   define  STEP(K, a, b, c, d, e, f, g, h) \
    W = wt_float[ K ];    \
    E = W * a; S8 += E;   \
    E = W * b; S7 += E;   \
    E = W * c; S6 += E;   \
    E = W * d; S5 += E;   \
    E = W * e; S4 += E;   \
    E = W * f; S3 += E;   \
    E = W * g; S2 += E;   \
    E = W * h; S1 += E;   \
    a  = lp[ K ];     \
    E = W * a; S0 += E

#   define  STEP_A(K) STEP(K, a, b, c, d, e, f, g, h)
#   define  STEP_B(K) STEP(K, b, c, d, e, f, g, h, a)
#   define  STEP_C(K) STEP(K, c, d, e, f, g, h, a, b)
#   define  STEP_D(K) STEP(K, d, e, f, g, h, a, b, c)
#   define  STEP_E(K) STEP(K, e, f, g, h, a, b, c, d)
#   define  STEP_F(K) STEP(K, f, g, h, a, b, c, d, e)
#   define  STEP_G(K) STEP(K, g, h, a, b, c, d, e, f)
#   define  STEP_H(K) STEP(K, h, a, b, c, d, e, f, g)

    STEP_A( 0 );
    STEP_B( 1 );
    STEP_C( 2 );
    STEP_D( 3 );
    STEP_E( 4 );
    STEP_F( 5 );
    STEP_G( 6 );
    STEP_H( 7 );

    STEP_A( 8 );
    STEP_B( 9 );
    STEP_C( 10 );
    STEP_D( 11 );
    STEP_E( 12 );
    STEP_F( 13 );
    STEP_G( 14 );
    STEP_H( 15 );

    STEP_A( 16 );
    STEP_B( 17 );
    STEP_C( 18 );
    STEP_D( 19 );
    STEP_E( 20 );
    STEP_F( 21 );
    STEP_G( 22 );
    STEP_H( 23 );

    STEP_A( 24 );
    STEP_B( 25 );
    STEP_C( 26 );
    STEP_D( 27 );
    STEP_E( 28 );
    STEP_F( 29 );
    STEP_G( 30 );
    STEP_H( 31 );

    STEP_A( 32 );
    STEP_B( 33 );
    STEP_C( 34 );
    STEP_D( 35 );
    STEP_E( 36 );
    STEP_F( 37 );
    STEP_G( 38 );
    STEP_H( 39 );

    if ( S0 > L_max ) {
      L_max = S0;
      Nc = lambda;
    }
    if ( S1 > L_max ) {
      L_max = S1;
      Nc = lambda + 1;
    }
    if ( S2 > L_max ) {
      L_max = S2;
      Nc = lambda + 2;
    }
    if ( S3 > L_max ) {
      L_max = S3;
      Nc = lambda + 3;
    }
    if ( S4 > L_max ) {
      L_max = S4;
      Nc = lambda + 4;
    }
    if ( S5 > L_max ) {
      L_max = S5;
      Nc = lambda + 5;
    }
    if ( S6 > L_max ) {
      L_max = S6;
      Nc = lambda + 6;
    }
    if ( S7 > L_max ) {
      L_max = S7;
      Nc = lambda + 7;
    }
    if ( S8 > L_max ) {
      L_max = S8;
      Nc = lambda + 8;
    }
  }
  *Nc_out = Nc;

  L_max <<= 1;

  /*  Rescaling of L_max
  */
  L_max = L_max >> ( 6 - scal ); /* sub(6, scal) */

  /*   Compute the power of the reconstructed short term residual
       signal dp[ .. ]
  */
  L_power = 0;
  _Pragma( "loopbound min 40 max 40" )
  for ( k = 0; k <= 39; k++ ) {

    longword L_temp;

    L_temp   = SASR( dp[ k - Nc ], 3 );
    L_power += L_temp * L_temp;
  }
  L_power <<= 1;  /* from L_MULT */

  /*  Normalization of L_max and L_power
  */

  if ( L_max <= 0 )  {
    *bc_out = 0;
    return;
  }
  if ( L_max >= L_power ) {
    *bc_out = 3;
    return;
  }

  temp = gsm_enc_norm( L_power );

  R = SASR( L_max   << temp, 16 );
  S = SASR( L_power << temp, 16 );

  /*  Coding of the LTP gain
  */

  /*  Table 4.3a must be used to obtain the level DLB[ i ] for the
      quantization of the LTP gain b to get the coded version bc.
  */
  // Replaced by macro function.
  //for (bc = 0; bc <= 2; bc++) if (R <= gsm_enc_mult(S, gsm_enc_DLB[ bc ])) break;
  _Pragma( "loopbound min 3 max 3" )
  for ( bc = 0; bc <= 2; bc++ ) if ( R <= GSM_MULT( S, gsm_enc_DLB[ bc ] ) ) break;
  *bc_out = bc;
}

#endif  /* USE_FLOAT_MUL */


/* 4.2.12 */

void gsm_enc_Long_term_analysis_filtering (
  word    bc, /*          IN  */
  word    Nc, /*          IN  */
  word   *dp, /* previous d [ -120..-1 ]    IN  */
  word   *d,  /* d    [ 0..39 ]     IN  */
  word   *dpp,  /* estimate [ 0..39 ]     OUT */
  word   *e /* long term res. signal [ 0..39 ]  OUT */
)
/*
    In this part, we have to decode the bc parameter to compute
    the samples of the estimate dpp[ 0..39 ].  The decoding of bc needs the
    use of table 4.3b.  The long term residual signal e[ 0..39 ]
    is then calculated to be fed to the RPE encoding section.
*/
{
  int      k;
  longword ltmp;

# undef STEP
# define STEP(BP)         \
  _Pragma("loopbound min 40 max 40") \
  for (k = 0; k <= 39; k++) {     \
    dpp[ k ]  = GSM_MULT_R( BP, dp[ k - Nc ]);  \
    e[ k ]  = GSM_SUB( d[ k ], dpp[ k ] );  \
  }

  switch ( bc ) {
    case 0:
      STEP(  3277 );
      break;
    case 1:
      STEP( 11469 );
      break;
    case 2:
      STEP( 21299 );
      break;
    case 3:
      STEP( 32767 );
      break;
  }
}

void gsm_enc_Gsm_Long_Term_Predictor (

  word   *d,  /* [ 0..39 ]   residual signal  IN  */
  word   *dp, /* [ -120..-1 ] d'    IN  */

  word   *e,  /* [ 0..39 ]      OUT */
  word   *dpp,  /* [ 0..39 ]      OUT */
  word   *Nc, /* correlation lag    OUT */
  word   *bc  /* gain factor      OUT */
)
{

  gsm_enc_Calculation_of_the_LTP_parameters( d, dp, bc, Nc );

  gsm_enc_Long_term_analysis_filtering( *bc, *Nc, dp, d, dpp, e );
}

/* short_term.c */
/*
    SHORT TERM ANALYSIS FILTERING SECTION
*/

/* 4.2.8 */

void gsm_enc_Decoding_of_the_coded_Log_Area_Ratios (
  word   *LARc,   /* coded log area ratio [ 0..7 ]  IN  */
  word   *LARpp ) /* out: decoded ..      */
{
  word  temp1 /* , temp2 */;
  long  ltmp; /* for GSM_ADD */

  /*  This procedure requires for efficient implementation
      two tables.

      INVA[ 1..8 ] = integer( (32768 * 8) / real_A[ 1..8 ])
      MIC[ 1..8 ]  = minimum value of the LARc[ 1..8 ]
  */

  /*  Compute the LARpp[ 1..8 ]
  */

#undef  STEP
#define STEP( B, MIC, INVA )  \
  temp1    = GSM_ADD( *LARc++, MIC ) << 10; \
  temp1    = GSM_SUB( temp1, (B >= 0 ? B << 1 : -((-B) << 1)));   \
  temp1    = GSM_MULT_R( INVA, temp1 );   \
  *LARpp++ = GSM_ADD( temp1, temp1 );

  STEP(      0,  -32,  13107 );
  STEP(      0,  -32,  13107 );
  STEP(   2048,  -16,  13107 );
  STEP(  -2560,  -16,  13107 );

  STEP(     94,   -8,  19223 );
  STEP(  -1792,   -8,  17476 );
  STEP(   -341,   -4,  31454 );
  STEP(  -1144,   -4,  29708 );

  /* NOTE: the addition of *MIC is used to restore
       the sign of *LARc.
  */
}

/* 4.2.9 */
/* Computation of the quantized reflection coefficients
*/

/* 4.2.9.1  Interpolation of the LARpp[ 1..8 ] to get the LARp[ 1..8 ]
*/

/*
    Within each frame of 160 analyzed speech samples the short term
    analysis and synthesis filters operate with four different sets of
    coefficients, derived from the previous set of decoded LARs(LARpp(j-1))
    and the actual set of decoded LARs (LARpp(j))

   (Initial value: LARpp(j-1)[ 1..8 ] = 0.)
*/

void gsm_enc_Coefficients_0_12 (
  word *LARpp_j_1,
  word *LARpp_j,
  word *LARp )
{
  int   i;
  longword ltmp;

  _Pragma( "loopbound min 8 max 8" )
  for ( i = 1; i <= 8; i++, LARp++, LARpp_j_1++, LARpp_j++ ) {
    *LARp = GSM_ADD( SASR( *LARpp_j_1, 2 ), SASR( *LARpp_j, 2 ) );
    *LARp = GSM_ADD( *LARp,  SASR( *LARpp_j_1, 1 ) );
  }
}

void gsm_enc_Coefficients_13_26 (
  word *LARpp_j_1,
  word *LARpp_j,
  word *LARp )
{
  int i;
  longword ltmp;
  _Pragma( "loopbound min 8 max 8" )
  for ( i = 1; i <= 8; i++, LARpp_j_1++, LARpp_j++, LARp++ )
    *LARp = GSM_ADD( SASR( *LARpp_j_1, 1 ), SASR( *LARpp_j, 1 ) );
}

void gsm_enc_Coefficients_27_39 (
  word *LARpp_j_1,
  word *LARpp_j,
  word *LARp )
{
  int i;
  longword ltmp;

  _Pragma( "loopbound min 8 max 8" )
  for ( i = 1; i <= 8; i++, LARpp_j_1++, LARpp_j++, LARp++ ) {
    *LARp = GSM_ADD( SASR( *LARpp_j_1, 2 ), SASR( *LARpp_j, 2 ) );
    *LARp = GSM_ADD( *LARp, SASR( *LARpp_j, 1 ) );
  }
}


void gsm_enc_Coefficients_40_159 (
  word *LARpp_j,
  word *LARp )
{
  int i;

  _Pragma( "loopbound min 8 max 8" )
  for ( i = 1; i <= 8; i++, LARp++, LARpp_j++ )
    *LARp = *LARpp_j;
}

/* 4.2.9.2 */

void gsm_enc_LARp_to_rp (
  word *LARp )  /* [ 0..7 ] IN/OUT  */
/*
    The input of this procedure is the interpolated LARp[ 0..7 ] array.
    The reflection coefficients, rp[ i ], are used in the analysis
    filter and in the synthesis filter.
*/
{
  int     i;
  word    temp;
  longword  ltmp;

  _Pragma( "loopbound min 8 max 8" )
  for ( i = 1; i <= 8; i++, LARp++ ) {

    /* temp = GSM_ABS( *LARp );

       if (temp < 11059) temp <<= 1;
       else if (temp < 20070) temp += 11059;
       else temp = GSM_ADD( temp >> 2, 26112 );

     * *LARp = *LARp < 0 ? -temp : temp;
    */

    if ( *LARp < 0 ) {
      temp = *LARp == MIN_WORD ? MAX_WORD : -( *LARp );
      *LARp = - ( ( temp < 11059 ) ? temp << 1
                  : ( ( temp < 20070 ) ? temp + 11059
                      :  GSM_ADD( temp >> 2, 26112 ) ) );
    } else {
      temp  = *LARp;
      *LARp =    ( temp < 11059 ) ? temp << 1
                 : ( ( temp < 20070 ) ? temp + 11059
                     :  GSM_ADD( temp >> 2, 26112 ) );
    }
  }
}


/* 4.2.10 */
void gsm_enc_Short_term_analysis_filtering (
  struct gsm_state *S,
  word   *rp, /* [ 0..7 ] IN  */
  int   k_n,  /*   k_end - k_start  */
  word   *s /* [ 0..n-1 ] IN/OUT  */
)
/*
    This procedure computes the short term residual signal d[ .. ] to be fed
    to the RPE-LTP loop from the s[ .. ] signal and from the local rp[ .. ]
    array (quantized reflection coefficients).  As the call of this
    procedure can be done in many ways (see the interpolation of the LAR
    coefficient), it is assumed that the computation begins with index
    k_start (for arrays d[ .. ] and s[ .. ]) and stops with index k_end
    (k_start and k_end are defined in 4.2.9.1).  This procedure also
    needs to keep the array u[ 0..7 ] in memory for each call.
*/
{
  word     *u = S->u;
  int   i;
  word    di, zzz, ui, sav, rpi;
  longword  ltmp;
  int j;

  _Pragma( "loopbound min 13 max 120" )
  for ( j = 0; j < k_n; ++j ) {

    di = sav = *s;

    _Pragma( "loopbound min 8 max 8" )
    for ( i = 0; i < 8; i++ ) { /* YYY */

      ui    = u[ i ];
      rpi   = rp[ i ];
      u[ i ]  = sav;

      zzz   = GSM_MULT_R( rpi, di );
      sav   = GSM_ADD(   ui,  zzz );

      zzz   = GSM_MULT_R( rpi, ui );
      di    = GSM_ADD(   di,  zzz );

    }

    *s = di;
  }
}

void gsm_enc_Gsm_Short_Term_Analysis_Filter (

  struct gsm_state *S,

  word   *LARc,   /* coded log area ratio [ 0..7 ]  IN  */
  word   *s   /* signal [ 0..159 ]    IN/OUT  */
)
{
  word     *LARpp_j = S->LARpp[  S->j       ];
  word     *LARpp_j_1 = S->LARpp[  S->j ^= 1  ];

  word    LARp[ 8 ];

#undef  FILTER
#   define  FILTER  gsm_enc_Short_term_analysis_filtering

  gsm_enc_Decoding_of_the_coded_Log_Area_Ratios( LARc, LARpp_j );

  gsm_enc_Coefficients_0_12(  LARpp_j_1, LARpp_j, LARp );
  gsm_enc_LARp_to_rp( LARp );
  FILTER( S, LARp, 13, s );

  gsm_enc_Coefficients_13_26( LARpp_j_1, LARpp_j, LARp );
  gsm_enc_LARp_to_rp( LARp );
  FILTER( S, LARp, 14, s + 13 );

  gsm_enc_Coefficients_27_39( LARpp_j_1, LARpp_j, LARp );
  gsm_enc_LARp_to_rp( LARp );
  FILTER( S, LARp, 13, s + 27 );

  gsm_enc_Coefficients_40_159( LARpp_j, LARp );
  gsm_enc_LARp_to_rp( LARp );
  FILTER( S, LARp, 120, s + 40 );
}

/* lpc.c */
#undef  P

/*
    4.2.4 .. 4.2.7 LPC ANALYSIS SECTION
*/

/* 4.2.4 */


void gsm_enc_Autocorrelation (
  word      *s,   /* [ 0..159 ] IN/OUT  */
  longword *L_ACF ) /* [ 0..8 ] OUT     */
/*
    The goal is to compute the array L_ACF[ k ].  The signal s[ i ] must
    be scaled in order to avoid an overflow situation.
*/
{
  int k, i;

  word    temp, smax, scalauto;

  /*  Dynamic scaling of the array  s[ 0..159 ]
  */

  /*  Search for the maximum.
  */
  smax = 0;

  _Pragma( "loopbound min 160 max 160" )
  for ( k = 0; k <= 159; k++ ) {
    temp = GSM_ABS( s[ k ] );
    if ( temp > smax ) smax = temp;
  }

  /*  Computation of the scaling factor.
  */
  if ( smax == 0 ) scalauto = 0;
  else {
    scalauto = 4 - gsm_enc_norm( ( longword )smax << 16 ); /* sub(4,..) */
  }

  /*  Scaling of the array s[ 0...159 ]
  */

  if ( scalauto > 0 ) {

#   define SCALE(n) \
  case n: \
    _Pragma("loopbound min 160 max 160") \
    for (k = 0; k <= 159; k++) \
      s[ k ] = GSM_MULT_R( s[ k ], 16384 >> (n-1) );\
    break;

    switch ( scalauto ) {
        SCALE( 1 )
        SCALE( 2 )
        SCALE( 3 )
        SCALE( 4 )
    }
# undef SCALE
  }

  /*  Compute the L_ACF[ .. ].
  */
  {
    word   *sp = s;
    word    sl = *sp;
#undef STEP
#   define STEP(k)   L_ACF[ k ] += ((longword)sl * sp[  -(k)  ]);

# define NEXTI   sl = *++sp


    _Pragma( "loopbound min 9 max 9" )
    for ( k = 9; k--; L_ACF[ k ] = 0 ) ;

    STEP ( 0 );
    NEXTI;
    STEP( 0 );
    STEP( 1 );
    NEXTI;
    STEP( 0 );
    STEP( 1 );
    STEP( 2 );
    NEXTI;
    STEP( 0 );
    STEP( 1 );
    STEP( 2 );
    STEP( 3 );
    NEXTI;
    STEP( 0 );
    STEP( 1 );
    STEP( 2 );
    STEP( 3 );
    STEP( 4 );
    NEXTI;
    STEP( 0 );
    STEP( 1 );
    STEP( 2 );
    STEP( 3 );
    STEP( 4 );
    STEP( 5 );
    NEXTI;
    STEP( 0 );
    STEP( 1 );
    STEP( 2 );
    STEP( 3 );
    STEP( 4 );
    STEP( 5 );
    STEP( 6 );
    NEXTI;
    STEP( 0 );
    STEP( 1 );
    STEP( 2 );
    STEP( 3 );
    STEP( 4 );
    STEP( 5 );
    STEP( 6 );
    STEP( 7 );

    _Pragma( "loopbound min 152 max 152" )
    for ( i = 8; i <= 159; i++ ) {

      NEXTI;

      STEP( 0 );
      STEP( 1 );
      STEP( 2 );
      STEP( 3 );
      STEP( 4 );
      STEP( 5 );
      STEP( 6 );
      STEP( 7 );
      STEP( 8 );
    }

    _Pragma( "loopbound min 9 max 9" )
    for ( k = 9; k--; L_ACF[ k ] <<= 1 ) ;

  }
  /*   Rescaling of the array s[ 0..159 ]
  */
  if ( scalauto > 0 ) {
    _Pragma( "loopbound min 160 max 160" )
    for ( k = 160; k--; *s++ <<= scalauto ) ;
  }
}

/* 4.2.5 */

void gsm_enc_Reflection_coefficients (
  longword   *L_ACF,    /* 0...8  IN  */
  word   *r     /* 0...7  OUT   */
)
{
  int i, m, n;
  word  temp;
  longword ltmp;
  word    ACF[ 9 ]; /* 0..8 */
  word    P[   9 ]; /* 0..8 */
  word    K[   9 ]; /* 2..8 */

  /*  Schur recursion with 16 bits arithmetic.
  */

  if ( L_ACF[ 0 ] == 0 ) {
    _Pragma( "loopbound min 8 max 8" )
    for ( i = 8; i--; *r++ = 0 ) ;
    return;
  }

  temp = gsm_enc_norm( L_ACF[ 0 ] );

  /* ? overflow ? */
  _Pragma( "loopbound min 9 max 9" )
  for ( i = 0; i <= 8; i++ ) ACF[ i ] = SASR( L_ACF[ i ] << temp, 16 );

  /*   Initialize array P[ .. ] and K[ .. ] for the recursion.
  */

  _Pragma( "loopbound min 7 max 7" )
  for ( i = 1; i <= 7; i++ ) K[  i  ] = ACF[  i  ];

  _Pragma( "loopbound min 9 max 9" )
  for ( i = 0; i <= 8; i++ ) P[  i  ] = ACF[  i  ];

  /*   Compute reflection coefficients
  */
  _Pragma( "loopbound min 8 max 8" )
  _Pragma( "marker outer-marker" )
  for ( n = 1; n <= 8; n++, r++ ) {

    temp = P[ 1 ];
    temp = GSM_ABS( temp );
    if ( P[ 0 ] < temp ) {
                
      for ( i = n; i <= 8; i++ ) *r++ = 0; _Pragma( "marker inner-marker" ) 
      return; _Pragma( "flowrestriction 1*inner-marker <= 36*outer-marker" )
    }

    *r = gsm_enc_div( temp, P[ 0 ] );

    if ( P[ 1 ] > 0 ) *r = -*r; /* r[ n ] = sub(0, r[ n ]) */
    if ( n == 8 ) return;

    /*  Schur recursion
    */
    temp = GSM_MULT_R( P[ 1 ], *r );
    P[ 0 ] = GSM_ADD( P[ 0 ], temp );

    _Pragma( "loopbound min 1 max 7" )
    for ( m = 1; m <= 8 - n; ++m ) {
      temp     = GSM_MULT_R( K[  m    ],    *r );
      P[ m ]     = GSM_ADD(    P[  m + 1  ],  temp );

      temp     = GSM_MULT_R( P[  m + 1  ],    *r );
      K[ m ]     = GSM_ADD(    K[  m    ],  temp );
    }
  }
}

/* 4.2.6 */

void gsm_enc_Transformation_to_Log_Area_Ratios (
  word   *r       /* 0..7    IN/OUT */
)
/*
    The following scaling for r[ .. ] and LAR[ .. ] has been used:

    r[ .. ]   = integer( real_r[ .. ]*32768. ); -1 <= real_r < 1.
    LAR[ .. ] = integer( real_LAR[ .. ] * 16384 );
    with -1.625 <= real_LAR <= 1.625
*/
{
  word  temp;
  int i;


  /* Computation of the LAR[ 0..7 ] from the r[ 0..7 ]
  */
  _Pragma( "loopbound min 8 max 8" )
  for ( i = 1; i <= 8; i++, r++ ) {

    temp = *r;
    temp = GSM_ABS( temp );

    if ( temp < 22118 )
      temp >>= 1;

    else
      if ( temp < 31130 )
        temp -= 11059;

      else {
        temp -= 26112;
        temp <<= 2;
      }

    *r = *r < 0 ? -temp : temp;
  }
}

/* 4.2.7 */

void gsm_enc_Quantization_and_coding (
  word *LAR       /* [ 0..7 ] IN/OUT  */
)
{
  word  temp;
  longword  ltmp;


  /*  This procedure needs four tables; the following equations
      give the optimum scaling for the constants:

      A[ 0..7 ] = integer( real_A[ 0..7 ] * 1024 )
      B[ 0..7 ] = integer( real_B[ 0..7 ] *  512 )
      MAC[ 0..7 ] = maximum of the LARc[ 0..7 ]
      MIC[ 0..7 ] = minimum of the LARc[ 0..7 ]
  */

# undef STEP
# define  STEP( A, B, MAC, MIC )    \
  temp = GSM_MULT( A,   *LAR ); \
  temp = GSM_ADD(  temp,   B ); \
  temp = GSM_ADD(  temp, 256 ); \
  temp = SASR(     temp,   9 ); \
  *LAR  =  temp>MAC ? MAC - MIC : (temp<MIC ? 0 : temp - MIC); \
  LAR++;

  STEP(  20480,     0,  31, -32 );
  STEP(  20480,     0,  31, -32 );
  STEP(  20480,  2048,  15, -16 );
  STEP(  20480, -2560,  15, -16 );

  STEP(  13964,    94,   7,  -8 );
  STEP(  15360, -1792,   7,  -8 );
  STEP(   8534,  -341,   3,  -4 );
  STEP(   9036, -1144,   3,  -4 );

# undef STEP
}

void gsm_enc_Gsm_LPC_Analysis (
  word      *s,   /* 0..159 signals IN/OUT  */
  word      *LARc ) /* 0..7   LARc's  OUT */
{
  longword  L_ACF[ 9 ];

  gsm_enc_Autocorrelation       ( s,   L_ACF );
  gsm_enc_Reflection_coefficients     ( L_ACF, LARc  );
  gsm_enc_Transformation_to_Log_Area_Ratios ( LARc );
  gsm_enc_Quantization_and_coding     ( LARc );
}

/* preprocess.c */
/*  4.2.0 .. 4.2.3  PREPROCESSING SECTION

      After A-law to linear conversion (or directly from the
      Ato D converter) the following scaling is assumed for
    input to the RPE-LTP algorithm:

        in:  0.1.....................12
         S.v.v.v.v.v.v.v.v.v.v.v.v.*.*.

    Where S is the sign bit, v a valid bit, and * a "don't care" bit.
    The original signal is called sop[ .. ]

        out:   0.1................... 12
         S.S.v.v.v.v.v.v.v.v.v.v.v.v.0.0
*/


void gsm_enc_Gsm_Preprocess (
  struct gsm_state *S,
  word      *s,
  word      *so )   /* [ 0..159 ]   IN/OUT  */
{

  word       z1 = S->z1;
  longword L_z2 = S->L_z2;
  word     mp = S->mp;

  word      s1;
  longword      L_s2;

  longword      L_temp;

  word    msp, lsp;
  word    SO;

  longword  ltmp;   /* for   ADD */
  ulongword utmp;   /* for L_ADD */

  int   k = 160;

  _Pragma( "loopbound min 160 max 160" )
  while ( k-- ) {

    /*  4.2.1   Downscaling of the input signal
    */
    SO = SASR( *s, 3 ) << 2;
    s++;

    /*  4.2.2   Offset compensation

        This part implements a high-pass filter and requires extended
        arithmetic precision for the recursive part of this filter.
        The input of this procedure is the array so[ 0...159 ] and the
        output the array sof[  0...159  ].
    */
    /*   Compute the non-recursive part
    */

    s1 = SO - z1;     /* s1 = gsm_enc_sub( *so, z1 ); */
    z1 = SO;

    /*   Compute the recursive part
    */
    L_s2 = s1;
    L_s2 <<= 15;

    /*   Execution of a 31 bv 16 bits multiplication
    */

    msp = SASR( L_z2, 15 );
    lsp = L_z2 - ( ( longword )msp << 15 ); /* gsm_enc_L_sub(L_z2,(msp<<15)); */

    L_s2  += GSM_MULT_R( lsp, 32735 );
    L_temp = ( longword )msp * 32735; /* GSM_L_MULT(msp,32735) >> 1;*/
    L_z2   = GSM_L_ADD( L_temp, L_s2 );

    /*    Compute sof[ k ] with rounding
    */
    L_temp = GSM_L_ADD( L_z2, 16384 );

    /*   4.2.3  Preemphasis
    */

    msp   = GSM_MULT_R( mp, -28180 );
    mp    = SASR( L_temp, 15 );
    *so++ = GSM_ADD( mp, msp );
  }

  S->z1   = z1;
  S->L_z2 = L_z2;
  S->mp   = mp;
}

/* gsm_enc_bench.c */

word gsm_enc_norm ( longword a )
/*
   the number of left shifts needed to normalize the 32 bit
   variable L_var1 for positive values on the interval

   with minimum of
   minimum of 1073741824  (01000000000000000000000000000000) and
   maximum of 2147483647  (01111111111111111111111111111111)


   and for negative values on the interval with
   minimum of -2147483648 (-10000000000000000000000000000000) and
   maximum of -1073741824 ( -1000000000000000000000000000000).

   in order to normalize the result, the following
   operation must be done: L_norm_var1 = L_var1 << norm( L_var1 );

   (That's 'ffs', only from the left, not the right..)
*/
{
  if ( a < 0 ) {
    if ( a <= -1073741824 ) return 0;
    a = ~a;
  }

  return    a & 0xffff0000
            ? ( a & 0xff000000
                ?  -1 + gsm_enc_bitoff[  0xFF & ( a >> 24 )  ]
                :   7 + gsm_enc_bitoff[  0xFF & ( a >> 16 )  ] )
            : ( a & 0xff00
                ?  15 + gsm_enc_bitoff[  0xFF & ( a >> 8 )  ]
                :  23 + gsm_enc_bitoff[  0xFF & a  ] );
}

word gsm_enc_asl ( word a, int n )
{
  if ( n >= 16 ) return 0;
  if ( n <= -16 ) return -( a < 0 );
  if ( n < 0 ) return gsm_enc_asr( a, -n );
  return a << n;
}

word gsm_enc_asr ( word a, int n )
{
  if ( n >= 16 ) return -( a < 0 );
  if ( n <= -16 ) return 0;
  if ( n < 0 ) return a << -n;

  # ifdef SASR
  return a >> n;
  # else
  if ( a >= 0 ) return a >> n;
  else return -( word )( -( uword )a >> n );
  # endif
}

/*
    (From p. 46, end of section 4.2.5)

    NOTE: The following lines gives [ sic ] one correct implementation
        of the div(num, denum) arithmetic operation.  Compute div
          which is the integer division of num by denum: with denum
      >= num > 0
*/

word gsm_enc_div ( word num, word denum )
{
  longword  L_num   = num;
  longword  L_denum = denum;
  word    div   = 0;
  int   k   = 15;

  /* The parameter num sometimes becomes zero.
     Although this is explicitly guarded against in 4.2.5,
     we assume that the result should then be zero as well.
  */

  if ( num == 0 )
    return 0;

  _Pragma( "loopbound min 15 max 15" )
  while ( k-- ) {
    div   <<= 1;
    L_num <<= 1;

    if ( L_num >= L_denum ) {
      L_num -= L_denum;
      div++;
    }
  }

  return div;
}



gsm gsm_enc_create( void )
{
  unsigned int i;
  gsm r;

  r = &gsm_enc_state;

  _Pragma( "loopbound min 648 max 648" )
  for ( i = 0; i < sizeof( *r ); i++ )
    ( ( char * )r )[ i ] = 0;

  r->nrp = 40;

  return r;
}

void gsm_enc_init( void )
{
  gsm_enc_state_ptr = gsm_enc_create();
}

int gsm_enc_return( void )
{
  return gsm_enc_result;
}

void _Pragma( "entrypoint" ) gsm_enc_main( void )
{
  gsm r;
  unsigned i;
  gsm_enc_result = 0;

  r = gsm_enc_state_ptr;

  _Pragma( "loopbound min 20 max 20" )
  for ( i = 0; i < SAMPLES; i++ )
    gsm_enc_encode( r, gsm_enc_pcmdata + i * 160,
                    gsm_enc_gsmdata + i * sizeof( gsm_frame ) );
}

int run_gsm_enc_bench( void )
{
  gsm_enc_init();
  gsm_enc_main();
  return ( gsm_enc_return() );
}
