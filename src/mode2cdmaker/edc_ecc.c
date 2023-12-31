/* reed-solomon encoder / decoder for compact disc building project
 * Copyright 1998 by Heiko Eissfeldt
 * portions used& Chris Smith
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ecc.h"
#include "ecc_tables.h"

/* these prototypes will become public when the function are implemented */
static int do_decode_L2(unsigned char in[(L2_RAW+L2_Q+L2_P)],
		unsigned char out[L2_RAW]);

static int do_decode_L1(unsigned char in[(L1_RAW+L1_Q+L1_P)*FRAMES_PER_SECTOR],
		unsigned char out[L1_RAW*FRAMES_PER_SECTOR],
		int delay1, int delay2, int delay3, int scramble);

/* ---------------------------*/

static int encode_L1_Q(unsigned char inout[L1_RAW + L1_Q])
{
  unsigned char *Q;
  int i;

  memmove(inout+L1_RAW/2+L1_Q, inout+L1_RAW/2, L1_RAW/2);
  Q = inout + L1_RAW/2;

  memset(Q, 0, L1_Q);
  for (i = 0; i < L1_RAW + L1_Q; i++) {
	unsigned char data;

	if (i == L1_RAW/2) i += L1_Q;
        data = inout[i];
	if (data != 0) {
		unsigned char base = rs_l12_log[data];

		Q[0] ^= rs_l12_alog[(base+AQ[0][i]) % ((1 << RS_L12_BITS)-1)];
		Q[1] ^= rs_l12_alog[(base+AQ[1][i]) % ((1 << RS_L12_BITS)-1)];
		Q[2] ^= rs_l12_alog[(base+AQ[2][i]) % ((1 << RS_L12_BITS)-1)];
		Q[3] ^= rs_l12_alog[(base+AQ[3][i]) % ((1 << RS_L12_BITS)-1)];
	}
  }

  return 0;
}
static int encode_L1_P(unsigned char inout[L1_RAW + L1_Q + L1_P])
{
  unsigned char *P;
  int i;

  P = inout + L1_RAW + L1_Q;

  memset(P, 0, L1_P);
  for (i = 0; i < L2_RAW + L2_Q + L2_P; i++) {
	unsigned char data;

        data = inout[i];
	if (data != 0) {
		unsigned char base = rs_l12_log[data];

		P[0] ^= rs_l12_alog[(base+AP[0][i]) % ((1 << RS_L12_BITS)-1)];
		P[1] ^= rs_l12_alog[(base+AP[1][i]) % ((1 << RS_L12_BITS)-1)];
		P[2] ^= rs_l12_alog[(base+AP[2][i]) % ((1 << RS_L12_BITS)-1)];
		P[3] ^= rs_l12_alog[(base+AP[3][i]) % ((1 << RS_L12_BITS)-1)];
	}
  }
  return 0;
}

static int decode_L1_Q(unsigned char inout[L1_RAW + L1_Q])
{
  return 0;
}
static int decode_L1_P(unsigned char in[L1_RAW + L1_Q + L1_P])
{
  return 0;
}

static int encode_L2_Q(unsigned char inout[4 + L2_RAW + 4 + 8 + L2_P + L2_Q])
{
  unsigned char *Q;
  int i,j;

  Q = inout + 4 + L2_RAW + 4 + 8 + L2_P;
  memset(Q, 0, L2_Q);
  for (j = 0; j < 26; j++) {
     for (i = 0; i < 43; i++) {
	unsigned char data;

        /* LSB */
        data = inout[(j*43*2+i*2*44) % (4 + L2_RAW + 4 + 8 + L2_P)];
	if (data != 0) {
		unsigned int base = rs_l12_log[data];

		unsigned int sum = base + DQ[0][i];
		if (sum >= ((1 << RS_L12_BITS)-1))
		  sum -= (1 << RS_L12_BITS)-1;
		
		Q[0]    ^= rs_l12_alog[sum];

		sum = base + DQ[1][i];
		if (sum >= ((1 << RS_L12_BITS)-1))
		  sum -= (1 << RS_L12_BITS)-1;
		
		Q[26*2] ^= rs_l12_alog[sum];
	}
        /* MSB */
        data = inout[(j*43*2+i*2*44+1) % (4 + L2_RAW + 4 + 8 + L2_P)];
	if (data != 0) {
		unsigned int base = rs_l12_log[data];

		unsigned int sum = base+DQ[0][i];
		if (sum >= ((1 << RS_L12_BITS)-1))
		  sum -= (1 << RS_L12_BITS)-1;
		
		Q[1]      ^= rs_l12_alog[sum];

		sum = base + DQ[1][i];
		if (sum >= ((1 << RS_L12_BITS)-1))
		  sum -= (1 << RS_L12_BITS)-1;
		
		Q[26*2+1] ^= rs_l12_alog[sum];
	}
     }
     Q += 2;
  }
  return 0;
}

