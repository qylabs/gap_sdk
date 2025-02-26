#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wpointer-sign"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wswitch"

#include "Gap.h"
#include "CNN_BasicKernels_SQ8.h"

static int CoreCountDynamic = 1;
static int ActiveCore = gap_ncore();

static inline unsigned int __attribute__((always_inline)) ChunkSize(unsigned int X)

{
        unsigned int NCore;
        unsigned int Log2Core;
        unsigned int Chunk;

        if (CoreCountDynamic) NCore = ActiveCore; else NCore = gap_ncore();
        Log2Core = gap_fl1(NCore);
        Chunk = (X>>Log2Core) + ((X&(NCore-1))!=0);
        return Chunk;
}

unsigned short int SIGMOID_LUT_uint16[256] = {
    32768, 33451, 34133, 34813, 35493, 36169, 36843, 37513, 38180, 38841, 39498,
    40149, 40794, 41432, 42064, 42688, 43304, 43912, 44511, 45102, 45683, 46255,
    46817, 47369, 47911, 48443, 48964, 49475, 49975, 50464, 50942, 51409, 51865,
    52311, 52745, 53169, 53581, 53983, 54374, 54755, 55125, 55485, 55834, 56174,
    56503, 56823, 57133, 57433, 57724, 58007, 58280, 58544, 58800, 59048, 59288,
    59519, 59743, 59959, 60168, 60370, 60565, 60753, 60935, 61110, 61279, 61441,
    61599, 61750, 61896, 62036, 62172, 62302, 62428, 62549, 62666, 62778, 62886,
    62990, 63090, 63186, 63279, 63368, 63454, 63536, 63615, 63691, 63765, 63835,
    63903, 63968, 64030, 64090, 64148, 64204, 64257, 64308, 64357, 64405, 64450,
    64494, 64536, 64576, 64614, 64652, 64687, 64721, 64754, 64786, 64816, 64845,
    64873, 64900, 64926, 64950, 64974, 64997, 65019, 65039, 65060, 65079, 65097,
    65115, 65132, 65149, 65164, 65179, 65194, 65208, 65221, 65234, 65246, 65258,
    65269, 65280, 65291, 65301, 65310, 65319, 65328, 65337, 65345, 65352, 65360,
    65367, 65374, 65381, 65387, 65393, 65399, 65404, 65410, 65415, 65420, 65425,
    65429, 65433, 65438, 65442, 65445, 65449, 65453, 65456, 65459, 65462, 65465,
    65468, 65471, 65474, 65476, 65479, 65481, 65483, 65485, 65488, 65489, 65491,
    65493, 65495, 65497, 65498, 65500, 65501, 65503, 65504, 65505, 65507, 65508,
    65509, 65510, 65511, 65512, 65513, 65514, 65515, 65516, 65517, 65517, 65518,
    65519, 65520, 65520, 65521, 65522, 65522, 65523, 65523, 65524, 65524, 65525,
    65525, 65526, 65526, 65526, 65527, 65527, 65528, 65528, 65528, 65529, 65529,
    65529, 65529, 65530, 65530, 65530, 65530, 65531, 65531, 65531, 65531, 65531,
    65532, 65532, 65532, 65532, 65532, 65532, 65533, 65533, 65533, 65533, 65533,
    65533, 65533, 65533, 65534, 65534, 65534, 65534, 65534, 65534, 65534, 65534,
    65534, 65534, 65535};

#define NEAREST //Use nearest LUT element instead of linearly interpolate
int Sigmoid(int x){
	/* Input x: Q12 [-8:8] range

	   Output y = sig(x) -> Q15
	*/
#ifndef NEAREST
	int result, ua, ub, ut;
	int abs_x = (Abs(x) * 3) >> 9; // * 3/4 (*3 >>2) and clip it to [0:255] (>>7) ot be an index of the LUT
	if (abs_x > 255) {
		result = 0x1FFFC00; // result = 1 in Q25
	} else {
		ua = SIGMOID_LUT_uint16[abs_x];
		ub = SIGMOID_LUT_uint16[abs_x+1];
		ut = abs_x & 0xFF;
		result = (ua << 9) + ut * (ub-ua); // LUT in Q16 * ut in Q9 = Q25
	}
	if (x>0) result = result;
	else     result = (1<<25) - result;
	return result >> 10;
#else
	int result;
	int abs_x = (Abs(x) * 3) >> 9; // * 3/4 (*3 >>2) and clip it to [0:255] (>>7) ot be an index of the LUT
	if (abs_x > 255) {
		result = 0xFFFF; // result = 1 in Q16
	} else {
		result = SIGMOID_LUT_uint16[abs_x]; // LUT in Q16
	}
	if (x>0) result = result;
	else     result = (1<<16) - result;
	return result >> 1;
#endif
}

int Tanh(int x){
#ifndef NEAREST
	int result, ua, ub, ut;
	int abs_x = (Abs(x) * 3) >> 8; // 2*x
	if (abs_x > 255) {
		result = 0xFFFF00;
	} else {
		ua = SIGMOID_LUT_uint16[abs_x];
		ub = SIGMOID_LUT_uint16[abs_x+1];
		ut = abs_x & 0xFF;
		result = (ua << 8) + ut * (ub-ua);
	}
	if (x>0) result =  result - (1 << 23);
	else     result = -result + (1 << 23);
	return result >> 8; // back to 16 bits
#else
	int result;
	int abs_x = (Abs(x) * 3) >> 8; // 2*x
	if (abs_x > 255) {
		result = 0xFFFF;
	} else {
		result = SIGMOID_LUT_uint16[abs_x];
	}
	if (x>0) result =  result - (1 << 15);
	else     result = -result + (1 << 15);
	return result; // back to 16 bits
#endif
}

/*
 * Standalone activation
*/
static void Ker_Activation_SQ8(
        signed char * __restrict__ In,
        signed char * __restrict__ Out,
	unsigned int N,
        CNN_ActivationOper_T Activation,
	unsigned int ActScale, unsigned int ActScaleN, int A0, int B0, int C0
        )

{
        for (unsigned int i=0; i<N/2; i++) {
                int Acc0 = In[2*i], Acc1 = In[2*i+1];
		switch (Activation) {
			case ACT_NONE:     Acc0 = AT_SCALE(Acc0, ActScale, ActScaleN); Acc1 = AT_SCALE(Acc1, ActScale, ActScaleN); break;
			case ACT_RELU:     Acc0 = AT_SCALE(Max(0, Acc0), ActScale, ActScaleN); Acc1 = AT_SCALE(Max(0, Acc1), ActScale, ActScaleN); break;
			case ACT_RELUM:    Acc0 = AT_SCALE(Max(A0, Acc0), ActScale, ActScaleN); Acc1 = AT_SCALE(Max(A0, Acc1), ActScale, ActScaleN); break;
			case ACT_RELUMN:   Acc0 = AT_SCALE(Min(B0, Max(A0, Acc0)), ActScale, ActScaleN); Acc1 = AT_SCALE(Min(B0, Max(A0, Acc1)), ActScale, ActScaleN); break;
			case ACT_RELUN:    Acc0 = AT_SCALE(AT_CLIP_POS(Acc0, A0), ActScale, ActScaleN); Acc1 = AT_SCALE(AT_CLIP_POS(Acc1, A0), ActScale, ActScaleN); break;
			case ACT_HSIGMOID: Acc0 = AT_SCALE(AT_CLIP_POS(Acc0 + B0, A0) * C0, ActScale, ActScaleN); Acc1 = AT_SCALE(AT_CLIP_POS(Acc1 + B0, A0) * C0, ActScale, ActScaleN); break;
			case ACT_HSWISH:   Acc0 = AT_SCALE(AT_CLIP_POS(Acc0 + B0, A0) * C0 * Acc0, ActScale, ActScaleN); Acc1 = AT_SCALE(AT_CLIP_POS(Acc1 + B0, A0) * C0 * Acc1, ActScale, ActScaleN); break;
			case ACT_LEAKYRELU:
				{
					int Neg0 = gap_bitextractu(Acc0, 1, 31), Pos0 = !Neg0;
					int Acc0N = AT_NORM(Acc0 * A0, 7);
					Acc0 = AT_SCALE((Neg0*Acc0N+Pos0*Acc0), ActScale, ActScaleN);
					int Neg1 = gap_bitextractu(Acc1, 1, 31), Pos1 = !Neg1;
					int Acc1N = AT_NORM(Acc1 * A0, 7);
					Acc1 = AT_SCALE((Neg1*Acc1N+Pos1*Acc1), ActScale, ActScaleN);
				//	Acc0 = AT_SCALE(((Acc0<0) ? AT_NORM((Acc0 * A0), 7):Acc0), ActScale, ActScaleN);
				//	Acc1 = AT_SCALE(((Acc1<0) ? AT_NORM((Acc1 * A0), 7):Acc1), ActScale, ActScaleN);
				}
				break;
			case ACT_SIGMOID:
				{
					// Assumes input (Acc) in Sq[-8:8] = 16 / 256 = 2**(-4)
					// y = Sigmoid(x) expects x in Q12 --> Sin/Sq12 = 2**(-4) / 2**(-12) = 2**(8) --> << 8
					// y in Q15 is then shifted to fit int8 Q7 data --> >> 8 and scaled to the output scale with ActScale
					int Acc0N = Acc0 << 8;
					Acc0 = AT_SCALE((Sigmoid(Acc0N) >> 8), ActScale, ActScaleN);
					int Acc1N = Acc1 << 8;
					Acc1 = AT_SCALE((Sigmoid(Acc1N) >> 8), ActScale, ActScaleN);
				}
				break;
		}
                Out[2*i] = gap_clip(Acc0, 7), Out[2*i+1] = gap_clip(Acc1, 7);
        }
	if (N&0x1) {
        	unsigned int i=N-1;
                int Acc0 = In[i];
		switch (Activation) {
			case ACT_NONE:     Acc0 = AT_SCALE(Acc0, ActScale, ActScaleN); break;
			case ACT_RELU:     Acc0 = AT_SCALE(Max(0, Acc0), ActScale, ActScaleN); break;
			case ACT_RELUM:    Acc0 = AT_SCALE(Max(A0, Acc0), ActScale, ActScaleN); break;
			case ACT_RELUMN:   Acc0 = AT_SCALE(Min(B0, Max(A0, Acc0)), ActScale, ActScaleN); break;
			case ACT_RELUN:    Acc0 = AT_SCALE(AT_CLIP_POS(Acc0, A0), ActScale, ActScaleN); break;
			case ACT_HSIGMOID: Acc0 = AT_SCALE(AT_CLIP_POS(Acc0 + B0, A0) * C0, ActScale, ActScaleN); break;
			case ACT_HSWISH:   Acc0 = AT_SCALE(AT_CLIP_POS(Acc0 + B0, A0) * C0 * Acc0, ActScale, ActScaleN); break;
			case ACT_LEAKYRELU:
				{
					int Neg0 = gap_bitextractu(Acc0, 1, 31), Pos0 = !Neg0;
					int Acc0N = AT_NORM(Acc0 * A0, 7);
					Acc0 = AT_SCALE((Neg0*Acc0N+Pos0*Acc0), ActScale, ActScaleN);
				//	Acc0 = AT_SCALE(((Acc0<0) ? AT_NORM((Acc0 * A0), 7):Acc0), ActScale, ActScaleN);
				}
				break;
			case ACT_SIGMOID:
				{
					int Acc0N = Acc0 << 8;
					Acc0 = AT_SCALE((Sigmoid(Acc0N) >> 8), ActScale, ActScaleN);
				}
				break;
		}
                Out[i] = gap_clip(Acc0, 7);
	}
}

