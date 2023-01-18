/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018-2020, 2023 snickerbockers
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

#include "i_hate_windows.h"

#include <cmath>
#include <mutex>
#include <condition_variable>

#include <portaudio.h>

#include "washdc/error.h"
#include "sound.hpp"
#include "config_file.h"
#include "threading.h"
#include "intmath.h"

namespace sound {

static DEF_ERROR_INT_ATTR(portaudio_error)
static DEF_ERROR_STRING_ATTR(portaudio_error_text)

static PaStream *snd_stream;

static int snd_cb(const void *input, void *output,
                  unsigned long n_frames,
                  PaStreamCallbackTimeInfo const *ti,
                  PaStreamCallbackFlags flags,
                  void *argp);


static washdc_mutex buffer_lock;
static washdc_cvar samples_submitted;

// 1/10 of a second
static const unsigned BUF_LEN = 4410;
static washdc_sample_type sample_buf_left[BUF_LEN], sample_buf_right[BUF_LEN];
static unsigned read_buf_idx, write_buf_idx;
static bool do_mute, have_sound_dev;
static enum sync_mode audio_sync_mode;

void init(void) {
    washdc_mutex_init(&buffer_lock);
    washdc_cvar_init(&samples_submitted);

    do_mute = false;
    have_sound_dev = true;
    audio_sync_mode = SYNC_MODE_NORM;
    cfg_get_bool("audio.mute", &do_mute);

    read_buf_idx = write_buf_idx = 0;

    int err;
    if ((err = Pa_Initialize()) != paNoError) {
        fprintf(stderr, "Unable to initialize PortAudio: %s\n",
                Pa_GetErrorText(err));
        have_sound_dev = false;
        return;
    }

    /*
     * XXX: if you ever change the sample frequency to something other than
     * 44.1kHz, then AICA_EXTERNAL_FREQ in libwashdc/hw/aica/aica.c needs to be
     * changed to match it.
     */
    err = Pa_OpenDefaultStream(&snd_stream, 0, 2, paInt32, 44100*1,
                               paFramesPerBufferUnspecified,
                               snd_cb, NULL);
    if (err != paNoError) {
        fprintf(stderr, "Unable to open default PortAudio stream: %s\n",
                Pa_GetErrorText(err));
        have_sound_dev = false;
        return;
    }

    Pa_StartStream(snd_stream);
}

void cleanup(void) {
    int err;
    if (have_sound_dev) {
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
    }

    washdc_cvar_cleanup(&samples_submitted);
    washdc_mutex_cleanup(&buffer_lock);
}

static int snd_cb(const void *input, void *output,
                  unsigned long n_frames,
                  PaStreamCallbackTimeInfo const *ti,
                  PaStreamCallbackFlags flags,
                  void *argp) {
    washdc_mutex_lock(&buffer_lock);

    washdc_sample_type *outbuf = (washdc_sample_type*)output;
    unsigned long frame_no;
    for (frame_no = 0; frame_no < n_frames; frame_no++) {
        washdc_sample_type sample_left, sample_right;
        if (read_buf_idx != write_buf_idx) {
            sample_left = sample_buf_left[read_buf_idx];
            sample_right = sample_buf_right[read_buf_idx];
            read_buf_idx = (1 + read_buf_idx) % BUF_LEN;
        } else {
            sample_left = 0;
            sample_right = 0;
        }
        *outbuf++ = sample_left;
        *outbuf++ = sample_right;
    }
    washdc_cvar_signal(&samples_submitted);

    washdc_mutex_unlock(&buffer_lock);
    return 0;
}

static washdc_sample_type scale_sample(washdc_sample_type sample) {
    /*
     * even though we use 32-bit int to store samples, we expect the emu
     * core to submit samples that were initially 16-bit, so we have to
     * scale them up a bit to compensate for the 16-bit to 32-bit conversion.
     */
    return sat_shift(sample, 8);
}

void submit_samples(washdc_sample_type *samples, unsigned count) {
    if (!have_sound_dev)
        return;
    washdc_mutex_lock(&buffer_lock);

    while (count--) {
        unsigned next_write_buf_idx = (1 + write_buf_idx) % BUF_LEN;
        if (audio_sync_mode == SYNC_MODE_NORM)
            while (next_write_buf_idx == read_buf_idx)
                washdc_cvar_wait(&samples_submitted, &buffer_lock);
        if (do_mute) {
            sample_buf_left[write_buf_idx] = 0;
            sample_buf_right[write_buf_idx] = 0;
        } else {
            sample_buf_left[write_buf_idx] = scale_sample(*samples++);
            sample_buf_right[write_buf_idx] = scale_sample(*samples++);
        }
        write_buf_idx = next_write_buf_idx;
    }

    washdc_mutex_unlock(&buffer_lock);
}

void mute(bool en_mute) {
    do_mute = en_mute;
}

bool is_muted(void) {
    return do_mute;
}

void set_sync_mode(enum sync_mode mode) {
    audio_sync_mode = mode;
}

}