static int encode_L2_P(unsigned char inout[4 + L2_RAW + 4 + 8 + L2_P])
{
  unsigned char *P;
  int i,j;

  P = inout + 4 + L2_RAW + 4 + 8;
  memset(P, 0, L2_P);
  for (j = 0; j < 43; j++) {
     for (i = 0; i < 24; i++) {
	unsigned char data;

        /* LSB */
        data = inout[i*2*43];
	if (data != 0) {
		unsigned int base = rs_l12_log[data];
		
		unsigned int sum = base + DP[0][i];
		if (sum >= ((1 << RS_L12_BITS)-1))
		  sum -= (1 << RS_L12_BITS)-1;

		P[0]    ^= rs_l12_alog[sum];

		sum = base + DP[1][i];
		if (sum >= ((1 << RS_L12_BITS)-1))
		  sum -= (1 << RS_L12_BITS)-1;
		
		P[43*2] ^= rs_l12_alog[sum];
	}
        /* MSB */
        data = inout[i*2*43+1];
	if (data != 0) {
		unsigned int base = rs_l12_log[data];

		unsigned int sum = base + DP[0][i];
		if (sum >= ((1 << RS_L12_BITS)-1))
		  sum -= (1 << RS_L12_BITS)-1;
		
		P[1]      ^= rs_l12_alog[sum];

		sum = base + DP[1][i];
		if (sum >= ((1 << RS_L12_BITS)-1))
		  sum -= (1 << RS_L12_BITS)-1;
		
		P[43*2+1] ^= rs_l12_alog[sum];
	}
     }
     P += 2;
     inout += 2;
  }
  return 0;
}

int decode_L2_Q(unsigned char inout[4 + L2_RAW + 12 + L2_Q])
{
  return 0;
}
int decode_L2_P(unsigned char inout[4 + L2_RAW + 12 + L2_Q + L2_P])
{
  return 0;
}

int scramble_L2(unsigned char *inout)
{
  unsigned char *r = inout + 12;
  unsigned char *s = yellowbook_scrambler;
  unsigned int i;
  unsigned int *f = (unsigned int *)inout;

  for (i = (L2_RAW + L2_Q + L2_P +16)/sizeof(unsigned char); i; i--) {
    *r++ ^= *s++;
  }

  /* generate F1 frames */
  for (i = (2352/sizeof(unsigned int)); i; i--) {
    *f++ = ((*f & 0xff00ff00UL) >> 8) | ((*f & 0x00ff00ffUL) << 8);
  }

  return 0;
}

