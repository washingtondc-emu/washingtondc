/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018, 2019 snickerbockers
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 ******************************************************************************/

#include <math.h>
#include <stdbool.h>

#include <portaudio.h>

#include "washdc/error.h"
#include "sound.h"

static DEF_ERROR_INT_ATTR(portaudio_error)
static DEF_ERROR_STRING_ATTR(portaudio_error_text)

PaStream *snd_stream;

static int snd_cb(const void *input, void *output,
                  unsigned long n_frames,
                  PaStreamCallbackTimeInfo const *ti,
                  PaStreamCallbackFlags flags,
                  void *argp);

static const bool dump_sound_to_file = false;

static FILE *outfile;

void sound_init(void) {
    if (dump_sound_to_file)
        outfile = fopen("snd.raw", "w");
    int err;
    if ((err = Pa_Initialize()) != paNoError) {
        error_set_portaudio_error(err);
        error_set_portaudio_error_text(Pa_GetErrorText(err));
        RAISE_ERROR(ERROR_EXT_FAILURE);
    }
    err = Pa_OpenDefaultStream(&snd_stream, 0, 2, paFloat32, 44100,
                               paFramesPerBufferUnspecified,
                               snd_cb, NULL);
    if (err != paNoError) {
        error_set_portaudio_error(err);
        error_set_portaudio_error_text(Pa_GetErrorText(err));
        RAISE_ERROR(ERROR_EXT_FAILURE);
    }

    Pa_StartStream(snd_stream);
}

void sound_cleanup(void) {
    int err;
    if ((err = Pa_StopStream(snd_stream)) != paNoError) {
        error_set_portaudio_error(err);
        error_set_portaudio_error_text(Pa_GetErrorText(err));
        RAISE_ERROR(ERROR_EXT_FAILURE);
    }
    if ((err = Pa_Terminate()) != paNoError) {
        error_set_portaudio_error(err);
        error_set_portaudio_error_text(Pa_GetErrorText(err));
        RAISE_ERROR(ERROR_EXT_FAILURE);
    }
    if (outfile) {
        fclose(outfile);
        outfile = NULL;
    }
}

/*
 * A * cos(2 * pi * f * t / 44100 + phi) + C
 */

unsigned phase;
#define FREQ 400

static int snd_cb(const void *input, void *output,
                  unsigned long n_frames,
                  PaStreamCallbackTimeInfo const *ti,
                  PaStreamCallbackFlags flags,
                  void *argp) {
    float *output_as_float = (float*)output;
    int frame_no;
    for (frame_no = 0; frame_no < n_frames; frame_no++) {
        float sample = 0.0f;
        // uncomment the below line to get an annoying 400hz test tone.
        /* float sample = cos(2 * M_PI * 400 * (phase++ / 44100.0)); */
        *output_as_float++ = sample;
        *output_as_float++ = sample;
    }
    return 0;
}

void sound_submit_sample(int16_t sample) {
    if (outfile)
        fwrite(&sample, sizeof(sample), 1, outfile);
}
