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

#include <cstring>
#include <iostream>

#include "sh4.hpp"

void Sh4::setTmuTocr(uint8_t new_val) {
    if (new_val != 0)
        std::cerr << "WARNING: writing non-zero value to sh4 TOCR register" <<
            std::endl;

    tmu.tocr = new_val;
}

uint8_t Sh4::getTmuTocr() {
    return tmu.tocr;
}

void Sh4::setTmuTstr(uint8_t new_val) {
    tmu.tstr = new_val;
}

uint8_t Sh4::getTmuTstr() {
    return tmu.tstr;
}

void Sh4::setTmuTcor(unsigned chan, uint32_t new_val) {
    tmu.channels[chan].tcor = new_val;
}

uint32_t Sh4::getTmuTcor(unsigned chan) {
    return tmu.channels[chan].tcor;
}

void Sh4::setTmuTcnt(unsigned chan, uint32_t new_val) {
    tmu.channels[chan].tcnt = new_val;
}

uint32_t Sh4::getTmuTcnt(unsigned chan) {
    return tmu.channels[chan].tcnt;
}

void Sh4::setTmuTcr(unsigned chan, uint16_t new_val) {
    tmu.channels[chan].tcr = new_val;
}

uint32_t Sh4::getTmuTcr(unsigned chan) {
    return tmu.channels[chan].tcr;
}

void Sh4::setTmuTcpr2(uint32_t new_val) {
    tmu.tcpr2 = new_val;
}

uint32_t Sh4::getTmuTcpr2() {
    return tmu.tcpr2;
}

int Sh4::TocrRegReadHandler(void *buf, struct MemMappedReg const *reg_info) {
    uint8_t tmp = getTmuTocr();

    if (reg_info->len != sizeof(tmp))
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("Whatever happens when you don't "
                                              "supply the correct size while "
                                              "reading from or writing to a "
                                              "memory-mapped register"));

    memcpy(buf, &tmp, reg_info->len);

    return 0;
}

int Sh4::TocrRegWriteHandler(void const *buf, struct MemMappedReg const *reg_info) {
    uint8_t tmp;

    if (reg_info->len != sizeof(tmp))
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("Whatever happens when you don't "
                                              "supply the correct size while "
                                              "reading from or writing to a "
                                              "memory-mapped register"));

    memcpy(&tmp, buf, sizeof(tmp));

    setTmuTocr(tmp);

    return 0;
}

int Sh4::TstrRegReadHandler(void *buf, struct MemMappedReg const *reg_info) {
    uint8_t tmp = getTmuTstr();

    if (reg_info->len != sizeof(tmp))
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("Whatever happens when you don't "
                                              "supply the correct size while "
                                              "reading from or writing to a "
                                              "memory-mapped register"));

    memcpy(buf, &tmp, reg_info->len);

    return 0;
}

int Sh4::TstrRegWriteHandler(void const *buf, struct MemMappedReg const *reg_info) {
    uint8_t tmp;

    if (reg_info->len != sizeof(tmp))
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("Whatever happens when you don't "
                                              "supply the correct size while "
                                              "reading from or writing to a "
                                              "memory-mapped register"));

    memcpy(&tmp, buf, sizeof(tmp));

    setTmuTstr(tmp);

    return 0;
}

int Sh4::Tcor0RegReadHandler(void *buf, struct MemMappedReg const *reg_info) {
    uint32_t tmp;

    if (reg_info->len != sizeof(tmp))
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("Whatever happens when you don't "
                                              "supply the correct size while "
                                              "reading from or writing to a "
                                              "memory-mapped register"));

    tmp = getTmuTcor(0);
    memcpy(buf, &tmp, sizeof(tmp));

    return 0;
}

int Sh4::Tcor0RegWriteHandler(void const *buf, struct MemMappedReg const *reg_info) {
    uint32_t tmp;

    if (reg_info->len != sizeof(tmp))
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("Whatever happens when you don't "
                                              "supply the correct size while "
                                              "reading from or writing to a "
                                              "memory-mapped register"));

    memcpy(&tmp, buf, sizeof(tmp));

    setTmuTcor(0, tmp);

    return 0;
}

