/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2017 snickerbockers
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

#ifndef GDROM_H_
#define GDROM_H_

struct gdrom_status {
    // get off the phone!
    bool bsy;

    // response to an ata command is possible
    bool drdy;

    // drive fault
    bool df;

    // seek processing complete
    bool dsc;

    // data transfer possible
    bool drq;

    //correctable error flag
    bool corr;

    // error flag
    bool check;
};

struct gdrom_error {
    uint32_t ili : 1;
    uint32_t eomf : 1;
    uint32_t abrt : 1;
    uint32_t mcr : 1;
    uint32_t sense_key : 4;
};

struct gdrom_features {
    bool dma_enable;
    bool set_feat_enable;// this is true if the lower 7 bits are 3
};

enum gdrom_trans_mode{
    TRANS_MODE_PIO_DFLT,
    TRANS_MODE_PIO_FLOW_CTRL,
    TRANS_MODE_SINGLE_WORD_DMA,
    TRANS_MODE_MULTI_WORD_DMA,
    TRANS_MODE_PSEUDO_DMA,

    TRANS_MODE_COUNT
};

struct gdrom_sector_count {
    enum gdrom_trans_mode trans_mode;
    unsigned mode_val;
};

struct gdrom_int_reason {
    bool cod;
    bool io;
};

struct gdrom_dev_ctrl {
    bool nien;
    bool srst;
};

#endif