static int encode_LSUB_Q(unsigned char inout[LSUB_RAW + LSUB_Q])
{
  unsigned char *Q;
  unsigned char data;
  int i;

  memmove(inout+LSUB_QRAW+LSUB_Q, inout+LSUB_QRAW, LSUB_RAW-LSUB_QRAW);
  Q = inout + LSUB_QRAW;

  memset(Q, 0, LSUB_Q);

#if 0
  data = inout[0] & 0x3f;
  if (data != 0) {
	unsigned char base = rs_sub_rw_log[data];

	Q[0] ^= rs_sub_rw_alog[(base+26) % ((1 << RS_SUB_RW_BITS)-1)];
	Q[1] ^= rs_sub_rw_alog[(base+7) % ((1 << RS_SUB_RW_BITS)-1)];
  }
  data = inout[1] & 0x3f;
  if (data != 0) {
	unsigned char base = rs_sub_rw_log[data];

	Q[0] ^= rs_sub_rw_alog[(base+6) % ((1 << RS_SUB_RW_BITS)-1)];
	Q[1] ^= rs_sub_rw_alog[(base+1) % ((1 << RS_SUB_RW_BITS)-1)];
  }
#else
  for (i = 0; i < LSUB_QRAW; i++) {
	unsigned char data;

	data = inout[i] & 0x3f;
	if (data != 0) {
		unsigned char base = rs_sub_rw_log[data];

		Q[0] ^= rs_sub_rw_alog[(base+SQ[0][i]) % ((1 << RS_SUB_RW_BITS)-1)];
		Q[1] ^= rs_sub_rw_alog[(base+SQ[1][i]) % ((1 << RS_SUB_RW_BITS)-1)];
	}
  }
#endif
  return 0;
}


static int encode_LSUB_P(unsigned char inout[LSUB_RAW + LSUB_Q + LSUB_P])
{
  unsigned char *P;
  int i;

  P = inout + LSUB_RAW + LSUB_Q;

  memset(P, 0, LSUB_P);
  for (i = 0; i < LSUB_RAW + LSUB_Q; i++) {
	unsigned char data;

	data = inout[i] & 0x3f;
	if (data != 0) {
		unsigned char base = rs_sub_rw_log[data];

		P[0] ^= rs_sub_rw_alog[(base+SP[0][i]) % ((1 << RS_SUB_RW_BITS)-1)];
		P[1] ^= rs_sub_rw_alog[(base+SP[1][i]) % ((1 << RS_SUB_RW_BITS)-1)];
		P[2] ^= rs_sub_rw_alog[(base+SP[2][i]) % ((1 << RS_SUB_RW_BITS)-1)];
		P[3] ^= rs_sub_rw_alog[(base+SP[3][i]) % ((1 << RS_SUB_RW_BITS)-1)];
	}
  }
  return 0;
}

int decode_LSUB_Q(unsigned char inout[LSUB_QRAW + LSUB_Q])
{
  unsigned char Q[LSUB_Q];
  int i;

  memset(Q, 0, LSUB_Q);
  for (i = LSUB_QRAW + LSUB_Q -1; i>=0; i--) {
	unsigned char data;

	data = inout[LSUB_QRAW + LSUB_Q -1 -i] & 0x3f;
	if (data != 0) {
		unsigned char base = rs_sub_rw_log[data];

		Q[0] ^= rs_sub_rw_alog[(base+0*i) % ((1 << RS_SUB_RW_BITS)-1)];
		Q[1] ^= rs_sub_rw_alog[(base+1*i) % ((1 << RS_SUB_RW_BITS)-1)];
	}
  }

  return (Q[0] != 0 || Q[1] != 0);
}
int decode_LSUB_P(unsigned char inout[LSUB_RAW + LSUB_Q + LSUB_P])
{
  unsigned char P[LSUB_P];
  int i;

  memset(P, 0, LSUB_P);
  for (i = LSUB_RAW + LSUB_Q + LSUB_P-1; i>=0; i--) {
	unsigned char data;

	data = inout[LSUB_RAW + LSUB_Q + LSUB_P -1 -i] & 0x3f;
	if (data != 0) {
		unsigned char base = rs_sub_rw_log[data];

		P[0] ^= rs_sub_rw_alog[(base+0*i) % ((1 << RS_SUB_RW_BITS)-1)];
		P[1] ^= rs_sub_rw_alog[(base+1*i) % ((1 << RS_SUB_RW_BITS)-1)];
		P[2] ^= rs_sub_rw_alog[(base+2*i) % ((1 << RS_SUB_RW_BITS)-1)];
		P[3] ^= rs_sub_rw_alog[(base+3*i) % ((1 << RS_SUB_RW_BITS)-1)];
	}
  }

  return (P[0] != 0 || P[1] != 0 || P[2] != 0 || P[3] != 0);
}

