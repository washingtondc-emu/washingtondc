all:
	make -C src/hw/sh4

test:
	make -C unit_tests/ test

clean:
	make -C unit_tests/ clean
	make -C src/hw/sh4 clean
