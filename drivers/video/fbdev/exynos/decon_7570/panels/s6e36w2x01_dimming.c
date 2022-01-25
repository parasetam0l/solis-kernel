/* drivers/video/fbdev/exynos/decon_7570/panels/s6e36w2x01_dimming.c
 *
 * MIPI-DSI based S6E36W1X01 AMOLED panel driver.
 *
 * Taeheon Kim, <th908.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include "s6e36w2x01_dimming.h"
#include "s6e36w2x01_param.h"

/*#define SMART_DIMMING_DEBUG*/
#define RGB_COMPENSATION 33

static unsigned int ref_gamma[NUM_VREF][CI_MAX] = {
	{0x00, 0x00, 0x00},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x80, 0x80, 0x80},
	{0x100, 0x100, 0x100},
};

const static int vreg_element_max[NUM_VREF] = {
	0xff,
	0xff,
	0xff,
	0xff,
	0xff,
	0xff,
	0xff,
	0xff,
	0xff,
	0x1ff,
};

const static struct v_constant fix_const[NUM_VREF] = {
	{.nu = 0,	.de = 256},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 64,	.de = 320},
	{.nu = 129,	.de = 860},
};

static const unsigned int vt_trans_volt[16] = {
	6200000, 6113488, 6026976, 5940465, 5853953, 5767441, 5680930, 5594418,
	5507906, 5421395, 5205116, 5133023, 5060930, 4988837, 4916744, 4859069
};

const static short v0_trans_volt[256] = {
	0, 4, 8, 12, 16, 20, 24, 28,
	32, 36, 40, 44, 48, 52, 56, 60,
	64, 68, 72, 76, 80, 84, 88, 92,
	96, 100, 104, 108, 112, 116, 120, 124,
	128, 132, 136, 140, 144, 148, 152, 156,
	160, 164, 168, 172, 176, 180, 184, 188,
	192, 196, 200, 204, 208, 212, 216, 220,
	224, 228, 232, 236, 240, 244, 248, 252,
	256, 260, 264, 268, 272, 276, 280, 284,
	288, 292, 296, 300, 304, 308, 312, 316,
	320, 324, 328, 332, 336, 340, 344, 348,
	352, 356, 360, 364, 368, 372, 376, 380,
	384, 388, 392, 396, 400, 404, 408, 412,
	416, 420, 424, 428, 432, 436, 440, 444,
	448, 452, 456, 460, 464, 468, 472, 476,
	480, 484, 488, 492, 496, 500, 504, 508,
	512, 516, 520, 524, 528, 532, 536, 540,
	544, 548, 552, 556, 560, 564, 568, 572,
	576, 580, 584, 588, 592, 596, 600, 604,
	608, 612, 616, 620, 624, 628, 632, 636,
	640, 644, 648, 652, 656, 660, 664, 668,
	672, 676, 680, 684, 688, 692, 696, 700,
	704, 708, 712, 716, 720, 724, 728, 732,
	736, 740, 744, 748, 752, 756, 760, 764,
	768, 772, 776, 780, 784, 788, 792, 796,
	800, 804, 808, 812, 816, 820, 824, 828,
	832, 836, 840, 844, 848, 852, 856, 860,
	864, 868, 872, 876, 880, 884, 888, 892,
	896, 900, 904, 908, 912, 916, 920, 924,
	928, 932, 936, 940, 944, 948, 952, 956,
	960, 964, 968, 972, 976, 980, 984, 988,
	992, 996, 1000, 1004, 1008, 1012, 1016, 1020
};

static const int gamma_tbl[256] = {
	0, 7, 31, 75, 138, 224, 331, 461,
	614, 791, 992, 1218, 1468, 1744, 2045, 2372,
	2725, 3105, 3511, 3943, 4403, 4890, 5404, 5946,
	6516, 7114, 7740, 8394, 9077, 9788, 10528, 11297,
	12095, 12922, 13779, 14665, 15581, 16526, 17501, 18507,
	19542, 20607, 21703, 22829, 23986, 25174, 26392, 27641,
	28921, 30232, 31574, 32947, 34352, 35788, 37255, 38754,
	40285, 41847, 43442, 45068, 46727, 48417, 50140, 51894,
	53682, 55501, 57353, 59238, 61155, 63105, 65088, 67103,
	69152, 71233, 73348, 75495, 77676, 79890, 82138, 84418,
	86733, 89080, 91461, 93876, 96325, 98807, 101324, 103874,
	106458, 109075, 111727, 114414, 117134, 119888, 122677, 125500,
	128358, 131250, 134176, 137137, 140132, 143163, 146227, 149327,
	152462, 155631, 158835, 162074, 165348, 168657, 172002, 175381,
	178796, 182246, 185731, 189251, 192807, 196398, 200025, 203688,
	207385, 211119, 214888, 218693, 222533, 226410, 230322, 234270,
	238254, 242274, 246330, 250422, 254550, 258714, 262914, 267151,
	271423, 275732, 280078, 284459, 288878, 293332, 297823, 302351,
	306915, 311516, 316153, 320827, 325538, 330285, 335069, 339890,
	344748, 349643, 354575, 359544, 364549, 369592, 374672, 379789,
	384943, 390134, 395363, 400629, 405932, 411272, 416650, 422065,
	427517, 433007, 438534, 444099, 449702, 455342, 461020, 466735,
	472488, 478279, 484107, 489973, 495878, 501819, 507799, 513817,
	519872, 525966, 532098, 538267, 544475, 550721, 557005, 563327,
	569687, 576085, 582522, 588997, 595510, 602062, 608651, 615280,
	621946, 628652, 635395, 642177, 648998, 655857, 662755, 669691,
	676667, 683680, 690733, 697824, 704954, 712122, 719330, 726576,
	733861, 741186, 748549, 755951, 763391, 770871, 778390, 785948,
	793545, 801182, 808857, 816571, 824325, 832118, 839950, 847821,
	855732, 863682, 871671, 879700, 887768, 895875, 904022, 912208,
	920434, 928699, 937004, 945349, 953733, 962156, 970619, 979122,
	987665, 996247, 1004869, 1013531, 1022233, 1030974, 1039755, 1048576
};