/* Layer 1 CIRC en/decoder */
#define MAX_L1_DEL1 2
static unsigned char l1_delay_line1[MAX_L1_DEL1][L1_RAW];
#define MAX_L1_DEL2 108
static unsigned char l1_delay_line2[MAX_L1_DEL2][L1_RAW+L1_Q];
#define MAX_L1_DEL3 1
static unsigned char l1_delay_line3[MAX_L1_DEL3][L1_RAW+L1_Q+L1_P];
static unsigned l1_del_index;

int do_encode_L1(unsigned char in[L1_RAW*FRAMES_PER_SECTOR],
		unsigned char out[(L1_RAW+L1_Q+L1_P)*FRAMES_PER_SECTOR],
		int delay1, int delay2, int delay3, int permute)
{
  int i;

  for (i = 0; i < FRAMES_PER_SECTOR; i++) {
     int j;
     unsigned char t;
	
     if (in != out)
	memcpy(out, in, L1_RAW);

     if (delay1) {
	/* shift through delay line 1 */
 	for (j = 0; j < L1_RAW; j++) {
		if (((j/4) % MAX_L1_DEL1) == 0) {
			t = l1_delay_line1[l1_del_index % (MAX_L1_DEL1)][j];
			l1_delay_line1[l1_del_index % (MAX_L1_DEL1)][j] = out[j];
			out[j] = t;
		}
	}
     }

     if (permute) {
	/* permute */
	t = out[2]; out[2] = out[8]; out[8] = out[10]; out[10] = out[18];
        out[18] = out[6]; out [6] = t;
	t = out[3]; out[3] = out[9]; out[9] = out[11]; out[11] = out[19];
        out[19] = out[7]; out [7] = t;
	t = out[4]; out[4] = out[16]; out[16] = out[20]; out[20] = out[14];
        out[14] = out[12]; out [12] = t;
	t = out[5]; out[5] = out[17]; out[17] = out[21]; out[21] = out[15];
        out[15] = out[13]; out [13] = t;
     }

     /* build Q parity */
     encode_L1_Q(out);

     if (delay2) {
	/* shift through delay line 2 */
 	for (j = 0; j < L1_RAW+L1_Q; j++) {
		if (j != 0) {
			t = l1_delay_line2[(l1_del_index) % MAX_L1_DEL2][j];
			l1_delay_line2[(l1_del_index + j*4) % MAX_L1_DEL2][j] = out[j];
			out[j] = t;
		}
	}
     }

     /* build P parity */
     encode_L1_P(out);

     if (delay3) {
	/* shift through delay line 3 */
 	for (j = 0; j < L1_RAW+L1_Q+L1_P; j++) {
		if (((j) & MAX_L1_DEL3) == 0) {
			t = l1_delay_line3[0][j];
			l1_delay_line3[0][j] = out[j];
			out[j] = t;
		}
	}
     }

     /* invert Q and P parity */
     for (j = 0; j < L1_Q; j++)
	out[j+12] = ~out[j+12];
     for (j = 0; j < L1_P; j++)
	out[j+28] = ~out[j+28];

     l1_del_index++;
     out += L1_RAW+L1_Q+L1_P;
     in += L1_RAW;
  }
  return 0;
}