/*
 * Standalone activation variant with Scale = 1.0
*/
static void Ker_ActivationScale1_SQ8(
        signed char * __restrict__ In,
        signed char * __restrict__ Out,
	unsigned int N,
        CNN_ActivationOper_T Activation,
	int A0,
        int B0
        )

{
        for (unsigned int i=0; i<N/2; i++) {
                int Acc0 = In[2*i], Acc1 = In[2*i+1];
		switch (Activation) {
			case ACT_RELU: Acc0 = Max(0, Acc0); Acc1 = Max(0, Acc1); break;
			case ACT_RELUN: Acc0 = AT_CLIP_POS(Acc0, A0); Acc1 = AT_CLIP_POS(Acc1, A0); break;
			case ACT_RELUM: Acc0 = Max(A0, Acc0); Acc1 = Max(A0, Acc1); break;
			case ACT_RELUMN: Acc0 = Min(B0, Max(A0, Acc0)); Acc1 = Min(B0, Max(A0, Acc1)); break;
		}
                Out[2*i] = Acc0; Out[2*i+1] = Acc1;
        }
	if (N&0x1) {
        	unsigned int i=N-1;
                int Acc0 = In[i];
		switch (Activation) {
			case ACT_RELU: Acc0 = Max(0, Acc0); break;
			case ACT_RELUN: Acc0 = AT_CLIP_POS(Acc0, A0); break;
			case ACT_RELUM: Acc0 = Max(A0, Acc0); break;
			case ACT_RELUMN: Acc0 = Min(B0, Max(A0, Acc0)); break;
		}
                Out[i] = Acc0;
	}
}

static void Ker_Activation_ScaleIn_SQ8(
        signed char * __restrict__ In,
        signed char * __restrict__ Out,
        unsigned int Scale,
        unsigned int ScaleN,
	unsigned int N,
        CNN_ActivationOper_T Activation,
	unsigned int ActScale, unsigned int ActScaleN, int A0, int B0, int C0
        )

{
        for (unsigned int i=0; i<N/2; i++) {
        	int Acc0 = gap_clip(AT_SCALE(In[2*i], Scale, ScaleN), 7);
        	int Acc1 = gap_clip(AT_SCALE(In[2*i+1], Scale, ScaleN), 7);
		switch (Activation) {
			case ACT_NONE:     Acc0 = AT_SCALE(Acc0, ActScale, ActScaleN); Acc1 = AT_SCALE(Acc1, ActScale, ActScaleN); break;
			case ACT_RELU:     Acc0 = AT_SCALE(Max(0, Acc0), ActScale, ActScaleN); Acc1 = AT_SCALE(Max(0, Acc1), ActScale, ActScaleN); break;
			case ACT_RELUN:    Acc0 = AT_SCALE(AT_CLIP_POS(Acc0, A0), ActScale, ActScaleN); Acc1 = AT_SCALE(AT_CLIP_POS(Acc1, A0), ActScale, ActScaleN); break;
			case ACT_RELUM:    Acc0 = AT_SCALE(Max(A0, Acc0), ActScale, ActScaleN); Acc1 = AT_SCALE(Max(A0, Acc1), ActScale, ActScaleN); break;
			case ACT_RELUMN:   Acc0 = AT_SCALE(Min(B0, Max(A0, Acc0)), ActScale, ActScaleN); Acc1 = AT_SCALE(Min(B0, Max(A0, Acc1)), ActScale, ActScaleN); break;
			case ACT_HSIGMOID: Acc0 = AT_SCALE(AT_CLIP_POS(Acc0 + B0, A0) * C0, ActScale, ActScaleN); Acc1 = AT_SCALE(AT_CLIP_POS(Acc1 + B0, A0) * C0, ActScale, ActScaleN); break;
			case ACT_HSWISH:   Acc0 = AT_SCALE(AT_CLIP_POS(Acc0 + B0, A0) * C0 * Acc0, ActScale, ActScaleN); Acc1 = AT_SCALE(AT_CLIP_POS(Acc1 + B0, A0) * C0 * Acc1, ActScale, ActScaleN); break;
			case ACT_LEAKYRELU:
				{
					int Neg0 = gap_bitextractu(Acc0, 1, 31), Pos0 = !Neg0;
					int Acc0N = AT_NORM(Acc0 * A0, 7);
					Acc0 = AT_SCALE((Neg0*Acc0N+Pos0*Acc0), ActScale, ActScaleN);
					int Neg1 = gap_bitextractu(Acc1, 1, 31), Pos1 = !Neg1;
					int Acc1N = AT_NORM(Acc1 * A0, 7);
					Acc1 = AT_SCALE((Neg1*Acc1N+Pos1*Acc1), ActScale, ActScaleN);
				//	Acc0 = AT_SCALE(((Acc0<0) ? AT_NORM((Acc0 * A0), 7):Acc0), ActScale, ActScaleN);
				//	Acc1 = AT_SCALE(((Acc1<0) ? AT_NORM((Acc1 * A0), 7):Acc1), ActScale, ActScaleN);
				}
				break;
			case ACT_SIGMOID:
				{
					// Assumes input (Acc) in Sq[-8:8] = 16 / 256 = 2**(-4)
					// y = Sigmoid(x) expects x in Q12 --> Sin/Sq12 = 2**(-4) / 2**(-12) = 2**(8) --> << 8
					// y in Q15 is then shifted to fit int8 Q7 data --> >> 8 and scaled to the output scale with ActScale
					int Acc0N = Acc0 << 8;
					Acc0 = AT_SCALE((Sigmoid(Acc0N) >> 8), ActScale, ActScaleN);
					int Acc1N = Acc1 << 8;
					Acc1 = AT_SCALE((Sigmoid(Acc1N) >> 8), ActScale, ActScaleN);
				}
				break;
		}
                Out[2*i] = gap_clip(Acc0, 7), Out[2*i+1] = gap_clip(Acc1, 7);
        }
	if (N&0x1) {
        	unsigned int i=N-1;
        	int Acc0 = gap_clip(AT_SCALE(In[i], Scale, ScaleN), 7);
		switch (Activation) {
			case ACT_NONE:     Acc0 = AT_SCALE(Acc0, ActScale, ActScaleN); break;
			case ACT_RELU:     Acc0 = AT_SCALE(Max(0, Acc0), ActScale, ActScaleN); break;
			case ACT_RELUN:    Acc0 = AT_SCALE(AT_CLIP_POS(Acc0, A0), ActScale, ActScaleN); break;
			case ACT_RELUM:    Acc0 = AT_SCALE(Max(A0, Acc0), ActScale, ActScaleN); break;
			case ACT_RELUMN:   Acc0 = AT_SCALE(Min(B0, Max(A0, Acc0)), ActScale, ActScaleN); break;
			case ACT_HSIGMOID: Acc0 = AT_SCALE(AT_CLIP_POS(Acc0 + B0, A0) * C0, ActScale, ActScaleN); break;
			case ACT_HSWISH:   Acc0 = AT_SCALE(AT_CLIP_POS(Acc0 + B0, A0) * C0 * Acc0, ActScale, ActScaleN); break;
			case ACT_LEAKYRELU:
				{
					int Neg0 = gap_bitextractu(Acc0, 1, 31), Pos0 = !Neg0;
					int Acc0N = AT_NORM(Acc0 * A0, 7);
					Acc0 = AT_SCALE((Neg0*Acc0N+Pos0*Acc0), ActScale, ActScaleN);
				//	Acc0 = AT_SCALE(((Acc0<0) ? AT_NORM((Acc0 * A0), 7):Acc0), ActScale, ActScaleN);
				}
				break;
			case ACT_SIGMOID:
				{
					int Acc0N = Acc0 << 8;
					Acc0 = AT_SCALE((Sigmoid(Acc0N) >> 8), ActScale, ActScaleN);
				}
				break;
		}
                Out[i] = gap_clip(Acc0, 7);
	}
}

/*
 * Standalone activation variant with Scale = 1.0
*/
static void Ker_ActivationScale1_ScaleIn_SQ8(
        signed char * __restrict__ In,
        signed char * __restrict__ Out,
        unsigned int Scale,
        unsigned int ScaleN,
	unsigned int N,
        CNN_ActivationOper_T Activation,
	int A0,
        int B0
        )

{
        for (unsigned int i=0; i<N/2; i++) {
        	int Acc0 = gap_clip(AT_SCALE(In[2*i], Scale, ScaleN), 7);
        	int Acc1 = gap_clip(AT_SCALE(In[2*i+1], Scale, ScaleN), 7);
		switch (Activation) {
			case ACT_RELU: Acc0 = Max(0, Acc0); Acc1 = Max(0, Acc1); break;
			case ACT_RELUN: Acc0 = AT_CLIP_POS(Acc0, A0); Acc1 = AT_CLIP_POS(Acc1, A0); break;
			case ACT_RELUM: Acc0 = Max(A0, Acc0); Acc1 = Max(A0, Acc1); break;
			case ACT_RELUMN: Acc0 = Min(B0, Max(A0, Acc0)); Acc1 = Min(B0, Max(A0, Acc1)); break;
		}
                Out[2*i] = Acc0; Out[2*i+1] = Acc1;
        }
	if (N&0x1) {
        	unsigned int i=N-1;
        	int Acc0 = gap_clip(AT_SCALE(In[i], Scale, ScaleN), 7);
		switch (Activation) {
			case ACT_RELU: Acc0 = Max(0, Acc0); break;
			case ACT_RELUN: Acc0 = AT_CLIP_POS(Acc0, A0); break;
			case ACT_RELUM: Acc0 = Max(A0, Acc0); break;
			case ACT_RELUMN: Acc0 = Min(B0, Max(A0, Acc0)); break;
		}
                Out[i] = Acc0;
	}
}

/*
 * Conv/Linear DP scaling followed by an optional activation, Out buffer is different from In Buffer
*/
static void KerReduct_Activation_SQ8(
        int * __restrict__ In,
        signed char * __restrict__ Out,
	unsigned int N,
	unsigned int Scale,
	unsigned int ScaleN,
        CNN_ActivationOper_T Activation,
	unsigned int ActScale, unsigned int ActScaleN, int A0, int B0, int C0
        )