const static int gamma_multi_tbl[256] = {
	0, 3, 14, 34, 64, 105, 157, 220, 296, 383,
	483, 595, 721, 860, 1012, 1178, 1358, 1551, 1759, 1982,
	2218, 2470, 2736, 3017, 3313, 3624, 3951, 4293, 4651, 5024,
	5413, 5818, 6239, 6676, 7129, 7598, 8084, 8586, 9105, 9641,
	10193, 10762, 11348, 11951, 12571, 13208, 13862, 14534, 15223, 15929,
	16653, 17395, 18154, 18931, 19726, 20538, 21369, 22217, 23084, 23968,
	24871, 25792, 26732, 27689, 28665, 29660, 30673, 31705, 32755, 33824,
	34912, 36019, 37144, 38289, 39452, 40635, 41836, 43057, 44297, 45556,
	46834, 48132, 49449, 50785, 52141, 53516, 54911, 56326, 57760, 59214,
	60687, 62180, 63694, 65227, 66780, 68353, 69945, 71558, 73191, 74845,
	76518, 78211, 79925, 81659, 83413, 85188, 86983, 88799, 90635, 92491,
	94368, 96266, 98184, 100123, 102083, 104063, 106065, 108087, 110129, 112193,
	114278, 116383, 118510, 120657, 122826, 125016, 127227, 129459, 131712, 133986,
	136282, 138599, 140937, 143297, 145678, 148080, 150504, 152950, 155416, 157905,
	160415, 162946, 165500, 168075, 170671, 173290, 175930, 178592, 181275, 183981,
	186708, 189458, 192229, 195022, 197837, 200675, 203534, 206415, 209319, 212245,
	215192, 218162, 221155, 224169, 227206, 230265, 233346, 236450, 239576, 242724,
	245895, 249089, 252305, 255543, 258804, 262088, 265394, 268723, 272074, 275448,
	278845, 282264, 285706, 289171, 292659, 296170, 299703, 303259, 306839, 310441,
	314066, 317714, 321385, 325079, 328796, 332536, 336299, 340086, 343895, 347728,
	351584, 355463, 359365, 363291, 367239, 371211, 375207, 379226, 383268, 387333,
	391422, 395534, 399670, 403829, 408012, 412218, 416448, 420702, 424979, 429279,
	433603, 437951, 442323, 446718, 451137, 455580, 460046, 464536, 469050, 473588,
	478150, 482735, 487345, 491978, 496635, 501317, 506022, 510751, 515504, 520281,
	525083, 529908, 534757, 539631, 544528, 549450, 554396, 559366, 564360, 569379,
	574422, 579489, 584580, 589696, 594836, 600000
};

const static unsigned char lookup_tbl[601] = {
	0, 14, 19, 23, 26, 29, 31, 34, 36, 38,
	40, 41, 43, 45, 46, 48, 49, 50, 52, 53,
	54, 56, 57, 58, 59, 60, 61, 62, 63, 64,
	65, 66, 67, 68, 69, 70, 71, 72, 73, 74,
	74, 75, 76, 77, 78, 79, 79, 80, 81, 82,
	82, 83, 84, 85, 85, 86, 87, 87, 88, 89,
	90, 90, 91, 92, 92, 93, 93, 94, 95, 95,
	96, 97, 97, 98, 98, 99, 100, 100, 101, 101,
	102, 103, 103, 104, 104, 105, 105, 106, 107, 107,
	108, 108, 109, 109, 110, 110, 111, 111, 112, 112,
	113, 113, 114, 114, 115, 115, 116, 116, 117, 117,
	118, 118, 119, 119, 120, 120, 121, 121, 122, 122,
	123, 123, 124, 124, 125, 125, 125, 126, 126, 127,
	127, 128, 128, 129, 129, 129, 130, 130, 131, 131,
	132, 132, 132, 133, 133, 134, 134, 135, 135, 135,
	136, 136, 137, 137, 137, 138, 138, 139, 139, 139,
	140, 140, 141, 141, 141, 142, 142, 143, 143, 143,
	144, 144, 145, 145, 145, 146, 146, 146, 147, 147,
	148, 148, 148, 149, 149, 149, 150, 150, 150, 151,
	151, 152, 152, 152, 153, 153, 153, 154, 154, 154,
	155, 155, 155, 156, 156, 157, 157, 157, 158, 158,
	158, 159, 159, 159, 160, 160, 160, 161, 161, 161,
	162, 162, 162, 163, 163, 163, 164, 164, 164, 165,
	165, 165, 166, 166, 166, 167, 167, 167, 167, 168,
	168, 168, 169, 169, 169, 170, 170, 170, 171, 171,
	171, 172, 172, 172, 173, 173, 173, 173, 174, 174,
	174, 175, 175, 175, 176, 176, 176, 176, 177, 177,
	177, 178, 178, 178, 179, 179, 179, 179, 180, 180,
	180, 181, 181, 181, 182, 182, 182, 182, 183, 183,
	183, 184, 184, 184, 184, 185, 185, 185, 186, 186,
	186, 186, 187, 187, 187, 187, 188, 188, 188, 189,
	189, 189, 189, 190, 190, 190, 191, 191, 191, 191,
	192, 192, 192, 192, 193, 193, 193, 194, 194, 194,
	194, 195, 195, 195, 195, 196, 196, 196, 196, 197,
	197, 197, 198, 198, 198, 198, 199, 199, 199, 199,
	200, 200, 200, 200, 201, 201, 201, 201, 202, 202,
	202, 202, 203, 203, 203, 203, 204, 204, 204, 204,
	205, 205, 205, 205, 206, 206, 206, 206, 207, 207,
	207, 207, 208, 208, 208, 208, 209, 209, 209, 209,
	210, 210, 210, 210, 211, 211, 211, 211, 212, 212,
	212, 212, 213, 213, 213, 213, 214, 214, 214, 214,
	214, 215, 215, 215, 215, 216, 216, 216, 216, 217,
	217, 217, 217, 218, 218, 218, 218, 218, 219, 219,
	219, 219, 220, 220, 220, 220, 221, 221, 221, 221,
	221, 222, 222, 222, 222, 223, 223, 223, 223, 224,
	224, 224, 224, 224, 225, 225, 225, 225, 226, 226,
	226, 226, 226, 227, 227, 227, 227, 228, 228, 228,
	228, 228, 229, 229, 229, 229, 230, 230, 230, 230,
	230, 231, 231, 231, 231, 231, 232, 232, 232, 232,
	233, 233, 233, 233, 233, 234, 234, 234, 234, 235,
	235, 235, 235, 235, 236, 236, 236, 236, 236, 237,
	237, 237, 237, 237, 238, 238, 238, 238, 239, 239,
	239, 239, 239, 240, 240, 240, 240, 240, 241, 241,
	241, 241, 241, 242, 242, 242, 242, 242, 243, 243,
	243, 243, 243, 244, 244, 244, 244, 245, 245, 245,
	245, 245, 246, 246, 246, 246, 246, 247, 247, 247,
	247, 247, 248, 248, 248, 248, 248, 249, 249, 249,
	249, 249, 250, 250, 250, 250, 250, 251, 251, 251,
	251, 251, 251, 252, 252, 252, 252, 252, 253, 253,
	253, 253, 253, 254, 254, 254, 254, 254, 255, 255,
	255
};