int do_decode_L1(unsigned char in[(L1_RAW+L1_Q+L1_P)*FRAMES_PER_SECTOR],
		unsigned char out[L1_RAW*FRAMES_PER_SECTOR],
		int delay1, int delay2, int delay3, int permute)
{
  int i;

  for (i = 0; i < FRAMES_PER_SECTOR; i++) {
     int j;
     unsigned char t;
	
     if (delay3) {
	/* shift through delay line 3 */
 	for (j = 0; j < L1_RAW+L1_Q+L1_P; j++) {
		if (((j) & MAX_L1_DEL3) != 0) {
			t = l1_delay_line3[0][j];
			l1_delay_line3[0][j] = in[j];
			in[j] = t;
		}
	}
     }

     /* invert Q and P parity */
     for (j = 0; j < L1_Q; j++)
	in[j+12] = ~in[j+12];
     for (j = 0; j < L1_P; j++)
	in[j+28] = ~in[j+28];

     /* build P parity */
     decode_L1_P(in);

     if (delay2) {
	/* shift through delay line 2 */
 	for (j = 0; j < L1_RAW+L1_Q; j++) {
		if (j != L1_RAW+L1_Q-1) {
			t = l1_delay_line2[(l1_del_index) % MAX_L1_DEL2][j];
			l1_delay_line2[(l1_del_index + (MAX_L1_DEL2 - j*4)) % MAX_L1_DEL2][j] = in[j];
			in[j] = t;
		}
	}
     }

     /* build Q parity */
     decode_L1_Q(in);

     if (permute) {
	/* permute */
	t = in[2]; in[2] = in[6]; in[6] = in[18]; in[18] = in[10];
        in[10] = in[8]; in [8] = t;
	t = in[3]; in[3] = in[7]; in[7] = in[19]; in[19] = in[11];
        in[11] = in[9]; in [9] = t;
	t = in[4]; in[4] = in[12]; in[12] = in[14]; in[14] = in[20];
        in[20] = in[16]; in [16] = t;
	t = in[5]; in[5] = in[13]; in[13] = in[15]; in[15] = in[21];
        in[21] = in[17]; in [17] = t;
     }

     if (delay1) {
	/* shift through delay line 1 */
 	for (j = 0; j < L1_RAW; j++) {
		if (((j/4) % MAX_L1_DEL1) != 0) {
			t = l1_delay_line1[l1_del_index % (MAX_L1_DEL1)][j];
			l1_delay_line1[l1_del_index % (MAX_L1_DEL1)][j] = in[j];
			in[j] = t;
		}
	}
     }

     if (in != out)
	memcpy(out, in, (L1_RAW));

     l1_del_index++;
     in += L1_RAW+L1_Q+L1_P;
     out += L1_RAW;
  }
  return 0;
}

static unsigned char bin2bcd(unsigned p)
{
  return ((p/10)<<4)|(p%10);
}

static int build_address(unsigned char inout[], int sectortype, unsigned address)
{
  inout[12] = bin2bcd(address / (60*75));
  inout[13] = bin2bcd((address / 75) % 60);
  inout[14] = bin2bcd(address % 75);
  if (sectortype == MODE_0)
	inout[15] = 0;
  else if (sectortype == MODE_1)
	inout[15] = 1;
  else if (sectortype == MODE_2)
	inout[15] = 2;
  else if (sectortype == MODE_2_FORM_1)
	inout[15] = 2;
  else if (sectortype == MODE_2_FORM_2)
	inout[15] = 2;
  else
	return -1;
  return 0;
}

unsigned int build_edc(unsigned char inout[], int from, int upto)
{
  unsigned char *p = inout+from;
  unsigned int result = 0;

  for (; from <= upto; from++)
    result = EDC_crctable[(result ^ *p++) & 0xffL] ^ (result >> 8);

  return result;
}

