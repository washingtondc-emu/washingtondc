// license:BSD-3-Clause
// copyright-holders:ElSemi, Deunan Knute, R. Belmont
// thanks-to: kingshriek

// this is copied from in mame/src/devices/sound/aica.cpp in the MAME source

#ifndef AICA_ADPCM_H_
#define AICA_ADPCM_H_

#include <stdint.h>

#include "aica.h"

// this is copied from in mame/src/devices/sound/aica.cpp in the MAME source
#define ADPCMSHIFT  8

#define ADFIX(f) ((int)((f) * (float)(1 << ADPCMSHIFT)))

static int TableQuant[8] = {ADFIX(0.8984375),ADFIX(0.8984375),ADFIX(0.8984375),ADFIX(0.8984375),ADFIX(1.19921875),ADFIX(1.59765625),ADFIX(2.0),ADFIX(2.3984375)};
static int quant_mul[16] =  { 1, 3, 5, 7, 9, 11, 13, 15, -1, -3, -5, -7, -9, -11, -13, -15};

static int aica_adpcm_min(int v1, int v2) {
    return v1 < v2 ? v1 : v2;
}

static int aica_adpcm_max(int v1, int v2) {
    return v1 > v2 ? v1 : v2;
}

static int32_t clip16(int x) { return aica_adpcm_min(32767, aica_adpcm_max(-32768, x)); }

// void aica_device::InitADPCM(int *PrevSignal, int *PrevQuant)
static void adpcm_init(int *PrevSignal, int *PrevQuant)
{
	*PrevSignal = 0;
	*PrevQuant = 0x7f;
}

// s16 aica_device::DecodeADPCM(int *PrevSignal, u8 Delta, int *PrevQuant)
static inline int32_t adpcm_yamaha_expand_nibble(struct aica_chan *c, uint8_t nibble)
{
    int *PrevSignal = &c->predictor;
    int *PrevQuant = &c->step;
    uint8_t Delta = nibble;

    if (!c->step)
        adpcm_init(&c->predictor, &c->step);

    int x = (*PrevQuant * quant_mul[Delta & 7]) / 8;
	if (x > 0x7FFF) x = 0x7FFF;
	if (Delta & 8)  x = -x;
	x += *PrevSignal;
#if 0 // older implementation
	int x = *PrevQuant * quant_mul [Delta & 15];
		x = *PrevSignal + ((int)(x + ((u32)x >> 29)) >> 3);
#endif
	*PrevSignal = clip16(x);
	*PrevQuant = (*PrevQuant * TableQuant[Delta & 7]) >> ADPCMSHIFT;
	*PrevQuant = (*PrevQuant < 0x7f) ? 0x7f : ((*PrevQuant > 0x6000) ? 0x6000 : *PrevQuant);
	return *PrevSignal;
}

#endif
