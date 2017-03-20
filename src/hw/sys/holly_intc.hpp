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

#ifndef HOLLY_INTC_HPP_
#define HOLLY_INTC_HPP_

enum HollyInt {
    HOLLY_INT_GDROM,

    HOLLY_INT_COUNT
};

const static unsigned HOLLY_REG_ISTEXT_GDROM_SHIFT = 0;
const static reg32_t HOLLY_REG_ISTEXT_GDROM_MASK =
    1 << HOLLY_REG_ISTEXT_GDROM_SHIFT;

void holly_raise_ext_int(HollyInt int_type);
void holly_clear_ext_int(HollyInt int_type);

int holly_reg_istnrm_read_handler(struct sys_mapped_reg const *reg_info,
                                  void *buf, addr32_t addr, unsigned len);
int holly_reg_istnrm_write_handler(struct sys_mapped_reg const *reg_info,
                                   void const *buf, addr32_t addr,
                                   unsigned len);
int holly_reg_istext_read_handler(struct sys_mapped_reg const *reg_info,
                                  void *buf, addr32_t addr, unsigned len);
int holly_reg_istext_write_handler(struct sys_mapped_reg const *reg_info,
                                   void const *buf, addr32_t addr,
                                   unsigned len);
int holly_reg_isterr_read_handler(struct sys_mapped_reg const *reg_info,
                                  void *buf, addr32_t addr, unsigned len);
int holly_reg_isterr_write_handler(struct sys_mapped_reg const *reg_info,
                                   void const *buf, addr32_t addr,
                                   unsigned len);
int holly_reg_iml2nrm_read_handler(struct sys_mapped_reg const *reg_info,
                                   void *buf, addr32_t addr, unsigned len);
int holly_reg_iml2nrm_write_handler(struct sys_mapped_reg const *reg_info,
                                    void const *buf, addr32_t addr,
                                    unsigned len);
int holly_reg_iml2err_read_handler(struct sys_mapped_reg const *reg_info,
                                   void *buf, addr32_t addr, unsigned len);
int holly_reg_iml2err_write_handler(struct sys_mapped_reg const *reg_info,
                                    void const *buf, addr32_t addr,
                                    unsigned len);
int holly_reg_iml2ext_read_handler(struct sys_mapped_reg const *reg_info,
                                   void *buf, addr32_t addr, unsigned len);
int holly_reg_iml2ext_write_handler(struct sys_mapped_reg const *reg_info,
                                    void const *buf, addr32_t addr,
                                    unsigned len);
int holly_reg_iml4nrm_read_handler(struct sys_mapped_reg const *reg_info,
                                   void *buf, addr32_t addr, unsigned len);
int holly_reg_iml4nrm_write_handler(struct sys_mapped_reg const *reg_info,
                                    void const *buf, addr32_t addr,
                                    unsigned len);
int holly_reg_iml4err_read_handler(struct sys_mapped_reg const *reg_info,
                                   void *buf, addr32_t addr, unsigned len);
int holly_reg_iml4err_write_handler(struct sys_mapped_reg const *reg_info,
                                    void const *buf, addr32_t addr,
                                    unsigned len);
int holly_reg_iml4ext_read_handler(struct sys_mapped_reg const *reg_info,
                                   void *buf, addr32_t addr, unsigned len);
int holly_reg_iml4ext_write_handler(struct sys_mapped_reg const *reg_info,
                                    void const *buf, addr32_t addr,
                                    unsigned len);
int holly_reg_iml6nrm_read_handler(struct sys_mapped_reg const *reg_info,
                                   void *buf, addr32_t addr, unsigned len);
int holly_reg_iml6nrm_write_handler(struct sys_mapped_reg const *reg_info,
                                    void const *buf, addr32_t addr,
                                    unsigned len);
int holly_reg_iml6err_read_handler(struct sys_mapped_reg const *reg_info,
                                   void *buf, addr32_t addr, unsigned len);
int holly_reg_iml6err_write_handler(struct sys_mapped_reg const *reg_info,
                                    void const *buf, addr32_t addr,
                                    unsigned len);
int holly_reg_iml6ext_read_handler(struct sys_mapped_reg const *reg_info,
                                   void *buf, addr32_t addr, unsigned len);
int holly_reg_iml6ext_write_handler(struct sys_mapped_reg const *reg_info,
                                    void const *buf, addr32_t addr,
                                    unsigned len);
#endif