static const unsigned int v255_trans_volt [513] = {
	5270000, 5262791, 5255581, 5248372, 5241163, 5233953, 5226744, 5219535, 5212326, 5205116, 
	5197907, 5190698, 5183488, 5176279, 5169070, 5161860, 5154651, 5147442, 5140233, 5133023,
	5125814, 5118605, 5111395, 5104186, 5096977, 5089767, 5082558, 5075349, 5068140, 5060930,
	5053721, 5046512, 5039302, 5032093, 5024884, 5017674, 5010465, 5003256, 4996047, 4988837,
	4981628, 4974419, 4967209, 4960000, 4952791, 4945581, 4938372, 4931163, 4923953, 4916744,
	4909535, 4902326, 4895116, 4887907, 4880698, 4873488, 4866279, 4859070, 4851860, 4844651,
	4837442, 4830233, 4823023, 4815814, 4808605, 4801395, 4794186, 4786977, 4779767, 4772558,
	4765349, 4758140, 4750930, 4743721, 4736512, 4729302, 4722093, 4714884, 4707674, 4700465,
	4693256, 4686047, 4678837, 4671628, 4664419, 4657209, 4650000, 4642791, 4635581, 4628372,
	4621163, 4613953, 4606744, 4599535, 4592326, 4585116, 4577907, 4570698, 4563488, 4556279,
	4549070, 4541860, 4534651, 4527442, 4520233, 4513023, 4505814, 4498605, 4491395, 4484186,
	4476977, 4469767, 4462558, 4455349, 4448140, 4440930, 4433721, 4426512, 4419302, 4412093,
	4404884, 4397674, 4390465, 4383256, 4376047, 4368837, 4361628, 4354419, 4347209, 4340000,
	4332791, 4325581, 4318372, 4311163, 4303953, 4296744, 4289535, 4282326, 4275116, 4267907,
	4260698, 4253488, 4246279, 4239070, 4231860, 4224651, 4217442, 4210233, 4203023, 4195814,
	4188605, 4181395, 4174186, 4166977, 4159767, 4152558, 4145349, 4138140, 4130930, 4123721,
	4116512, 4109302, 4102093, 4094884, 4087674, 4080465, 4073256, 4066047, 4058837, 4051628,
	4044419, 4037209, 4030000, 4022791, 4015581, 4008372, 4001163, 3993953, 3986744, 3979535,
	3972326, 3965116, 3957907, 3950698, 3943488, 3936279, 3929070, 3921860, 3914651, 3907442,
	3900233, 3893023, 3885814, 3878605, 3871395, 3864186, 3856977, 3849767, 3842558, 3835349,
	3828140, 3820930, 3813721, 3806512, 3799302, 3792093, 3784884, 3777674, 3770465, 3763256,
	3756047, 3748837, 3741628, 3734419, 3727209, 3720000, 3712791, 3705581, 3698372, 3691163,
	3683953, 3676744, 3669535, 3662326, 3655116, 3647907, 3640698, 3633488, 3626279, 3619070,
	3611860, 3604651, 3597442, 3590233, 3583023, 3575814, 3568605, 3561395, 3554186, 3546977,
	3539767, 3532558, 3525349, 3518140, 3510930, 3503721, 3496512, 3489302, 3482093, 3474884,
	3467674, 3460465, 3453256, 3446047, 3438837, 3431628, 3424419, 3417209, 3410000, 3402791,
	3395581, 3388372, 3381163, 3373953, 3366744, 3359535, 3352326, 3345116, 3337907, 3330698,
	3323488, 3316279, 3309070, 3301860, 3294651, 3287442, 3280233, 3273023, 3265814, 3258605,
	3251395, 3244186, 3236977, 3229767, 3222558, 3215349, 3208140, 3200930, 3193721, 3186512,
	3179302, 3172093, 3164884, 3157674, 3150465, 3143256, 3136047, 3128837, 3121628, 3114419,
	3107209, 3100000, 3092791, 3085581, 3078372, 3071163, 3063953, 3056744, 3049535, 3042326,
	3035116, 3027907, 3020698, 3013488, 3006279, 2999070, 2991860, 2984651, 2977442, 2970233,
	2963023, 2955814, 2948605, 2941395, 2934186, 2926977, 2919767, 2912558, 2905349, 2898140,
	2890930, 2883721, 2876512, 2869302, 2862093, 2854884, 2847674, 2840465, 2833256, 2826047,
	2818837, 2811628, 2804419, 2797209, 2790000, 2782791, 2775581, 2768372, 2761163, 2753953,
	2746744, 2739535, 2732326, 2725116, 2717907, 2710698, 2703488, 2696279, 2689070, 2681860,
	2674651, 2667442, 2660233, 2653023, 2645814, 2638605, 2631395, 2624186, 2616977, 2609767,
	2602558, 2595349, 2588140, 2580930, 2573721, 2566512, 2559302, 2552093, 2544884, 2537674,
	2530465, 2523256, 2516047, 2508837, 2501628, 2494419, 2487209, 2480000, 2472791, 2465581,
	2458372, 2451163, 2443953, 2436744, 2429535, 2422326, 2415116, 2407907, 2400698, 2393488,
	2386279, 2379070, 2371860, 2364651, 2357442, 2350233, 2343023, 2335814, 2328605, 2321395,
	2314186, 2306977, 2299767, 2292558, 2285349, 2278140, 2270930, 2263721, 2256512, 2249302,
	2242093, 2234884, 2227674, 2220465, 2213256, 2206047, 2198837, 2191628, 2184419, 2177209,
	2170000, 2162791, 2155581, 2148372, 2141163, 2133953, 2126744, 2119535, 2112326, 2105116,
	2097907, 2090698, 2083488, 2076279, 2069070, 2061860, 2054651, 2047442, 2040233, 2033023,
	2025814, 2018605, 2011395, 2004186, 1996977, 1989767, 1982558, 1975349, 1968140, 1960930,
	1953721, 1946512, 1939302, 1932093, 1924884, 1917674, 1910465, 1903256, 1896047, 1888837,
	1881628, 1874419, 1867209, 1860000, 1852791, 1845581, 1838372, 1831163, 1823953, 1816744,
	1809535, 1802326, 1795116, 1787907, 1780698, 1773488, 1766279, 1759070, 1751860, 1744651,
	1737442, 1730233, 1723023, 1715814, 1708605, 1701395, 1694186, 1686977, 1679767, 1672558,
	1665349, 1658140, 1650930, 1643721, 1636512, 1629302, 1622093, 1614884, 1607674, 1600465,
	1593256, 1586047, 1578837,
};