{
        for (unsigned int i=0; i<N; i++) {
                int Acc0 = gap_clip(AT_SCALE(In[i], Scale, ScaleN), 7);
		switch (Activation) {
			case ACT_NONE:
				break;
			case ACT_RELU:
				Acc0 = AT_SCALE(Max(0, Acc0), ActScale, ActScaleN);
				break;
			case ACT_RELUN:
				Acc0 = AT_SCALE(AT_CLIP_POS(Acc0, A0), ActScale, ActScaleN);
				break;
			case ACT_RELUM:
				Acc0 = AT_SCALE(Max(B0, Acc0), ActScale, ActScaleN);
				break;
			case ACT_RELUMN:
				Acc0 = AT_SCALE(Min(B0, Max(Acc0, A0)), ActScale, ActScaleN);
				break;
			case ACT_HSIGMOID:
				Acc0 = AT_SCALE(AT_CLIP_POS(Acc0 + B0, A0) * C0, ActScale, ActScaleN);
				break;
			case ACT_HSWISH:
				Acc0 = AT_SCALE(AT_CLIP_POS(Acc0 + B0, A0) * C0 * Acc0, ActScale, ActScaleN);
				break;
			case ACT_LEAKYRELU:
				{
					int Neg0 = gap_bitextractu(Acc0, 1, 31), Pos0 = !Neg0;
					int Acc0N = AT_NORM(Acc0 * A0, 7);
					Acc0 = AT_SCALE((Neg0*Acc0N+Pos0*Acc0), ActScale, ActScaleN);
				//	Acc0 = AT_SCALE(((Acc0<0) ? AT_NORM(Acc0 * A0, 7):Acc0), ActScale, ActScaleN);
				}
				break;
			case ACT_SIGMOID:
				{
					// Assumes input (Acc) in Sq[-8:8] = 16 / 256 = 2**(-4)
					// y = Sigmoid(x) expects x in Q12 --> Sin/Sq12 = 2**(-4) / 2**(-12) = 2**(8) --> << 8
					// y in Q15 is then shifted to fit int8 Q7 data --> >> 8 and scaled to the output scale with ActScale
					int Acc0N = Acc0 << 8;
					Acc0 = AT_SCALE((Sigmoid(Acc0N) >> 8), ActScale, ActScaleN);
				}
				break;
		}
                Out[i] = gap_clip(Acc0, 7);
        }
}

/*
 * Conv/Linear DP scaling followed by an optional activation, variant for ScaleAct=1.0, Out buffer is different from In Buffer
*/
static void KerReduct_ActivationScale1_SQ8(
        int * __restrict__ In,
        signed char * __restrict__ Out,
	unsigned int N,
	unsigned int Scale,
	unsigned int ScaleN,
        CNN_ActivationOper_T Activation,
	int A0, int B0, int C0
        )

{
        for (unsigned int i=0; i<N; i++) {
                int Acc0 = gap_clip(AT_SCALE(Scale, In[i], ScaleN), 7);
		switch (Activation) {
			case ACT_NONE:
				break;
			case ACT_RELU:
				Acc0 = Max(0, Acc0);
				break;
			case ACT_RELUN:
				Acc0 = AT_CLIP_POS(Acc0, A0);
				break;
			case ACT_RELUM:
				Acc0 = Max(Acc0, A0);
				break;
			case ACT_RELUMN:
				Acc0 = Min(B0, Max(Acc0, A0));
				break;
		}
                Out[i] = Acc0;
        }
}

/*
 * Conv/Linear DP scaling followed by an optional activation, In place version
 * Input is 32b int output is 8b
*/
static void KerReductIO_Activation_SQ8(
        signed char *__restrict__ Out,
        int *__restrict__ In,
	unsigned int N,
	unsigned int Scale,
	unsigned int ScaleN,
        CNN_ActivationOper_T Activation,
	unsigned int ActScale, unsigned int ActScaleN, int A0, int B0, int C0
        )

{
        for (unsigned int i=0; i<N; i++) {
                int Acc0 = gap_clip(AT_SCALE(In[i], Scale, ScaleN), 7);
		switch (Activation) {
			case ACT_NONE:
				break;
			case ACT_RELU:
				Acc0 = AT_SCALE(Max(0, Acc0), ActScale, ActScaleN);
				break;
			case ACT_RELUN:
				Acc0 = AT_SCALE(AT_CLIP_POS(Acc0, A0), ActScale, ActScaleN);
				break;
			case ACT_RELUM:
				Acc0 = AT_SCALE(Max(A0, Acc0), ActScale, ActScaleN);
				break;
			case ACT_RELUMN:
				Acc0 = AT_SCALE(Min(B0, Max(A0, Acc0)), ActScale, ActScaleN);
				break;
			case ACT_HSIGMOID:
				Acc0 = AT_SCALE(AT_CLIP_POS(Acc0 + B0, A0) * C0, ActScale, ActScaleN);
				break;
			case ACT_HSWISH:
				Acc0 = AT_SCALE(AT_CLIP_POS(Acc0 + B0, A0) * C0 * Acc0, ActScale, ActScaleN);
				break;
			case ACT_LEAKYRELU:
				{
					int Neg0 = gap_bitextractu(Acc0, 1, 31), Pos0 = !Neg0;
					int Acc0N = AT_NORM(Acc0 * A0, 7);
					Acc0 = AT_SCALE((Neg0*Acc0N+Pos0*Acc0), ActScale, ActScaleN);
				//	Acc0 = AT_SCALE(((Acc0<0) ? AT_NORM(Acc0 * A0, 7):Acc0), ActScale, ActScaleN);
				}
				break;
			case ACT_SIGMOID:
				{
					// Assumes input (Acc) in Sq[-8:8] = 16 / 256 = 2**(-4)
					// y = Sigmoid(x) expects x in Q12 --> Sin/Sq12 = 2**(-4) / 2**(-12) = 2**(8) --> << 8
					// y in Q15 is then shifted to fit int8 Q7 data --> >> 8 and scaled to the output scale with ActScale
					int Acc0N = Acc0 << 8;
					Acc0 = AT_SCALE((Sigmoid(Acc0N) >> 8), ActScale, ActScaleN);
				}
				break;
		}
                Out[i] = gap_clip(Acc0, 7);
        }
}

/*
 * Conv/Linear DP scaling followed by an optional activation, variant for ActScale=1.0, In place version
 * Input is 32b int output is 8b
*/
static void KerReductIO_ActivationScale1_SQ8(
        signed char *__restrict__ Out,
        int *__restrict__ In,
	unsigned int N,
	unsigned int Scale,
	unsigned int ScaleN,
        CNN_ActivationOper_T Activation,
	int A0, int B0, int C0
        )

{
        for (unsigned int i=0; i<N; i++) {
                int Acc0 = gap_clip(AT_SCALE(In[i], Scale, ScaleN), 7);
		switch (Activation) {
			case ACT_NONE:
				break;
			case ACT_RELU:
				Acc0 = Max(0, Acc0);
				break;
			case ACT_RELUN:
				Acc0 = AT_CLIP_POS(Acc0, A0);
				break;
			case ACT_RELUM:
				Acc0 = Max(B0, Acc0);
				break;
			case ACT_RELUMN:
				Acc0 = Min(B0, Max(B0, Acc0));
				break;
		}
                Out[i] = Acc0;
        }
}

/*
 * Conv/Linear DP scaling followed by an optional activation, Out buffer is different from In Buffer
 * Partial unroll to avoid load use penalty
*/
static void _KerReduct_Activation_SQ8(
        int * __restrict__ In,
        signed char * __restrict__ Out,
	unsigned int N,
	unsigned int Scale,
	unsigned int ScaleN,
        CNN_ActivationOper_T Activation,
	unsigned int ActScale, unsigned int ActScaleN, int A0, int B0, int C0
        )

{
        for (unsigned int i=0; i<(N/2); i++) {
                int Acc0 = gap_clip(AT_SCALE(In[2*i+0], Scale, ScaleN), 7);
                int Acc1 = gap_clip(AT_SCALE(In[2*i+1], Scale, ScaleN), 7);
		
		switch (Activation) {
			case ACT_NONE:
				break;
			case ACT_RELU:
				Acc0 = AT_SCALE(Max(0, Acc0), ActScale, ActScaleN);
				Acc1 = AT_SCALE(Max(0, Acc1), ActScale, ActScaleN);
				break;
			case ACT_RELUN:
				Acc0 = AT_SCALE(AT_CLIP_POS(Acc0, A0), ActScale, ActScaleN);
				Acc1 = AT_SCALE(AT_CLIP_POS(Acc1, A0), ActScale, ActScaleN);
				break;
			case ACT_RELUM:
				Acc0 = AT_SCALE(Max(A0, Acc0), ActScale, ActScaleN);
				Acc1 = AT_SCALE(Max(A0, Acc1), ActScale, ActScaleN);
				break;
			case ACT_RELUMN:
				Acc0 = AT_SCALE(Min(B0, Max(A0, Acc0)), ActScale, ActScaleN);
				Acc1 = AT_SCALE(Min(B0, Max(A0, Acc1)), ActScale, ActScaleN);
				break;
			case ACT_HSIGMOID:
				Acc0 = AT_SCALE(AT_CLIP_POS(Acc0 + B0, A0) * C0, ActScale, ActScaleN);
				Acc1 = AT_SCALE(AT_CLIP_POS(Acc1 + B0, A0) * C0, ActScale, ActScaleN);
				break;
			case ACT_HSWISH:
				Acc0 = AT_SCALE(AT_CLIP_POS(Acc0 + B0, A0) * C0 * Acc0, ActScale, ActScaleN);
				Acc1 = AT_SCALE(AT_CLIP_POS(Acc1 + B0, A0) * C0 * Acc1, ActScale, ActScaleN);
				break;
			case ACT_LEAKYRELU:
				{
					int Neg0 = gap_bitextractu(Acc0, 1, 31), Pos0 = !Neg0;
					int Neg1 = gap_bitextractu(Acc1, 1, 31), Pos1 = !Neg1;
					int Acc0N = AT_NORM(Acc0 * A0, 7);
					int Acc1N = AT_NORM(Acc1 * A0, 7);
					Acc0 = AT_SCALE((Neg0*Acc0N+Pos0*Acc0), ActScale, ActScaleN);
					Acc1 = AT_SCALE((Neg1*Acc1N+Pos1*Acc1), ActScale, ActScaleN);

				//	Acc0 = AT_SCALE(((Acc0<0) ? AT_NORM(Acc0 * A0, 7):Acc0), ActScale, ActScaleN);
				//	Acc1 = AT_SCALE(((Acc1<0) ? AT_NORM(Acc1 * A0, 7):Acc1), ActScale, ActScaleN);
				}
				break;
		}
                Out[2*i] = gap_clip(Acc0, 7); Out[2*i+1] = gap_clip(Acc1, 7);
        }
        if (N&0x1) {
                int Acc0 = gap_clip(AT_SCALE(In[N-1], Scale, ScaleN), 7);
		switch (Activation) {
			case ACT_NONE:
				break;
			case ACT_RELU:
				Acc0 = AT_SCALE(Max(0, Acc0), ActScale, ActScaleN);
				break;
			case ACT_RELUN:
				Acc0 = AT_SCALE(AT_CLIP_POS(Acc0, A0), ActScale, ActScaleN);
				break;
			case ACT_RELUM:
				Acc0 = AT_SCALE(Max(A0, Acc0), ActScale, ActScaleN);
				break;
			case ACT_RELUMN:
				Acc0 = AT_SCALE(Min(B0, Max(A0, Acc0)), ActScale, ActScaleN);
				break;
			case ACT_HSIGMOID:
				Acc0 = AT_SCALE(AT_CLIP_POS(Acc0 + B0, A0) * C0, ActScale, ActScaleN);
				break;
			case ACT_HSWISH:
				Acc0 = AT_SCALE(AT_CLIP_POS(Acc0 + B0, A0) * C0 * Acc0, ActScale, ActScaleN);
				break;
			case ACT_LEAKYRELU:
				{
					int Neg0 = gap_bitextractu(Acc0, 1, 31), Pos0 = !Neg0;
					int Acc0N = AT_NORM(Acc0 * A0, 7);
					Acc0 = AT_SCALE((Neg0*Acc0N+Pos0*Acc0), ActScale, ActScaleN);

					// Acc0 = AT_SCALE(((Acc0<0) ? AT_NORM(Acc0 * A0, 7):Acc0), ActScale, ActScaleN);
				}
				break;
		}
                Out[N-1] = gap_clip(Acc0, 7);
        }
}