int Sh4::Tcnt0RegReadHandler(void *buf, struct MemMappedReg const *reg_info) {
    uint32_t tmp;

    if (reg_info->len != sizeof(tmp))
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("Whatever happens when you don't "
                                              "supply the correct size while "
                                              "reading from or writing to a "
                                              "memory-mapped register"));

    tmp = getTmuTcnt(0);
    memcpy(buf, &tmp, sizeof(tmp));

    return 0;
}

int Sh4::Tcnt0RegWriteHandler(void const *buf, struct MemMappedReg const *reg_info) {
    uint32_t tmp;

    if (reg_info->len != sizeof(tmp))
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("Whatever happens when you don't "
                                              "supply the correct size while "
                                              "reading from or writing to a "
                                              "memory-mapped register"));

    memcpy(&tmp, buf, sizeof(tmp));

    setTmuTcnt(0, tmp);

    return 0;
}

int Sh4::Tcr0RegReadHandler(void *buf, struct MemMappedReg const *reg_info) {
    uint16_t tmp;

    if (reg_info->len != sizeof(tmp))
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("Whatever happens when you don't "
                                              "supply the correct size while "
                                              "reading from or writing to a "
                                              "memory-mapped register"));

    tmp = getTmuTcr(0);
    memcpy(buf, &tmp, sizeof(tmp));

    return 0;
}

int Sh4::Tcr0RegWriteHandler(void const *buf, struct MemMappedReg const *reg_info) {
    uint16_t tmp;

    if (reg_info->len != sizeof(tmp))
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("Whatever happens when you don't "
                                              "supply the correct size while "
                                              "reading from or writing to a "
                                              "memory-mapped register"));

    memcpy(&tmp, buf, sizeof(tmp));

    setTmuTcr(0, tmp);

    return 0;
}

int Sh4::Tcor1RegReadHandler(void *buf, struct MemMappedReg const *reg_info) {
    uint32_t tmp;

    if (reg_info->len != sizeof(tmp))
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("Whatever happens when you don't "
                                              "supply the correct size while "
                                              "reading from or writing to a "
                                              "memory-mapped register"));

    tmp = getTmuTcor(1);
    memcpy(buf, &tmp, sizeof(tmp));

    return 0;
}

int Sh4::Tcor1RegWriteHandler(void const *buf, struct MemMappedReg const *reg_info) {
    uint32_t tmp;

    if (reg_info->len != sizeof(tmp))
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("Whatever happens when you don't "
                                              "supply the correct size while "
                                              "reading from or writing to a "
                                              "memory-mapped register"));

    memcpy(&tmp, buf, sizeof(tmp));

    setTmuTcor(1, tmp);

    return 0;
}

int Sh4::Tcnt1RegReadHandler(void *buf, struct MemMappedReg const *reg_info) {
    uint32_t tmp;

    if (reg_info->len != sizeof(tmp))
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("Whatever happens when you don't "
                                              "supply the correct size while "
                                              "reading from or writing to a "
                                              "memory-mapped register"));

    tmp = getTmuTcnt(1);
    memcpy(buf, &tmp, sizeof(tmp));

    return 0;
}

int Sh4::Tcnt1RegWriteHandler(void const *buf,
                              struct MemMappedReg const *reg_info) {
    uint32_t tmp;

    if (reg_info->len != sizeof(tmp))
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("Whatever happens when you don't "
                                              "supply the correct size while "
                                              "reading from or writing to a "
                                              "memory-mapped register"));

    memcpy(&tmp, buf, sizeof(tmp));

    setTmuTcnt(1, tmp);

    return 0;
}

int Sh4::Tcr1RegReadHandler(void *buf, struct MemMappedReg const *reg_info) {
    uint16_t tmp;

    if (reg_info->len != sizeof(tmp))
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("Whatever happens when you don't "
                                              "supply the correct size while "
                                              "reading from or writing to a "
                                              "memory-mapped register"));

    tmp = getTmuTcr(1);
    memcpy(buf, &tmp, sizeof(tmp));

    return 0;
}