static const unsigned int v203_trans_volt [256] = {
	200, 203, 206, 209, 213, 216, 219, 222,
	225, 228, 231, 234, 238, 241, 244, 247,
	250, 253, 256, 259, 263, 266, 269, 272,
	275, 278, 281, 284, 288, 291, 294, 297,
	300, 303, 306, 309, 313, 316, 319, 322,
	325, 328, 331, 334, 338, 341, 344, 347,
	350, 353, 356, 359, 363, 366, 369, 372,
	375, 378, 381, 384, 388, 391, 394, 397,
	400, 403, 406, 409, 413, 416, 419, 422,
	425, 428, 431, 434, 438, 441, 444, 447,
	450, 453, 456, 459, 463, 466, 469, 472,
	475, 478, 481, 484, 488, 491, 494, 497,
	500, 503, 506, 509, 513, 516, 519, 522,
	525, 528, 531, 534, 538, 541, 544, 547,
	550, 553, 556, 559, 566, 563, 569, 572,
	575, 578, 581, 584, 588, 591, 594, 597,
	600, 603, 606, 609, 613, 616, 619, 622,
	625, 628, 631, 634, 638, 641, 644, 647,
	650, 653, 656, 659, 663, 666, 669, 672,
	675, 678, 681, 684, 688, 691, 694, 697,
	700, 703, 706, 709, 713, 716, 719, 722,
	725, 728, 731, 734, 738, 741, 744, 747,
	750, 753, 756, 759, 763, 766, 769, 772,
	775, 778, 781, 784, 788, 791, 794, 797,
	800, 803, 806, 809, 813, 816, 819, 822,
	825, 828, 831, 834, 838, 841, 844, 847,
	850, 853, 856, 859, 863, 866, 869, 872,
	875, 878, 881, 884, 888, 891, 894, 897,
	900, 903, 906, 909, 913, 916, 919, 922,
	925, 928, 931, 934, 938, 941, 944, 947,
	950, 953, 956, 959, 963, 966, 969, 972,
	975, 978, 981, 984, 988, 991, 994, 997,
};

static const unsigned int v1_trans_volt [256] = {
	0, 4, 8, 12, 16, 20, 23, 27,
	31, 35, 39, 43, 47,	51,	55, 59,
	63, 66, 70, 74, 78,	82, 86, 90,
	94, 98, 102, 105, 109, 113, 117, 121,
	125, 129, 133, 137, 141, 145, 148, 152,
	156, 160, 164, 168, 172, 176, 180, 184,
	188, 191, 195, 199, 203, 207, 211, 215,
	219, 223, 227, 230, 234, 238, 242, 246,
	250, 254, 258, 262, 266, 270, 273, 277,
	281, 285, 289, 293, 297, 301, 305, 309,
	313, 316, 320, 324, 328, 332, 336, 340,
	344, 348, 352, 355, 359, 363, 367, 371,
	375, 379, 383, 387, 391, 395, 398, 402,
	406, 410, 414, 418, 422, 426, 430, 434,
	438, 441, 445, 449, 453, 457, 461, 465,
	469, 473, 477, 480, 484, 488, 492, 496,
	500, 504, 508, 512, 516, 520, 523, 527,
	531, 535, 539, 543, 547, 551, 555, 559,
	563, 566, 570, 574, 578, 582, 586, 590,
	594, 598, 602, 605, 609, 613, 617, 621,
	625, 629, 633, 637, 641, 645, 648, 652,
	656, 660, 664, 668, 672, 676, 680, 684,
	688, 691, 695, 699, 703, 707, 711, 715,
	719, 723, 727, 730, 734, 738, 742, 746,
	750, 754, 758, 762, 766, 770, 773, 777,
	781, 785, 789, 793, 797, 801, 805, 809,
	813, 816, 820, 824, 828, 832, 836, 840,
	844, 848, 852, 855, 859, 863, 867, 871,
	875, 879, 883, 887, 891, 895, 898, 902,
	906, 910, 914, 918, 922, 926, 930, 934,
	938, 941, 945, 949, 953, 957, 961, 965,
	969, 973, 977, 980, 984, 988, 992, 996,
};

static const short int_tbl_v1_v7[5] = {
	171, 341, 512, 683, 853,
};


static const short int_tbl_v7_v11[3] = {
	256, 512, 768
};


static const short int_tbl_v11_v23[11] = {
	85, 171, 256, 341, 427, 512, 597, 683,
	768, 853, 939,
};


static const short int_tbl_v23_v35[11] = {
	85, 171, 256, 341, 427, 512, 597, 683,
	768, 853, 939,
};


static const short int_tbl_v35_v51[15] = {
	64, 128, 192, 256, 320, 384, 448, 512,
	576, 640, 704, 768, 832, 896, 960,
};


static const short int_tbl_v51_v87[35] = {
	28,	57,	85,	114, 142, 171, 199, 228,
	256, 284, 313, 341, 370, 398, 427, 455,
	484, 512, 540, 569, 597, 626, 654, 683,
	711, 740, 768, 796, 825, 853, 882, 910,
	939, 967, 996,
};


static const short int_tbl_v87_v151[63] = {
	16,	32, 48, 64, 80, 96, 112, 128,
	144, 160, 176, 192, 208, 224, 240, 256,
	272, 288, 304, 320, 336, 352, 368, 384,
	400, 416, 432, 448, 464, 480, 496, 512,
	528, 544, 560, 576, 592, 608, 624, 640,
	656, 672, 688, 704, 720, 736, 752, 768,
	784, 800, 816, 832, 848, 864, 880, 896,
	912, 928, 944, 960, 976, 992, 1008,
};