/* Layer 2 Product code en/decoder */
int do_encode_L2(unsigned char inout[(12 + 4 + L2_RAW+4+8+L2_Q+L2_P)], int sectortype, unsigned address)
{
  unsigned int result;

#define SYNCPATTERN "\x00\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"

  /* supply initial sync pattern */
  memcpy(inout, SYNCPATTERN, sizeof(SYNCPATTERN));

  if (sectortype == MODE_0) {
	memset(inout + sizeof(SYNCPATTERN), 0, 4 + L2_RAW + 12 + L2_P + L2_Q);
        build_address(inout, sectortype, address);
	return 0;
  }
  
  switch (sectortype) {
    case MODE_1:
        build_address(inout, sectortype, address);
	result = build_edc(inout, 0, 16+2048-1);
	inout[2064+0] = result >> 0L;
	inout[2064+1] = result >> 8L;
	inout[2064+2] = result >> 16L;
	inout[2064+3] = result >> 24L;
	memset(inout+2064+4, 0, 8);
  	encode_L2_P(inout+12);
 	encode_L2_Q(inout+12);
	break;
    case MODE_2:
        build_address(inout, sectortype, address);
	break;
    case MODE_2_FORM_1:
        result = build_edc(inout, 16, 16+8+2048-1);
	inout[2072+0] = result >> 0L;
	inout[2072+1] = result >> 8L;
	inout[2072+2] = result >> 16L;
	inout[2072+3] = result >> 24L;

	/* clear header for P/Q parity calculation */
	inout[12] = 0;
	inout[12+1] = 0;
	inout[12+2] = 0;
	inout[12+3] = 0;
  	encode_L2_P(inout+12);
 	encode_L2_Q(inout+12);
	build_address(inout, sectortype, address);
	break;
    case MODE_2_FORM_2:
        build_address(inout, sectortype, address);
	result = build_edc(inout, 16, 16+8+2324-1);
	inout[2348+0] = result >> 0L;
	inout[2348+1] = result >> 8L;
	inout[2348+2] = result >> 16L;
	inout[2348+3] = result >> 24L;
	break;
    default:
	return -1;
  }

  return 0;
}

static int do_decode_L2(unsigned char in[(L2_RAW+L2_Q+L2_P)],
		unsigned char out[L2_RAW])
{
  return 0;
}



#define MAX_SUB_DEL 8
static unsigned char sub_delay_line[MAX_SUB_DEL][LSUB_RAW+LSUB_Q+LSUB_P];
static unsigned sub_del_index;

/* R-W Subchannel en/decoder */
int do_encode_sub(unsigned char in[LSUB_RAW*PACKETS_PER_SUBCHANNELFRAME],
		unsigned char out[(LSUB_RAW+LSUB_Q+LSUB_P)*PACKETS_PER_SUBCHANNELFRAME],
		int delay1, int permute)
{
  int i;

  if (in == out) return -1;

  for (i = 0; i < PACKETS_PER_SUBCHANNELFRAME; i++) {
     int j;
     unsigned char t;

     memcpy(out, in, (LSUB_RAW));

     /* build Q parity */
     encode_LSUB_Q(out);

     /* build P parity */
     encode_LSUB_P(out);

     if (permute) {
	/* permute */
	t = out[1]; out[1] = out[18]; out[18] = t;
	t = out[2]; out[2] = out[ 5]; out[ 5] = t;
	t = out[3]; out[3] = out[23]; out[23] = t;
     }

     if (delay1) {
	/* shift through delay_line */
 	for (j = 0; j < LSUB_RAW+LSUB_Q+LSUB_P; j++) {
		if ((j % MAX_SUB_DEL) != 0) {
			t = sub_delay_line[(sub_del_index) % MAX_SUB_DEL][j];
			sub_delay_line[(sub_del_index + j) % MAX_SUB_DEL][j] = out[j];
			out[j] = t;
		}
	}
     }
     sub_del_index++;
     out += LSUB_RAW+LSUB_Q+LSUB_P;
     in += LSUB_RAW;
  }
  return 0;
}

int 
do_decode_sub(
     unsigned char in[(LSUB_RAW+LSUB_Q+LSUB_P)*PACKETS_PER_SUBCHANNELFRAME],
     unsigned char out[LSUB_RAW*PACKETS_PER_SUBCHANNELFRAME],
     int delay1, int permute)
{
  int i;

  if (in == out) return -1;

  for (i = 0; i < PACKETS_PER_SUBCHANNELFRAME; i++) {
     int j;
     unsigned char t;

     if (delay1) {
	/* shift through delay_line */
 	for (j = 0; j < LSUB_RAW+LSUB_Q+LSUB_P; j++) {
		if ((j % MAX_SUB_DEL) != MAX_SUB_DEL-1) {
			t = sub_delay_line[(sub_del_index) % MAX_SUB_DEL][j];
			sub_delay_line[(sub_del_index + (MAX_SUB_DEL - j)) % MAX_SUB_DEL][j] = in[j];
			in[j] = t;
		}
	}
     }

     if (permute) {
	/* permute */
	t = in[1]; in[1] = in[18]; in[18] = t;
	t = in[2]; in[2] = in[ 5]; in[ 5] = t;
	t = in[3]; in[3] = in[23]; in[23] = t;
     }

     /* build P parity */
     decode_LSUB_P(in);

     /* build Q parity */
     decode_LSUB_Q(in);

     memcpy(out, in, LSUB_QRAW);
     memcpy(out+LSUB_QRAW, in+LSUB_QRAW+LSUB_Q, LSUB_RAW-LSUB_QRAW);

     sub_del_index++;
     in += LSUB_RAW+LSUB_Q+LSUB_P;
     out += LSUB_RAW;
  }
  return 0;
}

