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

#include <cmath>
#include <mutex>
#include <condition_variable>

#include <portaudio.h>

#include "washdc/error.h"
#include "sound.hpp"
#include "washdc/config_file.h"

namespace sound {

static DEF_ERROR_INT_ATTR(portaudio_error)
static DEF_ERROR_STRING_ATTR(portaudio_error_text)

static PaStream *snd_stream;

static int snd_cb(const void *input, void *output,
                  unsigned long n_frames,
                  PaStreamCallbackTimeInfo const *ti,
                  PaStreamCallbackFlags flags,
                  void *argp);

static std::mutex buffer_lock;
static std::condition_variable samples_submitted;

// 1/10 of a second
static const unsigned BUF_LEN = 4410;
static washdc_sample_type sample_buf[BUF_LEN];
static unsigned read_buf_idx, write_buf_idx;
static bool do_mute;

void init(void) {
    do_mute = true;
    cfg_get_bool("audio.mute", &do_mute);

    read_buf_idx = write_buf_idx = 0;

    int err;
    if ((err = Pa_Initialize()) != paNoError) {
        error_set_portaudio_error(err);
        error_set_portaudio_error_text(Pa_GetErrorText(err));
        RAISE_ERROR(ERROR_EXT_FAILURE);
    }
    err = Pa_OpenDefaultStream(&snd_stream, 0, 2, paInt32, 44100,
                               paFramesPerBufferUnspecified,
                               snd_cb, NULL);
    if (err != paNoError) {
        error_set_portaudio_error(err);
        error_set_portaudio_error_text(Pa_GetErrorText(err));
        RAISE_ERROR(ERROR_EXT_FAILURE);
    }

    Pa_StartStream(snd_stream);
}

void cleanup(void) {
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
}

static int snd_cb(const void *input, void *output,
                  unsigned long n_frames,
                  PaStreamCallbackTimeInfo const *ti,
                  PaStreamCallbackFlags flags,
                  void *argp) {
    std::unique_lock<std::mutex> lck(buffer_lock);
    washdc_sample_type *outbuf = (washdc_sample_type*)output;
    int frame_no;
    for (frame_no = 0; frame_no < n_frames; frame_no++) {
        // TODO: stereo
        washdc_sample_type sample;
        if (read_buf_idx != write_buf_idx) {
            sample = sample_buf[read_buf_idx];
            read_buf_idx = (1 + read_buf_idx) % BUF_LEN;
        } else {
            sample = 0;
        }
        *outbuf++ = sample;
        *outbuf++ = sample;
    }
    samples_submitted.notify_one();
    return 0;
}

void submit_samples(washdc_sample_type *samples, unsigned count) {
    std::unique_lock<std::mutex> lck(buffer_lock);

    while (count) {
        unsigned next_write_buf_idx = (1 + write_buf_idx) % BUF_LEN;
        while (next_write_buf_idx == read_buf_idx) {
            samples_submitted.wait(lck);
        }
        if (do_mute)
            sample_buf[write_buf_idx] = 0;
        else
            sample_buf[write_buf_idx] = *samples++;
        write_buf_idx = next_write_buf_idx;
        count--;
    }
}

void mute(bool en_mute) {
    do_mute = en_mute;
}

bool is_muted(void) {
    return do_mute;
}

}