static const short int_tbl_v151_v203[51] = {
	20, 39, 59, 79, 98, 118, 138, 158,
	177, 197, 217, 236, 256, 276, 295, 315,
	335, 354, 374, 394, 414, 433, 453, 473,
	492, 512, 532, 551, 571, 591, 610, 630,
	650, 670, 689, 709, 729, 748, 768, 788,
	807, 827, 847, 866, 886, 906, 926, 945,
	965, 985, 1004,
};


static const short int_tbl_v203_v255[51] = {
	20, 39, 59, 79, 98, 118, 138, 158,
	177, 197, 217, 236, 256, 276, 295, 315,
	335, 354, 374, 394, 414, 433, 453, 473,
	492, 512, 532, 551, 571, 591, 610, 630,
	650, 670, 689, 709, 729, 748, 768, 788,
	807, 827, 847, 866, 886, 906, 926, 945,
	965, 985, 1004,
};

void s6e36w2x01_read_gamma(struct smart_dimming *dimming, const unsigned char *data)
{
	int i, j;
	int temp, tmp;
	u8 pos = 0;
	u8 s_v255[3]={0,};

	tmp = (data[31] & 0xf0) >> 4;
	s_v255[0] = (tmp >> 3) & 0x1;
	s_v255[1] = (tmp >> 2) & 0x1;
	s_v255[2] = (tmp >> 1) & 0x1;

	for (j = 0; j < CI_MAX; j++) {
		temp = ((s_v255[j] & 0x01) ? -1 : 1) * data[pos];
		dimming->gamma[V255][j] = ref_gamma[V255][j] + temp;
		dimming->mtp[V255][j] = temp;
		pos ++;
	}

	for (i = V203; i >= V1; i--) {
		for (j = 0; j < CI_MAX; j++) {
			temp = ((data[pos] & 0x80) ? -1 : 1) *
					(data[pos] & 0x7f);
			dimming->gamma[i][j] = ref_gamma[i][j] + temp;
			dimming->mtp[i][j] = temp;
			pos++;
		}
	}

	temp =data[pos] & 0xf;
	dimming->gamma[V1][CI_RED] = ref_gamma[V1][CI_RED] + temp;
	dimming->mtp[V1][CI_RED] = temp;

	temp = (data[pos] & 0xf0) >> 4;
	dimming->gamma[V1][CI_GREEN] = ref_gamma[V1][CI_GREEN] + temp;
	dimming->mtp[V1][CI_GREEN] = temp;

	temp =data[++pos] & 0xf;
	dimming->gamma[V1][CI_BLUE] = ref_gamma[V1][CI_BLUE] + temp;
	dimming->mtp[V1][CI_BLUE] = temp;

	pr_info("%s:MTP OFFSET\n", __func__);
	for (i = V1; i<= V255; i++)
		pr_info("%d %d %d\n", dimming->mtp[i][0],
				dimming->mtp[i][1],dimming->mtp[i][2]);

	pr_debug("MTP+ Center gamma\n");
	for (i = V1; i<= V255; i++)
		pr_debug("%d %d %d\n", dimming->gamma[i][0],
			dimming->gamma[i][1], dimming->gamma[i][2]);
}

static int calc_vt_volt(int gamma)
{
	int max;

	max = (sizeof(vt_trans_volt) >> 2) - 1;
	if (gamma > max) {
		pr_err(" %s exceed gamma value\n", __func__);
		gamma = max;
	}

	return (int)vt_trans_volt[gamma];
}

static int calc_v0_volt(struct dim_data *data, int color)
{
	return DOUBLE_MULTIPLE_VREGOUT;
}