/*
 * Conv/Linear DP scaling followed by an optional activation, variant for ActScale=1.0, Out buffer is different from In Buffer
 * Partial unroll to avoid load use penalty
*/
static void _KerReduct_ActivationScale1_SQ8(
        int * __restrict__ In,
        signed char * __restrict__ Out,
	unsigned int N,
	unsigned int Scale,
	unsigned int ScaleN,
        CNN_ActivationOper_T Activation,
	int A0, int B0, int C0
        )

{
        for (unsigned int i=0; i<(N/2); i++) {
                int Acc0 = gap_clip(AT_SCALE(In[2*i+0], Scale, ScaleN), 7);
                int Acc1 = gap_clip(AT_SCALE(In[2*i+1], Scale, ScaleN), 7);
		
		switch (Activation) {
			case ACT_NONE:
				break;
			case ACT_RELU:
				Acc0 = Max(0, Acc0);
				Acc1 = Max(0, Acc1);
				break;
			case ACT_RELUN:
				Acc0 = AT_CLIP_POS(Acc0, A0);
				Acc1 = AT_CLIP_POS(Acc1, A0);
				break;
			case ACT_RELUM:
				Acc0 = Max(A0, Acc0);
				Acc1 = Max(A0, Acc1);
				break;
			case ACT_RELUMN:
				Acc0 = Min(B0, Max(A0, Acc0));
				Acc1 = Min(B0, Max(A0, Acc1));
				break;
		}
                Out[2*i]   = Acc0; Out[2*i+1] = Acc1;
        }
        if (N&0x1) {
                int Acc0 = gap_clip(AT_SCALE(In[N-1], Scale, ScaleN), 7);
		switch (Activation) {
			case ACT_NONE:
				break;
			case ACT_RELU:
				Acc0 = Max(0, Acc0);
				break;
			case ACT_RELUN:
				Acc0 = AT_CLIP_POS(Acc0, A0);
				break;
			case ACT_RELUM:
				Acc0 = Max(A0, Acc0);
				break;
			case ACT_RELUMN:
				Acc0 = Min(B0, Max(A0, Acc0));
				break;
		}
                Out[N-1] = Acc0;
        }
}

/*
 * Conv/Linear DP scaling followed by an optional activation, In place version
 * Input is 32b int output is 8b
 * Partially unrolled version to avoid load use penalty
*/
static void _KerReductIO_Activation_SQ8(
        signed char * __restrict__ Out,
        int *__restrict__ In,
	unsigned int N,
	unsigned int Scale,
	unsigned int ScaleN,
        CNN_ActivationOper_T Activation,
	unsigned int ActScale, unsigned int ActScaleN, int A0, int B0, int C0
        )

{
        for (unsigned int i=0; i<(N/2); i++) {
                int Acc0 = gap_clip(AT_SCALE(In[2*i+0], Scale, ScaleN), 7);
                int Acc1 = gap_clip(AT_SCALE(In[2*i+1], Scale, ScaleN), 7);
		switch (Activation) {
			case ACT_NONE:
				break;
			case ACT_RELU:
				Acc0 = AT_SCALE(Max(0, Acc0), ActScale, ActScaleN);
				Acc1 = AT_SCALE(Max(0, Acc1), ActScale, ActScaleN);
				break;
			case ACT_RELUN:
				Acc0 = AT_SCALE(AT_CLIP_POS(Acc0, A0), ActScale, ActScaleN);
				Acc1 = AT_SCALE(AT_CLIP_POS(Acc1, A0), ActScale, ActScaleN);
				break;
			case ACT_RELUM:
				Acc0 = AT_SCALE(Max(A0, Acc0), ActScale, ActScaleN);
				Acc1 = AT_SCALE(Max(A0, Acc1), ActScale, ActScaleN);
				break;
			case ACT_RELUMN:
				Acc0 = AT_SCALE(Min(B0, Max(A0, Acc0)), ActScale, ActScaleN);
				Acc1 = AT_SCALE(Min(B0, Max(A0, Acc1)), ActScale, ActScaleN);
				break;
			case ACT_HSIGMOID:
				Acc0 = AT_SCALE(AT_CLIP_POS(Acc0 + B0, A0) * C0, ActScale, ActScaleN);
				Acc1 = AT_SCALE(AT_CLIP_POS(Acc1 + B0, A0) * C0, ActScale, ActScaleN);
				break;
			case ACT_HSWISH:
				Acc0 = AT_SCALE(AT_CLIP_POS(Acc0 + B0, A0) * C0 * Acc0, ActScale, ActScaleN);
				Acc1 = AT_SCALE(AT_CLIP_POS(Acc1 + B0, A0) * C0 * Acc1, ActScale, ActScaleN);
				break;
			case ACT_LEAKYRELU:
				{
					int Neg0 = gap_bitextractu(Acc0, 1, 31), Pos0 = !Neg0;
					int Neg1 = gap_bitextractu(Acc1, 1, 31), Pos1 = !Neg1;
					int Acc0N = AT_NORM(Acc0 * A0, 7);
					int Acc1N = AT_NORM(Acc1 * A0, 7);
					Acc0 = AT_SCALE((Neg0*Acc0N+Pos0*Acc0), ActScale, ActScaleN);
					Acc1 = AT_SCALE((Neg1*Acc1N+Pos1*Acc1), ActScale, ActScaleN);

				//	Acc0 = AT_SCALE(((Acc0<0) ? AT_NORM(Acc0 * A0, 7):Acc0), ActScale, ActScaleN);
				//	Acc1 = AT_SCALE(((Acc1<0) ? AT_NORM(Acc1 * A0, 7):Acc1), ActScale, ActScaleN);
				}
				break;
		}
                Out[2*i]   = gap_clip(Acc0, 7); Out[2*i+1] = gap_clip(Acc1, 7);
        }
        if (N&0x1) {
                int Acc0 = gap_clip(AT_SCALE(In[N-1], Scale, ScaleN), 7);
		switch (Activation) {
			case ACT_NONE:
				break;
			case ACT_RELU:
				Acc0 = AT_SCALE(Max(0, Acc0), ActScale, ActScaleN);
				break;
			case ACT_RELUN:
				Acc0 = AT_SCALE(AT_CLIP_POS(Acc0, A0), ActScale, ActScaleN);
				break;
			case ACT_RELUM:
				Acc0 = AT_SCALE(Max(A0, Acc0), ActScale, ActScaleN);
				break;
			case ACT_RELUMN:
				Acc0 = AT_SCALE(Min(B0, Max(A0, Acc0)), ActScale, ActScaleN);
				break;
			case ACT_HSIGMOID:
				Acc0 = AT_SCALE(AT_CLIP_POS(Acc0 + B0, A0) * C0, ActScale, ActScaleN);
				break;
			case ACT_HSWISH:
				Acc0 = AT_SCALE(AT_CLIP_POS(Acc0 + B0, A0) * C0 * Acc0, ActScale, ActScaleN);
				break;
			case ACT_LEAKYRELU:
				{
					int Neg0 = gap_bitextractu(Acc0, 1, 31), Pos0 = !Neg0;
					int Acc0N = AT_NORM(Acc0 * A0, 7);
					Acc0 = AT_SCALE((Neg0*Acc0N+Pos0*Acc0), ActScale, ActScaleN);

					// Acc0 = AT_SCALE(((Acc0<0) ? AT_NORM(Acc0 * A0, 7):Acc0), ActScale, ActScaleN);
				}
				break;
		}
                Out[N-1] = gap_clip(Acc0, 7);
        }
}

/*
 * Conv/Linear DP scaling followed by an optional activation, Variant for ActScale=1.0, In place version
 * Input is 32b int output is 8b
 * Partially unrolled version to avoid load use penalty
*/
static void _KerReductIO_ActivationScale1_SQ8(
        signed char *__restrict__ Out,
        int *__restrict__ In,
	unsigned int N,
	unsigned int Scale,
	unsigned int ScaleN,
        CNN_ActivationOper_T Activation,
	int A0, int B0, int C0
        )

{
        for (unsigned int i=0; i<(N/2); i++) {
                int Acc0 = gap_clip(AT_SCALE(In[2*i+0], Scale, ScaleN), 7);
                int Acc1 = gap_clip(AT_SCALE(In[2*i+1], Scale, ScaleN), 7);
		switch (Activation) {
			case ACT_NONE:
				break;
			case ACT_RELU:
				Acc0 = Max(0, Acc0);
				Acc1 = Max(0, Acc1);
				break;
			case ACT_RELUN:
				Acc0 = AT_CLIP_POS(Acc0, A0);
				Acc1 = AT_CLIP_POS(Acc1, A0);
				break;
			case ACT_RELUM:
				Acc0 = Max(A0, Acc0);
				Acc1 = Max(A0, Acc1);
				break;
			case ACT_RELUMN:
				Acc0 = Min(B0, Max(A0, Acc0));
				Acc1 = Min(B0, Max(A0, Acc1));
				break;
		}
                Out[2*i]   = Acc0; Out[2*i+1] = Acc1;
        }
        if (N&0x1) {
                int Acc0 = gap_clip(AT_SCALE(In[N-1], Scale, ScaleN), 7);
		switch (Activation) {
			case ACT_NONE:
				break;
			case ACT_RELU:
				Acc0 = Max(0, Acc0);
				break;
			case ACT_RELUN:
				Acc0 = AT_CLIP_POS(Acc0, A0);
				break;
			case ACT_RELUM:
				Acc0 = Max(A0, Acc0);
				break;
			case ACT_RELUMN:
				Acc0 = Min(B0, Max(A0, Acc0));
				break;
		}
                Out[N-1] = Acc0;
        }
}

/*
 * Buffer compaction, scattered by chunk size groups of 8b moved to a contiguous representation through a parallel reduction tree
*/
static void __attribute__ ((noinline)) KerReductIO_Compact_SQ8(int *__restrict__ In, unsigned int Size, unsigned int CoreId, unsigned int ChunkCell)

{
	unsigned int U = gap_ncore()/2, Log2Core = gap_fl1(gap_ncore()), A = 2, B = 1;
	for (int k=0; k<Log2Core; k++) {
		if (CoreId<U) {
			signed char *__restrict__ OOs = ((signed char *)In+(A*CoreId+B)*ChunkCell);
			signed char *__restrict__ IIs = ((signed char *)In+((sizeof(int)/sizeof(signed char))*(A*CoreId+B))*ChunkCell);
			int *__restrict__ II = (int *) IIs;
			int *__restrict__ OO = (int *) OOs;
			for (int i=0;i<Size/8;i++) {
				int V0 = II[2*i], V1 = II[2*i+1];
				OO[2*i] = V0; OO[2*i+1] = V1;
			}
			for (int i=((Size/8)*8); i<Size; i++) OOs[i] = IIs[i];
		}
		U = U/2; A = A*2; B = B*2;
	}
	gap_waitbarrier(0);
}