static int sectortype = MODE_0;
int get_sector_type(void)
{
  return sectortype;
}

int set_sector_type(int st)
{
  switch(st) {
    case MODE_0:
    case MODE_1:
    case MODE_2:
    case MODE_2_FORM_1:
    case MODE_2_FORM_2:
      sectortype = st;
    default:
      return -1;
  }
  return 0;
}

/* ---------------------------*/
#ifdef MAIN

#define DO_L1 1
#define DO_L2 2
#define DO_SUB 4

static const unsigned sect_size[8][2] = {
/* nothing */
{0,0},
/* Layer 1 decode/encode */
{ (L1_RAW+L1_Q+L1_P)*FRAMES_PER_SECTOR, L1_RAW*FRAMES_PER_SECTOR},
/* Layer 2 decode/encode */
{ 16+L2_RAW+12+L2_Q+L2_P, L2_RAW},
/* Layer 1 and 2 decode/encode */
{ (L1_RAW+L1_Q+L1_P)*FRAMES_PER_SECTOR, L1_RAW*FRAMES_PER_SECTOR},
/* Subchannel decode/encode */
{ (LSUB_RAW+LSUB_Q+LSUB_P)*PACKETS_PER_SUBCHANNELFRAME,
 LSUB_RAW*PACKETS_PER_SUBCHANNELFRAME},
/* Layer 1 and subchannel decode/encode */
{ (L1_RAW+L1_Q+L1_P)*FRAMES_PER_SECTOR +
   (LSUB_RAW+LSUB_Q+LSUB_P)*PACKETS_PER_SUBCHANNELFRAME,
  LSUB_RAW*PACKETS_PER_SUBCHANNELFRAME +
   L1_RAW*FRAMES_PER_SECTOR},
/* Layer 2 and subchannel decode/encode */
{ L2_RAW+L2_Q+L2_P+
   (LSUB_RAW+LSUB_Q+LSUB_P)*PACKETS_PER_SUBCHANNELFRAME,
  LSUB_RAW*PACKETS_PER_SUBCHANNELFRAME +
   L2_RAW},
/* Layer 1, 2 and subchannel decode/encode */
{ (L1_RAW+L1_Q+L1_P)*FRAMES_PER_SECTOR +
   (LSUB_RAW+LSUB_Q+LSUB_P)*PACKETS_PER_SUBCHANNELFRAME,
  LSUB_RAW*PACKETS_PER_SUBCHANNELFRAME +
   L1_RAW*FRAMES_PER_SECTOR},
};

