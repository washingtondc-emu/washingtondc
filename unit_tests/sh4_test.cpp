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

#include <iostream>
#include <list>

class Test {
public:
    virtual int run() = 0;
    virtual char const *name() = 0;
};

// the NullTest - does nothing, always passes
class NullTest : public Test {
    int run() {
        return 0;
    }

    virtual char const *name() {
        return "NullTest";
    }
};

typedef std::list<Test*> TestList;

static TestList tests;

void instantiate_tests() {
    tests.push_back(new NullTest);
}

void run_tests() {
    unsigned n_success = 0;
    unsigned n_tests = tests.size();

    for (TestList::iterator it = tests.begin(); it != tests.end(); it++) {
        char const *test_name = (*it)->name();
        std::cout << "Running " << test_name << "..." << std::endl;
        if ((*it)->run() == 0) {
            n_success++;
            std::cout << test_name << " completed successfully" << std::endl;
        } else {
            std::cout << test_name << " failed" << std::endl;
        }
    }

    double percent = 100.0 * double(n_success) / double(n_tests);
    std::cout << tests.size() << " tests run - " << n_success <<
        " successes " << "(" << percent << "%)" << std::endl;
}

int main(int argc, char **argv) {
    instantiate_tests();

    run_tests();

    return 0;
}