#define B_CLR(x, bits)  ((x)&(~((1<<(bits))-1)))
static void KerReductIO_Compact_SQ8_1(char *__restrict__ To, char *__restrict__ From, int Size, int TotalSize)

{
        unsigned int CoreId = gap_coreid(), Chunk = ChunkSize(Size), First = Chunk*CoreId, Last = Min(First+Chunk, Size);
        unsigned int Iter = Max(0, Last-First);

	for (int i=Size; i<TotalSize; i+=Size) {
		From += Size*4; To += Size;

        	int *pFrom = (int *) (From+First), *pTo = (int *) (To+First);
        	for (int j=0; j<Iter/8; j++) {
                	int V0 = pFrom[2*j], V1 = pFrom[2*j+1];
                	pTo[2*j] = V0; pTo[2*j+1] = V1;
        	}
        	if (Iter & 0x4) *((int *) (To + First + B_CLR(Iter, 3))) = *((int *) (From + First + B_CLR(Iter, 3)));
        	if (Iter & 0x2) *((short int *) (To + First + B_CLR(Iter, 2))) = *((short int *) (From + First + B_CLR(Iter, 2)));
        	if (Iter & 0x1) *((signed char *) (To + First + Iter - 1)) = *((signed char *) (From + First + Iter - 1));
		gap_waitbarrier(0);
	}
}

/*
 * Input Scaling and reduction to 8b then channel cnetric activation, Out location != In location. Features are evaluated in parallel
*/
void KerParReduct_CC_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	int S = Arg->Feat;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ In = (int *__restrict__) Arg->In;
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Arg->W*Arg->H;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=First; c<Last; c++) KerReduct_ActivationScale1_SQ8(In+Size*c, Out+Size*c, Size, Scale[c], ScaleN[c], ACT_NONE, A0, B0, C0);
	gap_waitbarrier(0);
}


void KerParReduct_CC_ReLU_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	int S = Arg->Feat;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ In = (int *__restrict__) Arg->In;
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Arg->W*Arg->H;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=First; c<Last; c++) KerReduct_ActivationScale1_SQ8(In+Size*c, Out+Size*c, Size, Scale[c], ScaleN[c], ACT_RELU, A0, B0, C0);
	gap_waitbarrier(0);
}

void KerParReduct_CC_ReLUN_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	int S = Arg->Feat;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ In = (int *__restrict__) Arg->In;
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Arg->W*Arg->H;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=First; c<Last; c++) KerReduct_ActivationScale1_SQ8(In+Size*c, Out+Size*c, Size, Scale[c], ScaleN[c], ACT_RELUN, A0, B0, C0);
	gap_waitbarrier(0);

}

void KerParReduct_CC_ReLUM_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	int S = Arg->Feat;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ In = (int *__restrict__) Arg->In;
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Arg->W*Arg->H;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=First; c<Last; c++) KerReduct_ActivationScale1_SQ8(In+Size*c, Out+Size*c, Size, Scale[c], ScaleN[c], ACT_RELUM, A0, B0, C0);
	gap_waitbarrier(0);

}

void KerParReduct_CC_ReLUMN_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	int S = Arg->Feat;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ In = (int *__restrict__) Arg->In;
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Arg->W*Arg->H;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=First; c<Last; c++) KerReduct_ActivationScale1_SQ8(In+Size*c, Out+Size*c, Size, Scale[c], ScaleN[c], ACT_RELUMN, A0, B0, C0);
	gap_waitbarrier(0);

}

void KerParReduct_CC_HSigmoid_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	int S = Arg->Feat;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ In = (int *__restrict__) Arg->In;
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Arg->W*Arg->H;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=First; c<Last; c++) KerReduct_Activation_SQ8(In+Size*c, Out+Size*c, Size, Scale[c], ScaleN[c], ACT_HSIGMOID, ActScale, ActScaleN, A0, B0, C0);
	gap_waitbarrier(0);
}

void KerParReduct_CC_HSwish_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	int S = Arg->Feat;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ In = (int *__restrict__) Arg->In;
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Arg->W*Arg->H;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=First; c<Last; c++) KerReduct_Activation_SQ8(In+Size*c, Out+Size*c, Size, Scale[c], ScaleN[c], ACT_HSWISH, ActScale, ActScaleN, A0, B0, C0);
	gap_waitbarrier(0);
}

void KerParReduct_CC_LeakyReLU_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	int S = Arg->Feat;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ In = (int *__restrict__) Arg->In;
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Arg->W*Arg->H;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=First; c<Last; c++) KerReduct_Activation_SQ8(In+Size*c, Out+Size*c, Size, Scale[c], ScaleN[c], ACT_LEAKYRELU, ActScale, ActScaleN, A0, B0, C0);
	gap_waitbarrier(0);
}

void KerParReduct_CC_Sigmoid_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	int S = Arg->Feat;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ In = (int *__restrict__) Arg->In;
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Arg->W*Arg->H;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=First; c<Last; c++) KerReduct_Activation_SQ8(In+Size*c, Out+Size*c, Size, Scale[c], ScaleN[c], ACT_SIGMOID, ActScale, ActScaleN, A0, B0, C0);
	gap_waitbarrier(0);
}

/*
 * Input Scaling and reduction to 8b then channel centric activation, Out location = In location. Features are evaluated in parallel
*/
extern void DumpFeaturePlanes(char *Mess, int DataSize, void *Plane, unsigned int NPlanes, unsigned int W, unsigned int Wmax, unsigned int H, unsigned int Hmax);

void KerParReductIO_CC_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	unsigned int S = Arg->Feat;
	unsigned int Size = Arg->W*Arg->H;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ In = (int *__restrict__) Arg->In;
	signed char *__restrict__ Out = (signed char *__restrict__)(In+First*Size);
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	S = Size*Max(0, Last-First);
	for (int c=First; c<Last; Out+=Size, c++) KerReductIO_ActivationScale1_SQ8(Out, In+Size*c, Size, Scale[c], ScaleN[c], ACT_NONE, A0, B0, C0);
	gap_waitbarrier(0);
	// KerReductIO_Compact_SQ8(In, S, CoreId, ChunkCell*Size);
	KerReductIO_Compact_SQ8_1((signed char *__restrict__)In, (signed char *__restrict__)In, Size*ChunkCell, Size * Arg->Feat);
}

void KerParReductIO_CC_ReLU_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	unsigned int S = Arg->Feat;
	unsigned int Size = Arg->W*Arg->H;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ In = (int *__restrict__) Arg->In;
	signed char *__restrict__ Out = (signed char *__restrict__)(In+First*Size);
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	S = Size*Max(0, Last-First);
	for (int c=First; c<Last; Out+=Size, c++) KerReductIO_ActivationScale1_SQ8(Out, In+Size*c, Size, Scale[c], ScaleN[c], ACT_RELU, A0, B0, C0);
	gap_waitbarrier(0);
	// KerReductIO_Compact_SQ8(In, S, CoreId, ChunkCell*Size);
	KerReductIO_Compact_SQ8_1((signed char *__restrict__)In, (signed char *__restrict__)In, Size*ChunkCell, Size * Arg->Feat);
}

void KerParReductIO_CC_ReLUN_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	unsigned int S = Arg->Feat;
	unsigned int Size = Arg->W*Arg->H;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ In = (int *__restrict__) Arg->In;
	signed char *__restrict__ Out = (signed char *__restrict__)(In+First*Size);
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	S = Size*Max(0, Last-First);
	for (int c=First; c<Last; Out+=Size, c++) KerReductIO_ActivationScale1_SQ8(Out, In+Size*c, Size, Scale[c], ScaleN[c], ACT_RELUN, A0, B0, C0);
	gap_waitbarrier(0);
	// KerReductIO_Compact_SQ8(In, S, CoreId, ChunkCell*Size);
	KerReductIO_Compact_SQ8_1((signed char *__restrict__)In, (signed char *__restrict__)In, Size*ChunkCell, Size * Arg->Feat);
}

void KerParReductIO_CC_ReLUM_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	unsigned int S = Arg->Feat;
	unsigned int Size = Arg->W*Arg->H;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ In = (int *__restrict__) Arg->In;
	signed char *__restrict__ Out = (signed char *__restrict__)(In+First*Size);
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	S = Size*Max(0, Last-First);
	for (int c=First; c<Last; Out+=Size, c++) KerReductIO_ActivationScale1_SQ8(Out, In+Size*c, Size, Scale[c], ScaleN[c], ACT_RELUM, A0, B0, C0);
	gap_waitbarrier(0);
	// KerReductIO_Compact_SQ8(In, S, CoreId, ChunkCell*Size);
	KerReductIO_Compact_SQ8_1((signed char *__restrict__)In, (signed char *__restrict__)In, Size*ChunkCell, Size * Arg->Feat);
}

void KerParReductIO_CC_ReLUMN_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	unsigned int S = Arg->Feat;
	unsigned int Size = Arg->W*Arg->H;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ In = (int *__restrict__) Arg->In;
	signed char *__restrict__ Out = (signed char *__restrict__)(In+First*Size);
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	S = Size*Max(0, Last-First);
	for (int c=First; c<Last; Out+=Size, c++) KerReductIO_ActivationScale1_SQ8(Out, In+Size*c, Size, Scale[c], ScaleN[c], ACT_RELUMN, A0, B0, C0);
	gap_waitbarrier(0);
	// KerReductIO_Compact_SQ8(In, S, CoreId, ChunkCell*Size);
	KerReductIO_Compact_SQ8_1((signed char *__restrict__)In, (signed char *__restrict__)In, Size*ChunkCell, Size * Arg->Feat);
}

void KerParReductIO_CC_HSigmoid_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	unsigned int S = Arg->Feat;
	unsigned int Size = Arg->W*Arg->H;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ In = (int *__restrict__) Arg->In;
	signed char *__restrict__ Out = (signed char *__restrict__)(In+First*Size);
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	S = Size*Max(0, Last-First);
	for (int c=First; c<Last; Out+=Size, c++) KerReductIO_Activation_SQ8(Out, In+Size*c, Size, Scale[c], ScaleN[c], ACT_HSIGMOID, ActScale, ActScaleN, A0, B0, C0);
	gap_waitbarrier(0);
	// KerReductIO_Compact_SQ8(In, S, CoreId, ChunkCell*Size);
	KerReductIO_Compact_SQ8_1((signed char *__restrict__)In, (signed char *__restrict__)In, Size*ChunkCell, Size * Arg->Feat);
}

