all: libsh4 libsh4asm

libsh4:
	make -C src/hw/sh4

libsh4asm:
	make -C src/tool/sh4asm

test:
	make -C unit_tests/ test

clean:
	make -C unit_tests/ clean
	make -C src/hw/sh4 clean
	make -C src/tool/sh4asm clean
