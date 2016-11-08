/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2016 snickerbockers
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

#ifndef RANDGENERATOR_HPP_
#define RANDGENERATOR_HPP_

#include <cstdlib>

// Generator that returns pseudo-random values.
template<typename T>
class RandGenerator {
public:
    RandGenerator() {
        this->seed = time(NULL);
        this->first_val = true;
    }

    RandGenerator(unsigned int seed) {
        this->seed = seed;
    }

    /*
     * cause subsequent calls to pick_val to return the same values as they did
     * after the last time reset was called for this generator.
     *
     * YOU MUST CALL RESET YOURSELF BEFORE THE FIRST CALL TO pic_val
     */
    void reset() {
        if (first_val) {
            std::cout << name() << " using seed=" << this->seed << std::endl;
            first_val = false;
        }
        srand(this->seed);
    }

    T pick_val(addr32_t addr) {
        return (T)rand();
    }

    std::string name() const {
        std::stringstream ss;
        ss << "RandGenerator<" << (sizeof(T) * 8) << " bits>";
        return ss.str();
    }
private:
    unsigned seed;
    bool first_val; // used to print the 'using seed=' message only once
};

/*
 * on x86_64, the rand function returns a 32-bit int, so we uint64_t needs a
 * special version of RandGenerator that will combine two calls to rand into a
 * 64-bit int.
 */
template<>
class RandGenerator<boost::uint64_t> {
public:
    RandGenerator() {
        this->seed = time(NULL);
        this->first_val = true;
    }

    RandGenerator(unsigned int seed) {
        this->seed = seed;
    }

    /*
     * cause subsequent calls to pick_val to return the same values as they did
     * after the last time reset was called for this generator.
     *
     * YOU MUST CALL RESET YOURSELF BEFORE THE FIRST CALL TO pic_val
     */
    void reset() {
        if (first_val) {
            std::cout << name() << " using seed=" << this->seed << std::endl;
            first_val = false;
        }
        srand(this->seed);
    }

    /*
     * The reason this function ands with 0xffffffff is that it is theoretically
     * possible that there may be some platform where sizeof(int) is actually 8
     * and not 4.
     */
    boost::uint64_t pick_val(addr32_t addr) {
        return boost::uint64_t(rand() & 0xffffffff) |
            (boost::uint64_t(rand() & 0xffffffff) << 32);
    }

    std::string name() const {
        std::stringstream ss;
        ss << "RandGenerator<" << (sizeof(boost::uint64_t) * 8) << " bits>";
        return ss.str();
    }
private:
    unsigned seed;
    bool first_val; // used to print the 'using seed=' message only once
};

#endif