void KerParReductIO_CC_HSwish_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	unsigned int S = Arg->Feat;
	unsigned int Size = Arg->W*Arg->H;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ In = (int *__restrict__) Arg->In;
	signed char *__restrict__ Out = (signed char *__restrict__)(In+First*Size);
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	S = Size*Max(0, Last-First);
	for (int c=First; c<Last; Out+=Size, c++) KerReductIO_Activation_SQ8(Out, In+Size*c, Size, Scale[c], ScaleN[c], ACT_HSWISH, ActScale, ActScaleN, A0, B0, C0);
	gap_waitbarrier(0);
	// KerReductIO_Compact_SQ8(In, S, CoreId, ChunkCell*Size);
	KerReductIO_Compact_SQ8_1((signed char *__restrict__)In, (signed char *__restrict__)In, Size*ChunkCell, Size * Arg->Feat);
}

void KerParReductIO_CC_LeakyReLU_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	unsigned int S = Arg->Feat;
	unsigned int Size = Arg->W*Arg->H;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ In = (int *__restrict__) Arg->In;
	signed char *__restrict__ Out = (signed char *__restrict__)(In+First*Size);
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	S = Size*Max(0, Last-First);
	for (int c=First; c<Last; Out+=Size, c++) KerReductIO_Activation_SQ8(Out, In+Size*c, Size, Scale[c], ScaleN[c], ACT_LEAKYRELU, ActScale, ActScaleN, A0, B0, C0);
	gap_waitbarrier(0);
	// KerReductIO_Compact_SQ8(In, S, CoreId, ChunkCell*Size);
	KerReductIO_Compact_SQ8_1((signed char *__restrict__)In, (signed char *__restrict__)In, Size*ChunkCell, Size * Arg->Feat);
}

void KerParReductIO_CC_Sigmoid_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	unsigned int S = Arg->Feat;
	unsigned int Size = Arg->W*Arg->H;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ In = (int *__restrict__) Arg->In;
	signed char *__restrict__ Out = (signed char *__restrict__)(In+First*Size);
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	S = Size*Max(0, Last-First);
	for (int c=First; c<Last; Out+=Size, c++) KerReductIO_Activation_SQ8(Out, In+Size*c, Size, Scale[c], ScaleN[c], ACT_SIGMOID, ActScale, ActScaleN, A0, B0, C0);
	gap_waitbarrier(0);
	// KerReductIO_Compact_SQ8(In, S, CoreId, ChunkCell*Size);
	KerReductIO_Compact_SQ8_1((signed char *__restrict__)In, (signed char *__restrict__)In, Size*ChunkCell, Size * Arg->Feat);
}

/* Input Scaling and reduction to 8b then channel centric activation, Out location != In location. Features are evaluated one after the other in parallel */
void KerReduct_CC_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	unsigned int Feat = Arg->Feat;
	unsigned S = Arg->W*Arg->H;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ In = (int *__restrict__) Arg->In;
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=0; c<Feat; c++) KerReduct_ActivationScale1_SQ8(In+S*c+First, Out+S*c+First, Size, Scale[c], ScaleN[c], ACT_NONE, A0, B0, C0);
	gap_waitbarrier(0);
}

void KerReduct_CC_ReLU_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	unsigned int Feat = Arg->Feat;
	unsigned S = Arg->W*Arg->H;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ In = (int *__restrict__) Arg->In;
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=0; c<Feat; c++) KerReduct_ActivationScale1_SQ8(In+S*c+First, Out+S*c+First, Size, Scale[c], ScaleN[c], ACT_RELU, A0, B0, C0);
	gap_waitbarrier(0);
}

void KerReduct_CC_ReLUN_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	unsigned int Feat = Arg->Feat;
	unsigned S = Arg->W*Arg->H;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ In = (int *__restrict__) Arg->In;
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=0; c<Feat; c++) KerReduct_ActivationScale1_SQ8(In+S*c+First, Out+S*c+First, Size, Scale[c], ScaleN[c], ACT_RELUN, A0, B0, C0);
	gap_waitbarrier(0);
}

void KerReduct_CC_ReLUM_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	unsigned int Feat = Arg->Feat;
	unsigned S = Arg->W*Arg->H;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ In = (int *__restrict__) Arg->In;
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=0; c<Feat; c++) KerReduct_ActivationScale1_SQ8(In+S*c+First, Out+S*c+First, Size, Scale[c], ScaleN[c], ACT_RELUM, A0, B0, C0);
	gap_waitbarrier(0);
}

void KerReduct_CC_ReLUMN_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	unsigned int Feat = Arg->Feat;
	unsigned S = Arg->W*Arg->H;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ In = (int *__restrict__) Arg->In;
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=0; c<Feat; c++) KerReduct_ActivationScale1_SQ8(In+S*c+First, Out+S*c+First, Size, Scale[c], ScaleN[c], ACT_RELUMN, A0, B0, C0);
	gap_waitbarrier(0);
}

void KerReduct_CC_HSigmoid_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	unsigned int Feat = Arg->Feat;
	unsigned S = Arg->W*Arg->H;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ In = (int *__restrict__) Arg->In;
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=0; c<Feat; c++) KerReduct_Activation_SQ8(In+S*c+First, Out+S*c+First, Size, Scale[c], ScaleN[c], ACT_HSIGMOID, ActScale, ActScaleN, A0, B0, C0);
	gap_waitbarrier(0);
}

void KerReduct_CC_HSwish_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	unsigned int Feat = Arg->Feat;
	unsigned S = Arg->W*Arg->H;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ In = (int *__restrict__) Arg->In;
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=0; c<Feat; c++) KerReduct_Activation_SQ8(In+S*c+First, Out+S*c+First, Size, Scale[c], ScaleN[c], ACT_HSWISH, ActScale, ActScaleN, A0, B0, C0);
	gap_waitbarrier(0);
}

void KerReduct_CC_LeakyReLU_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	unsigned int Feat = Arg->Feat;
	unsigned S = Arg->W*Arg->H;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ In = (int *__restrict__) Arg->In;
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=0; c<Feat; c++) KerReduct_Activation_SQ8(In+S*c+First, Out+S*c+First, Size, Scale[c], ScaleN[c], ACT_LEAKYRELU, ActScale, ActScaleN, A0, B0, C0);
	gap_waitbarrier(0);
}

void KerReduct_CC_Sigmoid_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	unsigned int Feat = Arg->Feat;
	unsigned S = Arg->W*Arg->H;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ In = (int *__restrict__) Arg->In;
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=0; c<Feat; c++) KerReduct_Activation_SQ8(In+S*c+First, Out+S*c+First, Size, Scale[c], ScaleN[c], ACT_SIGMOID, ActScale, ActScaleN, A0, B0, C0);
	gap_waitbarrier(0);
}

/* Input Scaling and reduction to 8b then channel centric activation, Out location = In location. Features are evaluated one after the other in parallel */
void KerReductIO_CC_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	unsigned int Feat = Arg->Feat;
	unsigned int S = Arg->W*Arg->H;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ InOut = (int *__restrict__) Arg->In;
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=0; c<Feat; c++) {
		KerReductIO_ActivationScale1_SQ8((signed char *__restrict__)(InOut+S*c+First), InOut+S*c+First, Size, Scale[c], ScaleN[c], ACT_NONE, A0, B0, C0);
		gap_waitbarrier(0);
		// KerReductIO_Compact_SQ8(InOut+S*c, Size, CoreId, ChunkCell);
		KerReductIO_Compact_SQ8_1((signed char *__restrict__)InOut+S*c, (signed char *__restrict__)(InOut+S*c), ChunkCell, S);
	}

	// ChunkCell = ChunkSize(Feat); First = CoreId*ChunkCell; Last  = Min(First+ChunkCell, Feat); Size = S*Max(0, Last-First);
	// KerReductIO_Compact_SQ8(InOut, Size, CoreId, ChunkCell*Size);
}

void KerReductIO_CC_ReLU_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	unsigned int Feat = Arg->Feat;
	unsigned int S = Arg->W*Arg->H;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ InOut = (int *__restrict__) Arg->In;
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=0; c<Feat; c++) {
		KerReductIO_ActivationScale1_SQ8((signed char *__restrict__)(InOut+S*c+First), InOut+S*c+First, Size, Scale[c], ScaleN[c], ACT_RELU, A0, B0, C0);
		gap_waitbarrier(0);
		// KerReductIO_Compact_SQ8(InOut+S*c, Size, CoreId, ChunkCell);
		KerReductIO_Compact_SQ8_1((signed char *__restrict__)InOut+S*c, (signed char *__restrict__)(InOut+S*c), ChunkCell, S);
	}
	// ChunkCell = ChunkSize(Feat); First = CoreId*ChunkCell; Last  = Min(First+ChunkCell, Feat); Size = S*Max(0, Last-First);
	// KerReductIO_Compact_SQ8(InOut, Size, CoreId, ChunkCell*Size);
}

void KerReductIO_CC_ReLUN_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	unsigned int Feat = Arg->Feat;
	unsigned int S = Arg->W*Arg->H;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ InOut = (int *__restrict__) Arg->In;
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=0; c<Feat; c++) {
		KerReductIO_ActivationScale1_SQ8((signed char *__restrict__)(InOut+S*c+First), InOut+S*c+First, Size, Scale[c], ScaleN[c], ACT_RELUN, A0, B0, C0);
		gap_waitbarrier(0);
		// KerReductIO_Compact_SQ8(InOut+S*c, Size, CoreId, ChunkCell);
		KerReductIO_Compact_SQ8_1((signed char *__restrict__)InOut+S*c, (signed char *__restrict__)(InOut+S*c), ChunkCell, S);
	}
	// ChunkCell = ChunkSize(Feat); First = CoreId*ChunkCell; Last  = Min(First+ChunkCell, Feat); Size = S*Max(0, Last-First);
	// KerReductIO_Compact_SQ8(InOut, Size, CoreId, ChunkCell*Size);
}

void KerReductIO_CC_ReLUM_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	unsigned int Feat = Arg->Feat;
	unsigned int S = Arg->W*Arg->H;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ InOut = (int *__restrict__) Arg->In;
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=0; c<Feat; c++) {
		KerReductIO_ActivationScale1_SQ8((signed char *__restrict__)(InOut+S*c+First), InOut+S*c+First, Size, Scale[c], ScaleN[c], ACT_RELUM, A0, B0, C0);
		gap_waitbarrier(0);
		// KerReductIO_Compact_SQ8(InOut+S*c, Size, CoreId, ChunkCell);
		KerReductIO_Compact_SQ8_1((signed char *__restrict__)InOut+S*c, (signed char *__restrict__)(InOut+S*c), ChunkCell, S);
	}
	// ChunkCell = ChunkSize(Feat); First = CoreId*ChunkCell; Last  = Min(First+ChunkCell, Feat); Size = S*Max(0, Last-First);
	// KerReductIO_Compact_SQ8(InOut, Size, CoreId, ChunkCell*Size);
}

void KerReductIO_CC_ReLUMN_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	unsigned int Feat = Arg->Feat;
	unsigned int S = Arg->W*Arg->H;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ InOut = (int *__restrict__) Arg->In;
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=0; c<Feat; c++) {
		KerReductIO_ActivationScale1_SQ8((signed char *__restrict__)(InOut+S*c+First), InOut+S*c+First, Size, Scale[c], ScaleN[c], ACT_RELUMN, A0, B0, C0);
		gap_waitbarrier(0);
		// KerReductIO_Compact_SQ8(InOut+S*c, Size, CoreId, ChunkCell);
		KerReductIO_Compact_SQ8_1((signed char *__restrict__)InOut+S*c, (signed char *__restrict__)(InOut+S*c), ChunkCell, S);
	}
	// ChunkCell = ChunkSize(Feat); First = CoreId*ChunkCell; Last  = Min(First+ChunkCell, Feat); Size = S*Max(0, Last-First);
	// KerReductIO_Compact_SQ8(InOut, Size, CoreId, ChunkCell*Size);
}