int main(int argc, char **argv)
{
  int encode = 1;
  int mask = DO_L2;
  FILE *infp;
  FILE *outfp;
  unsigned address = 0;
  unsigned char *l1_inbuf;
  unsigned char *l1_outbuf;
  unsigned char *l2_inbuf;
  unsigned char *l2_outbuf;
  unsigned char *sub_inbuf;
  unsigned char *sub_outbuf;
  unsigned char *last_outbuf;
  unsigned char inbuf[(LSUB_RAW+LSUB_Q+LSUB_P)*PACKETS_PER_SUBCHANNELFRAME +
			(L1_RAW+L1_Q+L1_P)*FRAMES_PER_SECTOR];
  unsigned char outbuf[(LSUB_RAW+LSUB_Q+LSUB_P)*PACKETS_PER_SUBCHANNELFRAME +
			(L1_RAW+L1_Q+L1_P)*FRAMES_PER_SECTOR];
  unsigned load_offset;

  l1_inbuf = l2_inbuf = sub_inbuf = inbuf;
  l1_outbuf = l2_outbuf = sub_outbuf = last_outbuf = outbuf;

  infp = fopen("sectors_in", "rb");
  outfp = fopen("sectors_out", "wb");

  sectortype= MODE_1;
  address = 0 + 75*2;

  switch (sectortype) {
		case MODE_1:
		case MODE_2:
			load_offset = 16;
		break;
		case MODE_2_FORM_1:
		case MODE_2_FORM_2:
			load_offset = 24;
		break;
		default:
			load_offset = 0;
  }
  while(1) {

	if (1 != fread(inbuf+load_offset,
		  sect_size[mask][encode], 1, infp)) { perror(""); break; }
	if (encode == 1) {
     		if (mask & DO_L2) {
			switch (sectortype) {
				case MODE_0:
				break;
				case MODE_1:
				break;
				case MODE_2:
					if (1 !=
					  fread(inbuf+load_offset+
						sect_size[mask][encode],
					  2336 - sect_size[mask][encode],
						1, infp)) { perror(""); break; }
				break;
				case MODE_2_FORM_1:
				break;
				case MODE_2_FORM_2:
					if (1 !=
					  fread(inbuf+load_offset+
						sect_size[mask][encode],
					  2324 - sect_size[mask][encode],
						1, infp)) { perror(""); break; }
				break;
				default:
					if (1 !=
					  fread(inbuf+load_offset+
						sect_size[mask][encode],
					  2448 - sect_size[mask][encode],
						1, infp)) { perror(""); break; }
					memset(inbuf,0,16);
					/*memset(inbuf+16+2048,0,12+272);*/
				break;
			}
			do_encode_L2(l2_inbuf, MODE_1, address);
			if (0) scramble_L2(l2_inbuf);
			last_outbuf = l1_inbuf = l2_inbuf;
			l1_outbuf = l2_inbuf;
			sub_inbuf = l2_inbuf + L2_RAW;
			sub_outbuf = l2_outbuf + 12 + 4+ L2_RAW+4+ 8+ L2_Q+L2_P;
     		}
     		if (mask & DO_L1) {
			do_encode_L1(l1_inbuf, l1_outbuf,1,1,1,1);
			last_outbuf = l1_outbuf;
			sub_inbuf = l1_inbuf + L1_RAW*FRAMES_PER_SECTOR;
			sub_outbuf = l1_outbuf + (L1_RAW+L1_Q+L1_P)*FRAMES_PER_SECTOR;
     		}
     		if (mask & DO_SUB) {
			do_encode_sub(sub_inbuf, sub_outbuf, 0, 0);
     		}
	} else {
     		if (mask & DO_L1) {
			do_decode_L1(l1_inbuf, l1_outbuf,1,1,1,1);
			last_outbuf = l2_inbuf = l1_outbuf;
			l2_outbuf = l1_inbuf;
			sub_inbuf = l1_inbuf + (L1_RAW+L1_Q+L1_P)*FRAMES_PER_SECTOR;
			sub_outbuf = l1_outbuf + L1_RAW*FRAMES_PER_SECTOR;
     		}
     		if (mask & DO_L2) {
			do_decode_L2(l2_inbuf, l2_outbuf);
			last_outbuf = l2_outbuf;
			sub_inbuf = l2_inbuf + L2_RAW+L2_Q+L2_P;
			sub_outbuf = l2_outbuf + L2_RAW;
     		}
     		if (mask & DO_SUB) {
			do_decode_sub(sub_inbuf, sub_outbuf, 1, 1);
     		}
	}
	if (1 != fwrite(last_outbuf, sect_size[mask][1 - encode], 1, outfp)) {
		perror("");
		break;
	}
	address++;
  }
#if 0
  /* flush the data from the delay lines with zeroed sectors, if necessary */
#endif
  return 0;
}
#endif