static int calc_v1_volt(struct dim_data *data, int color)
{
	int gap;
	int ret, v7, gamma;
	unsigned long temp;

	gamma = data->t_gamma[V1][color];

	if (gamma > vreg_element_max[V1]) {
		pr_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V1];
	}
	if (gamma < 0) {
		pr_err(":%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}

	v7 = data->volt[TBL_INDEX_V7][color];
	gap = (DOUBLE_MULTIPLE_VREGOUT - v7);
	temp = (unsigned long)gap * (unsigned long)v1_trans_volt[gamma];
	temp /= MULTIPLY_VALUE;

	ret = DOUBLE_MULTIPLE_VREGOUT - (int)temp;
	//printk("[LCD] calc_v1_volt : ret : %d, gamma : %d\n", ret, gamma);

	return ret;
}

static int calc_v7_volt(struct dim_data *data, int color)
{
	int gap;
	int ret, v11, gamma;
	unsigned long temp;

	gamma = data->t_gamma[V7][color];

	if (gamma > vreg_element_max[V7]) {
		pr_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V7];
	}
	if (gamma < 0) {
		pr_err(":%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}

	v11 = data->volt[TBL_INDEX_V11][color];

	gap = (DOUBLE_MULTIPLE_VREGOUT - v11);
	temp = (unsigned long)gap * (unsigned long)v203_trans_volt[gamma];
	temp /= MULTIPLY_VALUE;

	ret = DOUBLE_MULTIPLE_VREGOUT - (int)temp;
	//printk("[LCD] calc_v7_volt : ret : %d\n", ret);

	return ret;
}

static int calc_v11_volt(struct dim_data *data, int color)
{
	int gap;
	int vt, ret, v23, gamma;
	unsigned long temp;

	gamma = data->t_gamma[V11][color];

	if (gamma > vreg_element_max[V11]) {
		pr_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V11];
	}
	if (gamma < 0) {
		pr_err("%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}

	vt = data->volt_vt[color];
	v23 = data->volt[TBL_INDEX_V23][color];

	gap = vt - v23;
	temp = (unsigned long)gap * (unsigned long)v203_trans_volt[gamma];
	temp /= MULTIPLY_VALUE;

	ret = vt - (int)temp;
	//printk("[LCD] calc_v11_volt : ret : %d\n", ret);

	return ret ;
}

static int calc_v23_volt(struct dim_data *data, int color)
{
	int gap;
	int vt, ret, v35, gamma;
	unsigned long temp;

	gamma = data->t_gamma[V23][color];

	if (gamma > vreg_element_max[V23]) {
		pr_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V23];
	}
	if (gamma < 0) {
		pr_err("%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}


	vt = data->volt_vt[color];
	v35 = data->volt[TBL_INDEX_V35][color];

	gap = vt - v35;
	temp = (unsigned long)gap * (unsigned long)v203_trans_volt[gamma];
	temp /= MULTIPLY_VALUE;

	ret = vt - (int)temp;
	//printk("[LCD] calc_v23_volt : ret : %d\n", ret);

	return ret;

}

static int calc_v35_volt(struct dim_data *data, int color)
{
	int gap;
	int vt, ret, v51, gamma;
	unsigned long temp;

	gamma = data->t_gamma[V35][color];

	if (gamma > vreg_element_max[V35]) {
		pr_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V35];
	}
	if (gamma < 0) {
		pr_err("%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}

	vt = data->volt_vt[color];
	v51 = data->volt[TBL_INDEX_V51][color];

	gap = vt - v51;
	temp = (unsigned long)gap * (unsigned long)v203_trans_volt[gamma];
	temp /= MULTIPLY_VALUE;

	ret = vt - (int)temp;
	//printk("[LCD] calc_v35_volt : ret : %d\n", ret);

	return ret;
}

static int calc_v51_volt(struct dim_data *data, int color)
{
	int gap;
	int vt, ret, v87, gamma;
	unsigned long temp;

	gamma = data->t_gamma[V51][color];

	if (gamma > vreg_element_max[V51]) {
		pr_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V51];
	}
	if (gamma < 0) {
		pr_err("%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}

	vt = data->volt_vt[color];
	v87 = data->volt[TBL_INDEX_V87][color];

	gap = vt - v87;
	temp = (unsigned long)gap * (unsigned long)v203_trans_volt[gamma];
	temp /= MULTIPLY_VALUE;

	ret = vt - (int)temp;
	//printk("[LCD] calc_v51_volt : ret : %d\n", ret);

	return ret;
}

static int calc_v87_volt(struct dim_data *data, int color)
{
	int gap;
	int vt, ret, v151, gamma;
	unsigned long temp;

	gamma = data->t_gamma[V87][color];

	if (gamma > vreg_element_max[V87]) {
		pr_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V87];
	}
	if (gamma < 0) {
		pr_err("%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}

	vt = data->volt_vt[color];
	v151 = data->volt[TBL_INDEX_V151][color];
	//printk("[LCD] calc_v87_volt : vt : %d, v151 : %d\n", vt, v151);

	gap = vt - v151;
	temp = (unsigned long)gap * (unsigned long)v203_trans_volt[gamma];
	temp /= MULTIPLY_VALUE;

	ret = vt - (int)temp;
	//printk("[LCD] calc_v87_volt : ret : %d\n", ret);

	return ret;
}

static int calc_v151_volt(struct dim_data *data, int color)
{
	int gap;
	int vt, ret, v203, gamma;
	unsigned long temp;

	gamma = data->t_gamma[V151][color];

	if (gamma > vreg_element_max[V151]) {
		pr_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V151];
	}
	if (gamma < 0) {
		pr_err("%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}

	vt = data->volt_vt[color];
	v203 = data->volt[TBL_INDEX_V203][color];

	gap = vt - v203;
	temp = (unsigned long)gap * (unsigned long)v203_trans_volt[gamma];
	temp /= MULTIPLY_VALUE;
	ret = vt - (int)temp;

	return ret;
}

static int calc_v203_volt(struct dim_data *data, int color)
{
	int gap;
	int vt, ret, v255, gamma;
	unsigned long temp;

	gamma = data->t_gamma[V203][color];

	if (gamma > vreg_element_max[V203]) {
		pr_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V203];
	}
	if (gamma < 0) {
		pr_err("%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}

	vt = data->volt_vt[color];
	v255 = data->volt[TBL_INDEX_V255][color];

	gap = vt - v255;
	temp = (unsigned long)gap * (unsigned long)v203_trans_volt[gamma];
	temp /= MULTIPLY_VALUE;
	ret = vt - (int)temp;

	return ret;
}

static int calc_v255_volt(struct dim_data *data, int color)
{
	int ret, gamma;

	gamma = data->t_gamma[V255][color];

	if (gamma > vreg_element_max[V255]) {
		pr_err("%s : gamma overflow : %d\n", __FUNCTION__, gamma);
		gamma = vreg_element_max[V255];
	}
	if (gamma < 0) {
		pr_err("%s : gamma undeflow : %d\n", __FUNCTION__, gamma);
		gamma = 0;
	}

	ret = (int)v255_trans_volt[gamma];

	return ret;
}

static int calc_inter_v1_v7(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v1, v7, ratio, temp;

	ratio = (int)int_tbl_v1_v7[gray];

	v1 = data->volt[TBL_INDEX_V1][color];
	v7 = data->volt[TBL_INDEX_V7][color];

	temp = (v1 - v7) * ratio;
	temp >>= 10;
	ret = v1 - temp;

	return ret;
}

static int calc_inter_v7_v11(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v7, v11, ratio, temp;

	ratio = (int)int_tbl_v7_v11[gray];
	v7 = data->volt[TBL_INDEX_V7][color];
	v11 = data->volt[TBL_INDEX_V11][color];

	temp = (v7 - v11) * ratio;
	temp >>= 10;
	ret = v7 - temp;
	//printk("[LCD] ret: %d\n", ret);

	return ret;
}

static int calc_inter_v11_v23(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v11, v23, ratio, temp;

	ratio = (int)int_tbl_v11_v23[gray];
	v11 = data->volt[TBL_INDEX_V11][color];
	v23 = data->volt[TBL_INDEX_V23][color];

	temp = (v11 - v23) * ratio;
	temp >>= 10;
	ret = v11 - temp;

	return ret;
}

static int calc_inter_v23_v35(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v23, v35, ratio, temp;

	ratio = (int)int_tbl_v23_v35[gray];
	v23 = data->volt[TBL_INDEX_V23][color];
	v35 = data->volt[TBL_INDEX_V35][color];

	temp = (v23 - v35) * ratio;
	temp >>= 10;
	ret = v23 - temp;

	return ret;
}

static int calc_inter_v35_v51(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v35, v51, ratio, temp;

	ratio = (int)int_tbl_v35_v51[gray];
	v35 = data->volt[TBL_INDEX_V35][color];
	v51 = data->volt[TBL_INDEX_V51][color];

	temp = (v35 - v51) * ratio;
	temp >>= 10;
	ret = v35 - temp;

	return ret;
}

static int calc_inter_v51_v87(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v51, v87, ratio, temp;

	ratio = (int)int_tbl_v51_v87[gray];
	v51 = data->volt[TBL_INDEX_V51][color];
	v87 = data->volt[TBL_INDEX_V87][color];

	temp = (v51 - v87) * ratio;
	temp >>= 10;
	ret = v51 - temp;

	return ret;
}

static int calc_inter_v87_v151(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v87, v151, ratio, temp;

	ratio = (int)int_tbl_v87_v151[gray];
	v87 = data->volt[TBL_INDEX_V87][color];
	v151 = data->volt[TBL_INDEX_V151][color];

	temp = (v87 - v151) * ratio;
	temp >>= 10;
	ret = v87 - temp;
	return ret;
}

static int calc_inter_v151_v203(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v151, v203, ratio, temp;

	ratio = (int)int_tbl_v151_v203[gray];
	v151 = data->volt[TBL_INDEX_V151][color];
	v203 = data->volt[TBL_INDEX_V203][color];

	temp = (v151 - v203) * ratio;
	temp >>= 10;
	ret = v151 - temp;

	return ret;
}

static int calc_inter_v203_v255(struct dim_data *data, int gray, int color)
{
	int ret = 0;
	int v203, v255, ratio, temp;

	ratio = (int)int_tbl_v203_v255[gray];
	v203 = data->volt[TBL_INDEX_V203][color];
	v255 = data->volt[TBL_INDEX_V255][color];

	temp = (v203 - v255) * ratio;
	temp >>= 10;
	ret = v203 - temp;

	return ret;
}

static int calc_reg_v1(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;
	unsigned long temp;

	t1 = DOUBLE_MULTIPLE_VREGOUT - data->look_volt[V1][color];
	t2 = DOUBLE_MULTIPLE_VREGOUT - data->look_volt[V7][color];

	temp = ((unsigned long)t1 * (unsigned long)fix_const[V1].de) / (unsigned long)t2;
	ret =  temp - fix_const[V1].nu;

	return ret;
}


static int calc_reg_v7(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;
	unsigned long temp;

	t1 = DOUBLE_MULTIPLE_VREGOUT - data->look_volt[V7][color];
	t2 = DOUBLE_MULTIPLE_VREGOUT - data->look_volt[V11][color];

	temp = ((unsigned long)t1 * (unsigned long)fix_const[V7].de) / (unsigned long)t2;
	ret =  temp - fix_const[V7].nu;

	return ret;
}


static int calc_reg_v11(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;
	unsigned long temp;

	t1 = data->volt_vt[color] - data->look_volt[V11][color];
	t2 = data->volt_vt[color] - data->look_volt[V23][color];

	temp = ((unsigned long)t1 * (unsigned long)fix_const[V11].de) / (unsigned long)t2;
	ret =  (int)temp - fix_const[V11].nu;

	return ret;

}

static int calc_reg_v23(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;
	unsigned long temp;

	t1 = data->volt_vt[color] - data->look_volt[V23][color];
	t2 = data->volt_vt[color] - data->look_volt[V35][color];

	temp = ((unsigned long)t1 * (unsigned long)fix_const[V23].de) / (unsigned long)t2;
	ret = (int)temp - fix_const[V23].nu;

	return ret;

}

static int calc_reg_v35(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;
	unsigned long temp;

	t1 = data->volt_vt[color] - data->look_volt[V35][color];
	t2 = data->volt_vt[color] - data->look_volt[V51][color];

	temp = ((unsigned long)t1 * (unsigned long)fix_const[V35].de)/ (unsigned long)t2;
	ret = (int)temp - fix_const[V35].nu;

	return ret;
}


static int calc_reg_v51(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;
	unsigned long temp;

	t1 = data->volt_vt[color] - data->look_volt[V51][color];
	t2 = data->volt_vt[color] - data->look_volt[V87][color];

	temp = ((unsigned long)t1 * (unsigned long)fix_const[V51].de) / (unsigned long)t2;
	ret = (int)temp - fix_const[V51].nu;

	return ret;
}


static int calc_reg_v87(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;
	unsigned long temp;

	t1 = data->volt_vt[color] - data->look_volt[V87][color];
	t2 = data->volt_vt[color] - data->look_volt[V151][color];

	temp = ((unsigned long)t1 * (unsigned long)fix_const[V87].de) / (unsigned long)t2;

	ret = (int)temp - fix_const[V87].nu;

	return ret;
}

static int calc_reg_v151(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;
	unsigned long temp;

	t1 = data->volt_vt[color] - data->look_volt[V151][color];
	t2 = data->volt_vt[color] - data->look_volt[V203][color];

	temp = ((unsigned long)t1 * (unsigned long)fix_const[V151].de) / (unsigned long)t2;
	ret = (int)temp - fix_const[V151].nu;

	return ret;
}



static int calc_reg_v203(struct dim_data *data, int color)
{
	int ret;
	int t1, t2;
	unsigned long temp;

	t1 = data->volt_vt[color] - data->look_volt[V203][color];
	t2 = data->volt_vt[color] - data->look_volt[V255][color];

	temp = ((unsigned long)t1 * (unsigned long)fix_const[V203].de) / (unsigned long)t2;
	ret = (int)temp - fix_const[V203].nu;

	return ret;
}

static int calc_reg_v255(struct dim_data *data, int color)
{
	int ret;
	int t1;
	unsigned long temp;

	t1 = DOUBLE_MULTIPLE_VREGOUT -  data->look_volt[V255][color];
	temp = ((unsigned long)t1 * (unsigned long)fix_const[V255].de) / DOUBLE_MULTIPLE_VREGOUT;
	ret = (int)temp - fix_const[V255].nu;

	return ret;
}

int generate_volt_table(struct dim_data *data)
{
	int i, j;
	int seq, index, gray;
	int ret = 0;

	int calc_seq[NUM_VREF] = {V255, V203, V151, V87, V51, V35, V23, V11, V7, V1};
	int (*calc_volt_point[NUM_VREF])(struct dim_data *, int) = {
		calc_v1_volt,
		calc_v7_volt,
		calc_v11_volt,
		calc_v23_volt,
		calc_v35_volt,
		calc_v51_volt,
		calc_v87_volt,
		calc_v151_volt,
		calc_v203_volt,
		calc_v255_volt,
	};
	int (*calc_inter_volt[NUM_VREF])(struct dim_data *, int, int)  = {
		NULL,
		calc_inter_v1_v7,
		calc_inter_v7_v11,
		calc_inter_v11_v23,
		calc_inter_v23_v35,
		calc_inter_v35_v51,
		calc_inter_v51_v87,
		calc_inter_v87_v151,
		calc_inter_v151_v203,
		calc_inter_v203_v255,
	};

	for (i = 0; i < CI_MAX; i++)
		data->volt_vt[i] = calc_vt_volt(data->vt_mtp[i]);

	/* calculate voltage for V0 */
	for (i = 0; i < CI_MAX; i++)
		data->volt[0][i] = calc_v0_volt(data, i);

	/* calculate voltage for every vref point */
	for (j = 0; j < NUM_VREF; j++) {
		seq = calc_seq[j];
		index = vref_index[seq];
		if (calc_volt_point[seq] != NULL) {
			for (i = 0; i < CI_MAX; i++)
				data->volt[index][i] = calc_volt_point[seq](data ,i);
		}
	}

	index = 0;
	for (i = 0; i < MAX_GRADATION; i++) {
		if (i == vref_index[index]) {
			index++;
			continue;
		}
		gray = (i - vref_index[index - 1]) - 1;
		for (j = 0; j < CI_MAX; j++) {
			if (calc_inter_volt[index] != NULL) {
				data->volt[i][j] = calc_inter_volt[index](data, gray, j);
			}
		}

	}
#if defined (SMART_DIMMING_DEBUG)
	printk("=========================== VT Voltage ===========================\n");

	printk("R : %05d : G: %05d : B : %05d\n",
					data->volt_vt[0], data->volt_vt[1], data->volt_vt[2]);

	printk("\n=================================================================\n");

	for (i = 0; i < MAX_GRADATION; i++) {
		printk("V%03d R : %05d : G : %05d B : %05d\n", i,
					data->volt[i][CI_RED], data->volt[i][CI_GREEN], data->volt[i][CI_BLUE]);
	}
#endif
	return ret;
}

static int lookup_volt_index(struct dim_data *data, int gray)
{
	int ret, i;
	int temp;
	int index;
	int index_l, index_h, exit;
	int cnt_l, cnt_h;
	int p_delta, delta;

	temp = gray / (MULTIPLY_VALUE * MULTIPLY_VALUE);

	index = (int)lookup_tbl[temp];

	exit = 1;
	i = 0;
	while(exit) {
		index_l = temp - i;
		index_h = temp + i;
		if (index_l < 0)
			index_l = 0;
		if (index_h > MAX_BRIGHTNESS_COUNT)
			index_h = MAX_BRIGHTNESS_COUNT;
		cnt_l = (int)lookup_tbl[index] - (int)lookup_tbl[index_l];
		cnt_h = (int)lookup_tbl[index_h] - (int)lookup_tbl[index];

		if (cnt_l + cnt_h) {
			exit = 0;
		}
		i++;
	}

	p_delta = 0;
	index = (int)lookup_tbl[index_l];
	ret = index;

	temp = gamma_multi_tbl[index] * MULTIPLY_VALUE;

	if (gray > temp)
		p_delta = gray - temp;
	else
		p_delta = temp - gray;

	for (i = 0; i <= (cnt_l + cnt_h); i++) {
		temp = gamma_multi_tbl[index + i] * MULTIPLY_VALUE;
		if (gray > temp)
			delta = gray - temp;
		else
			delta = temp - gray;

		if (delta < p_delta) {
			p_delta = delta;
			ret = index + i;
		}
	}

	return ret;
}

int cal_gamma_from_index(struct dim_data *data, struct SmtDimInfo *brInfo)
{
	int i, j;
	int ret = 0;
	int gray, index;
	signed int shift, c_shift;
	int gamma_int[NUM_VREF][CI_MAX];
	int br, temp;
	unsigned char *result;
	int (*calc_reg[NUM_VREF])(struct dim_data *, int)  = {
		calc_reg_v1,
		calc_reg_v7,
		calc_reg_v11,
		calc_reg_v23,
		calc_reg_v35,
		calc_reg_v51,
		calc_reg_v87,
		calc_reg_v151,
		calc_reg_v203,
		calc_reg_v255,
	};

	br = brInfo->refBr;

	result = brInfo->gamma;

	if (br > MAX_BRIGHTNESS_COUNT) {
		printk("Warning Exceed Max brightness : %d\n", br);
		br = MAX_BRIGHTNESS_COUNT;
	}
	for (i = V1; i < NUM_VREF; i++) {
		/* get reference shift value */
		if (brInfo->rTbl == NULL) {
			shift = 0;
		}
		else {
			shift = (signed int)brInfo->rTbl[i];
		}

		gray = brInfo->cGma[vref_index[i]] * br;
		index = lookup_volt_index(data, gray);
		index = index + shift;

		if(i == V1)
			index = 1;
		for (j = 0; j < CI_MAX; j++) {
			if (calc_reg[i] != NULL) {
				data->look_volt[i][j] = data->volt[index][j];
			}
		}
	}

	for (i = V1; i < NUM_VREF; i++) {
		for (j = 0; j < CI_MAX; j++) {
			if (calc_reg[i] != NULL) {
				index = (i * CI_MAX) + j;

				if (brInfo->cTbl == NULL)
					c_shift = 0;
				else
					c_shift = (signed int)brInfo->cTbl[index];

				temp = calc_reg[i](data, j);
				gamma_int[i][j] = (temp + c_shift) - data->mtp[i][j];

				if (gamma_int[i][j] >= vreg_element_max[i])
					gamma_int[i][j] = vreg_element_max[i];
				if (gamma_int[i][j] < 0)
					gamma_int[i][j] = 0;
			}
		}
	}

	index = 0;
	result[index++] = OLED_CMD_GAMMA;

	for (i = V255; i >= V1; i--) {
		for (j = 0; j < CI_MAX; j++) {
			//printk("%3d, ", gamma_int[i][j]);
			if (i == V255) {
				result[index++] = gamma_int[i][j] > 0xff ? 1 : 0;
				result[index++] = gamma_int[i][j] & 0xff;
			} else
				result[index++] = (unsigned char)gamma_int[i][j];
		}
	}
		//printk("\n");
	result[index++] = 0x00;
	result[index++] = 0x00;

	return ret;
}