void KerReductIO_CC_HSigmoid_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	unsigned int Feat = Arg->Feat;
	unsigned int S = Arg->W*Arg->H;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ InOut = (int *__restrict__) Arg->In;
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=0; c<Feat; c++) {
		KerReductIO_Activation_SQ8((signed char *__restrict__)(InOut+S*c+First), InOut+S*c+First, Size, Scale[c], ScaleN[c], ACT_HSIGMOID, ActScale, ActScaleN, A0, B0, C0);
		gap_waitbarrier(0);
		// KerReductIO_Compact_SQ8(InOut+S*c, Size, CoreId, ChunkCell);
		KerReductIO_Compact_SQ8_1((signed char *__restrict__)InOut+S*c, (signed char *__restrict__)(InOut+S*c), ChunkCell, S);
	}
	// ChunkCell = ChunkSize(Feat); First = CoreId*ChunkCell; Last  = Min(First+ChunkCell, Feat); Size = S*Max(0, Last-First);
	// KerReductIO_Compact_SQ8(InOut, Size, CoreId, ChunkCell*Size);
}

void KerReductIO_CC_HSwish_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	unsigned int Feat = Arg->Feat;
	unsigned int S = Arg->W*Arg->H;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ InOut = (int *__restrict__) Arg->In;
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=0; c<Feat; c++) {
		KerReductIO_Activation_SQ8((signed char *__restrict__)(InOut+S*c+First), InOut+S*c+First, Size, Scale[c], ScaleN[c], ACT_HSWISH, ActScale, ActScaleN, A0, B0, C0);
		gap_waitbarrier(0);
		// KerReductIO_Compact_SQ8(InOut+S*c, Size, CoreId, ChunkCell);
		KerReductIO_Compact_SQ8_1((signed char *__restrict__)InOut+S*c, (signed char *__restrict__)(InOut+S*c), ChunkCell, S);
	}
	// ChunkCell = ChunkSize(Feat); First = CoreId*ChunkCell; Last  = Min(First+ChunkCell, Feat); Size = S*Max(0, Last-First);
	// KerReductIO_Compact_SQ8(InOut, Size, CoreId, ChunkCell*Size);
}

void KerReductIO_CC_LeakyReLU_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	unsigned int Feat = Arg->Feat;
	unsigned int S = Arg->W*Arg->H;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ InOut = (int *__restrict__) Arg->In;
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=0; c<Feat; c++) {
		KerReductIO_Activation_SQ8((signed char *__restrict__)(InOut+S*c+First), InOut+S*c+First, Size, Scale[c], ScaleN[c], ACT_LEAKYRELU, ActScale, ActScaleN, A0, B0, C0);
		gap_waitbarrier(0);
		// KerReductIO_Compact_SQ8(InOut+S*c, Size, CoreId, ChunkCell);
		KerReductIO_Compact_SQ8_1((signed char *__restrict__)InOut+S*c, (signed char *__restrict__)(InOut+S*c), ChunkCell, S);
	}
	// ChunkCell = ChunkSize(Feat); First = CoreId*ChunkCell; Last  = Min(First+ChunkCell, Feat); Size = S*Max(0, Last-First);
	// KerReductIO_Compact_SQ8(InOut, Size, CoreId, ChunkCell*Size);
}

void KerReductIO_CC_Sigmoid_SQ8(KerConvLinReduct_SQ8_T *Arg)

{
	unsigned int Feat = Arg->Feat;
	unsigned int S = Arg->W*Arg->H;
	unsigned int CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	int * __restrict__ InOut = (int *__restrict__) Arg->In;
	unsigned char * __restrict__ Scale = (unsigned char *__restrict__) Arg->Scale;
	unsigned char * __restrict__ ScaleN = (unsigned char *__restrict__) Arg->ScaleN;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=0; c<Feat; c++) {
		KerReductIO_Activation_SQ8((signed char *__restrict__)(InOut+S*c+First), InOut+S*c+First, Size, Scale[c], ScaleN[c], ACT_SIGMOID, ActScale, ActScaleN, A0, B0, C0);
		gap_waitbarrier(0);
		// KerReductIO_Compact_SQ8(InOut+S*c, Size, CoreId, ChunkCell);
		KerReductIO_Compact_SQ8_1((signed char *__restrict__)InOut+S*c, (signed char *__restrict__)(InOut+S*c), ChunkCell, S);
	}
	// ChunkCell = ChunkSize(Feat); First = CoreId*ChunkCell; Last  = Min(First+ChunkCell, Feat); Size = S*Max(0, Last-First);
	// KerReductIO_Compact_SQ8(InOut, Size, CoreId, ChunkCell*Size);
}

/*
 * Standalone Scaled Activation, Features are evaluated in parallel
*/

void KerPar_ActNone_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	unsigned int Size = Arg->W*Arg->H;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	if (ActScale) for (int c=First; c<Last; c++) Ker_Activation_SQ8(In + Size*c, Out + Size*c, Size, ACT_NONE, ActScale, ActScaleN, A0, B0, C0);
	else for (int c=First; c<Last; c++) Ker_ActivationScale1_SQ8(In + Size*c, Out + Size*c, Size, ACT_NONE, A0, B0);
	gap_waitbarrier(0);
}

void KerPar_ReLU_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	unsigned int Size = Arg->W*Arg->H;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	if (ActScale) for (int c=First; c<Last; c++) Ker_Activation_SQ8(In + Size*c, Out + Size*c, Size, ACT_RELU, ActScale, ActScaleN, A0, B0, C0);
	else for (int c=First; c<Last; c++) Ker_ActivationScale1_SQ8(In + Size*c, Out + Size*c, Size, ACT_RELU, A0, B0);
	gap_waitbarrier(0);
}

void KerPar_ReLUN_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	unsigned int Size = Arg->W*Arg->H;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	if (ActScale) for (int c=First; c<Last; c++) Ker_Activation_SQ8(In + Size*c, Out + Size*c, Size, ACT_RELUN, ActScale, ActScaleN, A0, B0, C0);
	else for (int c=First; c<Last; c++) Ker_ActivationScale1_SQ8(In + Size*c, Out + Size*c, Size, ACT_RELUN, A0, B0);
	gap_waitbarrier(0);
}

void KerPar_ReLUM_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	unsigned int Size = Arg->W*Arg->H;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	if (ActScale) for (int c=First; c<Last; c++) Ker_Activation_SQ8(In + Size*c, Out + Size*c, Size, ACT_RELUM, ActScale, ActScaleN, A0, B0, C0);
	else for (int c=First; c<Last; c++) Ker_ActivationScale1_SQ8(In + Size*c, Out + Size*c, Size, ACT_RELUM, A0, B0);
	gap_waitbarrier(0);
}

void KerPar_ReLUMN_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	unsigned int Size = Arg->W*Arg->H;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	if (ActScale) for (int c=First; c<Last; c++) Ker_Activation_SQ8(In + Size*c, Out + Size*c, Size, ACT_RELUMN, ActScale, ActScaleN, A0, B0, C0);
	else for (int c=First; c<Last; c++) Ker_ActivationScale1_SQ8(In + Size*c, Out + Size*c, Size, ACT_RELUMN, A0, B0);
	gap_waitbarrier(0);
}

void KerPar_HSigmoid_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	unsigned int Size = Arg->W*Arg->H;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=First; c<Last; c++) Ker_Activation_SQ8(In + Size*c, Out + Size*c, Size, ACT_HSIGMOID, ActScale, ActScaleN, A0, B0, C0);
	gap_waitbarrier(0);
}

void KerPar_HSwish_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	unsigned int Size = Arg->W*Arg->H;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=First; c<Last; c++) Ker_Activation_SQ8(In + Size*c, Out + Size*c, Size, ACT_HSWISH, ActScale, ActScaleN, A0, B0, C0);
	gap_waitbarrier(0);
}

void KerPar_LeakyReLU_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	unsigned int Size = Arg->W*Arg->H;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=First; c<Last; c++) Ker_Activation_SQ8(In + Size*c, Out + Size*c, Size, ACT_LEAKYRELU, ActScale, ActScaleN, A0, B0, C0);
	gap_waitbarrier(0);
}

void KerPar_Sigmoid_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	unsigned int Size = Arg->W*Arg->H;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=First; c<Last; c++) Ker_Activation_SQ8(In + Size*c, Out + Size*c, Size, ACT_SIGMOID, ActScale, ActScaleN, A0, B0, C0);
	gap_waitbarrier(0);
}

/*
 * Standalone Scaled Activation, Features are evaluated one after the other in parallel
*/

void Ker_ActNone_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->W*Arg->H*Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];


	if (ActScale) Ker_Activation_SQ8(In+First, Out+First, Size, ACT_NONE, ActScale, ActScaleN, A0, B0, C0);
	else Ker_ActivationScale1_SQ8(In+First, Out+First, Size, ACT_NONE, A0, B0);
	gap_waitbarrier(0);
}

void Ker_ReLU_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->W*Arg->H*Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];


	if (ActScale) Ker_Activation_SQ8(In+First, Out+First, Size, ACT_RELU, ActScale, ActScaleN, A0, B0, C0);
	else Ker_ActivationScale1_SQ8(In+First, Out+First, Size, ACT_RELU, A0, B0);
	gap_waitbarrier(0);
}

void Ker_ReLUN_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->W*Arg->H*Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];


	if (ActScale) Ker_Activation_SQ8(In+First, Out+First, Size, ACT_RELUN, ActScale, ActScaleN, A0, B0, C0);
	else Ker_ActivationScale1_SQ8(In+First, Out+First, Size, ACT_RELUN, A0, B0);
	gap_waitbarrier(0);
}

void Ker_ReLUM_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->W*Arg->H*Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];


	if (ActScale) Ker_Activation_SQ8(In+First, Out+First, Size, ACT_RELUM, ActScale, ActScaleN, A0, B0, C0);
	else Ker_ActivationScale1_SQ8(In+First, Out+First, Size, ACT_RELUM, A0, B0);
	gap_waitbarrier(0);
}

void Ker_ReLUMN_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->W*Arg->H*Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];


	if (ActScale) Ker_Activation_SQ8(In+First, Out+First, Size, ACT_RELUMN, ActScale, ActScaleN, A0, B0, C0);
	else Ker_ActivationScale1_SQ8(In+First, Out+First, Size, ACT_RELUMN, A0, B0);
	gap_waitbarrier(0);
}

void Ker_HSigmoid_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->W*Arg->H*Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];


	Ker_Activation_SQ8(In+First, Out+First, Size, ACT_HSIGMOID, ActScale, ActScaleN, A0, B0, C0);
	gap_waitbarrier(0);
}

void Ker_HSwish_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->W*Arg->H*Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];


	Ker_Activation_SQ8(In+First, Out+First, Size, ACT_HSWISH, ActScale, ActScaleN, A0, B0, C0);
	gap_waitbarrier(0);
}

