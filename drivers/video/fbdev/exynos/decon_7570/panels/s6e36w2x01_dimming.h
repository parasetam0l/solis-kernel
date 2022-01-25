#ifndef __S6E36W2X01_SMART_DIMMING_H__
#define __S6E36W2X01_SMART_DIMMING_H__

/* color index */
enum {
	CI_RED,
	CI_GREEN,
	CI_BLUE,
	CI_MAX
};

enum {
	V1,
	V7,
	V11,
	V23,
	V35,
	V51,
	V87,
	V151,
	V203,
	V255,
	VMAX
};

#define VT						0
#define NUM_VREF				VMAX	/*10*/
//#define MULTIPLE_VREGOUT		6144	/*6.0 * 1024 = 6144*/
#define MULTIPLY_VALUE			1000
#define MULTIPLE_VREGOUT		6200
#define DOUBLE_MULTIPLE_VREGOUT	6200000

#define MAX_GAMMA				600		/*360cd*/
#define MAX_GRADATION			256

#define TBL_INDEX_V1			1
#define TBL_INDEX_V7			7
#define TBL_INDEX_V11			11
#define TBL_INDEX_V23			23
#define TBL_INDEX_V35			35
#define TBL_INDEX_V51			51
#define TBL_INDEX_V87			87
#define TBL_INDEX_V151			151
#define TBL_INDEX_V203			203
#define TBL_INDEX_V255			255

#define GAMMA_CNT			36
#define MAX_GAMMA_CNT			74
#define OLED_CMD_GAMMA			0xCA

static const int  candela_tbl[MAX_GAMMA_CNT] = {
	10, 11, 12, 13, 14, 15, 16, 17, 19, 20,
	21, 22, 24, 25, 27, 29, 30, 32, 34, 37,
	39, 41, 44, 47, 50, 53, 56, 60, 64, 68,
	72, 77, 82, 87, 93, 98, 105, 111, 119, 126,
	134, 143, 152, 162, 172, 183, 195, 207, 220, 234,
	249, 265, 282, 300, 316, 333, 350, 357, 365, 372,
	380, 387, 395, 403, 412, 420, 443, 465, 488, 510,
	533, 555, 578, 600
};

struct v_constant {
	int nu;
	int de;
};

static const unsigned int vref_index[NUM_VREF] = {
	TBL_INDEX_V1,
	TBL_INDEX_V7,
	TBL_INDEX_V11,
	TBL_INDEX_V23,
	TBL_INDEX_V35,
	TBL_INDEX_V51,
	TBL_INDEX_V87,
	TBL_INDEX_V151,
	TBL_INDEX_V203,
	TBL_INDEX_V255,
};

struct smart_dimming {
	int gamma[NUM_VREF][CI_MAX];
	int mtp[NUM_VREF][CI_MAX];
	int volt[MAX_GRADATION][CI_MAX];
	int volt_vt[CI_MAX];
	int look_volt[NUM_VREF][CI_MAX];
};

struct dim_data {
	int t_gamma[NUM_VREF][CI_MAX];
	int mtp[NUM_VREF][CI_MAX];
	int volt[MAX_GRADATION][CI_MAX];
	int volt_vt[CI_MAX];
	int vt_mtp[CI_MAX];
	int look_volt[NUM_VREF][CI_MAX];
};

void panel_read_gamma(struct smart_dimming *dimming, const unsigned char *data);
int panel_generate_volt_tbl(struct smart_dimming *dimming);
int panel_get_gamma(struct smart_dimming *dimming, int candela,
							unsigned char *target);
#endif /* __SMART_DIMMING_H__*/