int Sh4::Tcr1RegWriteHandler(void const *buf,
                             struct MemMappedReg const *reg_info) {
    uint16_t tmp;

    if (reg_info->len != sizeof(tmp))
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("Whatever happens when you don't "
                                              "supply the correct size while "
                                              "reading from or writing to a "
                                              "memory-mapped register"));

    memcpy(&tmp, buf, sizeof(tmp));

    setTmuTcr(1, tmp);

    return 0;
}

int Sh4::Tcor2RegReadHandler(void *buf, struct MemMappedReg const *reg_info) {
    uint32_t tmp;

    if (reg_info->len != sizeof(tmp))
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("Whatever happens when you don't "
                                              "supply the correct size while "
                                              "reading from or writing to a "
                                              "memory-mapped register"));

    tmp = getTmuTcor(2);
    memcpy(buf, &tmp, sizeof(tmp));

    return 0;
}

int Sh4::Tcor2RegWriteHandler(void const *buf,
                              struct MemMappedReg const *reg_info) {
    uint32_t tmp;

    if (reg_info->len != sizeof(tmp))
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("Whatever happens when you don't "
                                              "supply the correct size while "
                                              "reading from or writing to a "
                                              "memory-mapped register"));

    memcpy(&tmp, buf, sizeof(tmp));

    setTmuTcor(2, tmp);

    return 0;
}

int Sh4::Tcnt2RegReadHandler(void *buf, struct MemMappedReg const *reg_info) {
    uint32_t tmp;

    if (reg_info->len != sizeof(tmp))
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("Whatever happens when you don't "
                                              "supply the correct size while "
                                              "reading from or writing to a "
                                              "memory-mapped register"));

    tmp = getTmuTcnt(2);
    memcpy(buf, &tmp, sizeof(tmp));

    return 0;
}

int Sh4::Tcnt2RegWriteHandler(void const *buf,
                              struct MemMappedReg const *reg_info) {
    uint32_t tmp;

    if (reg_info->len != sizeof(tmp))
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("Whatever happens when you don't "
                                              "supply the correct size while "
                                              "reading from or writing to a "
                                              "memory-mapped register"));

    memcpy(&tmp, buf, sizeof(tmp));

    setTmuTcnt(2, tmp);

    return 0;
}

int Sh4::Tcr2RegReadHandler(void *buf, struct MemMappedReg const *reg_info) {
    uint16_t tmp;

    if (reg_info->len != sizeof(tmp))
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("Whatever happens when you don't "
                                              "supply the correct size while "
                                              "reading from or writing to a "
                                              "memory-mapped register"));

    tmp = getTmuTcr(2);
    memcpy(buf, &tmp, sizeof(tmp));

    return 0;
}

int Sh4::Tcr2RegWriteHandler(void const *buf,
                             struct MemMappedReg const *reg_info) {
    uint16_t tmp;

    if (reg_info->len != sizeof(tmp))
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("Whatever happens when you don't "
                                              "supply the correct size while "
                                              "reading from or writing to a "
                                              "memory-mapped register"));

    memcpy(&tmp, buf, sizeof(tmp));

    setTmuTcr(2, tmp);

    return 0;
}

int Sh4::Tcpr2RegReadHandler(void *buf, struct MemMappedReg const *reg_info) {
    uint32_t tmp;

    if (reg_info->len != sizeof(tmp))
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("Whatever happens when you don't "
                                              "supply the correct size while "
                                              "reading from or writing to a "
                                              "memory-mapped register"));

    tmp = getTmuTcpr2();
    memcpy(buf, &tmp, sizeof(tmp));

    return 0;
}

int Sh4::Tcpr2RegWriteHandler(void const *buf,
                              struct MemMappedReg const *reg_info) {
    uint32_t tmp;

    if (reg_info->len != sizeof(tmp))
        BOOST_THROW_EXCEPTION(UnimplementedError() <<
                              errinfo_feature("Whatever happens when you don't "
                                              "supply the correct size while "
                                              "reading from or writing to a "
                                              "memory-mapped register"));

    memcpy(&tmp, buf, sizeof(tmp));

    setTmuTcpr2(tmp);

    return 0;
}