void Ker_LeakyReLU_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->W*Arg->H*Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];


	Ker_Activation_SQ8(In+First, Out+First, Size, ACT_LEAKYRELU, ActScale, ActScaleN, A0, B0, C0);
	gap_waitbarrier(0);
}

void Ker_Sigmoid_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->W*Arg->H*Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];


	Ker_Activation_SQ8(In+First, Out+First, Size, ACT_SIGMOID, ActScale, ActScaleN, A0, B0, C0);
	gap_waitbarrier(0);
}

/*
 * Standalone Scaled Activation, Features are evaluated in parallel
*/

void KerPar_ActNone_ScaleIn_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	unsigned int Size = Arg->W*Arg->H;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	unsigned int In1Scale = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALE], In1ScaleN = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	if (ActScale) for (int c=First; c<Last; c++) Ker_Activation_ScaleIn_SQ8(In + Size*c, Out + Size*c, In1Scale, In1ScaleN, Size, ACT_NONE, ActScale, ActScaleN, A0, B0, C0);
	else for (int c=First; c<Last; c++) Ker_ActivationScale1_ScaleIn_SQ8(In + Size*c, Out + Size*c, In1Scale, In1ScaleN, Size, ACT_NONE, A0, B0);
	gap_waitbarrier(0);
}

void KerPar_ReLU_ScaleIn_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	unsigned int Size = Arg->W*Arg->H;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	unsigned int In1Scale = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALE], In1ScaleN = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	if (ActScale) for (int c=First; c<Last; c++) Ker_Activation_ScaleIn_SQ8(In + Size*c, Out + Size*c, In1Scale, In1ScaleN, Size, ACT_RELU, ActScale, ActScaleN, A0, B0, C0);
	else for (int c=First; c<Last; c++) Ker_ActivationScale1_ScaleIn_SQ8(In + Size*c, Out + Size*c, In1Scale, In1ScaleN, Size, ACT_RELU, A0, B0);
	gap_waitbarrier(0);
}

void KerPar_ReLUN_ScaleIn_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	unsigned int Size = Arg->W*Arg->H;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	unsigned int In1Scale = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALE], In1ScaleN = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	if (ActScale) for (int c=First; c<Last; c++) Ker_Activation_ScaleIn_SQ8(In + Size*c, Out + Size*c, In1Scale, In1ScaleN, Size, ACT_RELUN, ActScale, ActScaleN, A0, B0, C0);
	else for (int c=First; c<Last; c++) Ker_ActivationScale1_ScaleIn_SQ8(In + Size*c, Out + Size*c, In1Scale, In1ScaleN, Size, ACT_RELUN, A0, B0);
	gap_waitbarrier(0);
}

void KerPar_ReLUM_ScaleIn_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	unsigned int Size = Arg->W*Arg->H;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	unsigned int In1Scale = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALE], In1ScaleN = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	if (ActScale) for (int c=First; c<Last; c++) Ker_Activation_ScaleIn_SQ8(In + Size*c, Out + Size*c, In1Scale, In1ScaleN, Size, ACT_RELUM, ActScale, ActScaleN, A0, B0, C0);
	else for (int c=First; c<Last; c++) Ker_ActivationScale1_ScaleIn_SQ8(In + Size*c, Out + Size*c, In1Scale, In1ScaleN, Size, ACT_RELUM, A0, B0);
	gap_waitbarrier(0);
}

void KerPar_ReLUMN_ScaleIn_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	unsigned int Size = Arg->W*Arg->H;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	unsigned int In1Scale = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALE], In1ScaleN = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	if (ActScale) for (int c=First; c<Last; c++) Ker_Activation_ScaleIn_SQ8(In + Size*c, Out + Size*c, In1Scale, In1ScaleN, Size, ACT_RELUMN, ActScale, ActScaleN, A0, B0, C0);
	else for (int c=First; c<Last; c++) Ker_ActivationScale1_ScaleIn_SQ8(In + Size*c, Out + Size*c, In1Scale, In1ScaleN, Size, ACT_RELUMN, A0, B0);
	gap_waitbarrier(0);
}

void KerPar_HSigmoid_ScaleIn_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	unsigned int Size = Arg->W*Arg->H;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	unsigned int In1Scale = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALE], In1ScaleN = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=First; c<Last; c++) Ker_Activation_ScaleIn_SQ8(In + Size*c, Out + Size*c, In1Scale, In1ScaleN, Size, ACT_HSIGMOID, ActScale, ActScaleN, A0, B0, C0);
	gap_waitbarrier(0);
}

void KerPar_HSwish_ScaleIn_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	unsigned int Size = Arg->W*Arg->H;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	unsigned int In1Scale = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALE], In1ScaleN = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=First; c<Last; c++) Ker_Activation_ScaleIn_SQ8(In + Size*c, Out + Size*c, In1Scale, In1ScaleN, Size, ACT_HSWISH, ActScale, ActScaleN, A0, B0, C0);
	gap_waitbarrier(0);
}

void KerPar_LeakyReLU_ScaleIn_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	unsigned int Size = Arg->W*Arg->H;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	unsigned int In1Scale = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALE], In1ScaleN = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=First; c<Last; c++) Ker_Activation_ScaleIn_SQ8(In + Size*c, Out + Size*c, In1Scale, In1ScaleN, Size, ACT_LEAKYRELU, ActScale, ActScaleN, A0, B0, C0);
	gap_waitbarrier(0);
}

void KerPar_Sigmoid_ScaleIn_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	unsigned int Size = Arg->W*Arg->H;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	unsigned int In1Scale = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALE], In1ScaleN = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];

	for (int c=First; c<Last; c++) Ker_Activation_ScaleIn_SQ8(In + Size*c, Out + Size*c, In1Scale, In1ScaleN, Size, ACT_SIGMOID, ActScale, ActScaleN, A0, B0, C0);
	gap_waitbarrier(0);
}

/*
 * Standalone Scaled Activation with Extra Scale before activation, Features are evaluated one after the other in parallel
*/

void Ker_ActNone_ScaleIn_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->W*Arg->H*Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	unsigned int In1Scale = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALE], In1ScaleN = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];


	if (ActScale) Ker_Activation_ScaleIn_SQ8(In+First, Out+First, In1Scale, In1ScaleN, Size, ACT_NONE, ActScale, ActScaleN, A0, B0, C0);
	else Ker_ActivationScale1_ScaleIn_SQ8(In+First, Out+First, In1Scale, In1ScaleN, Size, ACT_NONE, A0, B0);
	gap_waitbarrier(0);
}

void Ker_ReLU_ScaleIn_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->W*Arg->H*Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	unsigned int In1Scale = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALE], In1ScaleN = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];


	if (ActScale) Ker_Activation_ScaleIn_SQ8(In+First, Out+First, In1Scale, In1ScaleN, Size, ACT_RELU, ActScale, ActScaleN, A0, B0, C0);
	else Ker_ActivationScale1_ScaleIn_SQ8(In+First, Out+First, In1Scale, In1ScaleN, Size, ACT_RELU, A0, B0);
	gap_waitbarrier(0);
}

void Ker_ReLUN_ScaleIn_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->W*Arg->H*Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	unsigned int In1Scale = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALE], In1ScaleN = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];


	if (ActScale) Ker_Activation_ScaleIn_SQ8(In+First, Out+First, In1Scale, In1ScaleN, Size, ACT_RELUN, ActScale, ActScaleN, A0, B0, C0);
	else Ker_ActivationScale1_ScaleIn_SQ8(In+First, Out+First, In1Scale, In1ScaleN, Size, ACT_RELUN, A0, B0);
	gap_waitbarrier(0);
}

void Ker_ReLUM_ScaleIn_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->W*Arg->H*Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	unsigned int In1Scale = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALE], In1ScaleN = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];


	if (ActScale) Ker_Activation_ScaleIn_SQ8(In+First, Out+First, In1Scale, In1ScaleN, Size, ACT_RELUM, ActScale, ActScaleN, A0, B0, C0);
	else Ker_ActivationScale1_ScaleIn_SQ8(In+First, Out+First, In1Scale, In1ScaleN, Size, ACT_RELUM, A0, B0);
	gap_waitbarrier(0);
}

void Ker_ReLUMN_ScaleIn_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->W*Arg->H*Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	unsigned int In1Scale = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALE], In1ScaleN = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];


	if (ActScale) Ker_Activation_ScaleIn_SQ8(In+First, Out+First, In1Scale, In1ScaleN, Size, ACT_RELUMN, ActScale, ActScaleN, A0, B0, C0);
	else Ker_ActivationScale1_ScaleIn_SQ8(In+First, Out+First, In1Scale, In1ScaleN, Size, ACT_RELUMN, A0, B0);
	gap_waitbarrier(0);
}

void Ker_HSigmoid_ScaleIn_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->W*Arg->H*Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	unsigned int In1Scale = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALE], In1ScaleN = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];


	Ker_Activation_ScaleIn_SQ8(In+First, Out+First, In1Scale, In1ScaleN, Size, ACT_HSIGMOID, ActScale, ActScaleN, A0, B0, C0);
	gap_waitbarrier(0);
}

void Ker_HSwish_ScaleIn_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->W*Arg->H*Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	unsigned int In1Scale = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALE], In1ScaleN = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];


	Ker_Activation_ScaleIn_SQ8(In+First, Out+First, In1Scale, In1ScaleN, Size, ACT_HSWISH, ActScale, ActScaleN, A0, B0, C0);
	gap_waitbarrier(0);
}

void Ker_LeakyReLU_ScaleIn_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->W*Arg->H*Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	unsigned int In1Scale = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALE], In1ScaleN = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];


	Ker_Activation_ScaleIn_SQ8(In+First, Out+First, In1Scale, In1ScaleN, Size, ACT_LEAKYRELU, ActScale, ActScaleN, A0, B0, C0);
	gap_waitbarrier(0);
}

void Ker_Sigmoid_ScaleIn_SQ8(KerActivation_SQ8_T *Arg)

{
	unsigned int S = Arg->W*Arg->H*Arg->Feat, CoreId = gap_coreid(), ChunkCell = ChunkSize(S), First = CoreId*ChunkCell, Last  = Min(First+ChunkCell, S);
	signed char * __restrict__ In = (signed char *__restrict__) Arg->In;
	signed char * __restrict__ Out = (signed char *__restrict__) Arg->Out;
	signed char * __restrict__ Infos = (signed char *__restrict__) Arg->Infos;
	unsigned int Size = Max(0, Last-First);
	unsigned int ActScale = ((unsigned char *)Infos)[AT_INF_ACTSCALE], ActScaleN = ((unsigned char *)Infos)[AT_INF_ACTSCALEN];
	unsigned int In1Scale = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALE], In1ScaleN = ((unsigned char *)Arg->Infos)[AT_INF_IN1SCALEN];
	int A0 = Infos[AT_INF_A0], B0 = Infos[AT_INF_B0], C0 = Infos[AT_INF_C0];


	Ker_Activation_ScaleIn_SQ8(In+First, Out+First, In1Scale, In1ScaleN, Size, ACT_SIGMOID, ActScale, ActScaleN, A0, B0, C0);
	gap_waitbarrier(0);
}
#pragma GCC diagnostic pop
