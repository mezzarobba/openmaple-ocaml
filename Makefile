MAPLE_ROOT=${HOME}/opt/maple/13
MAPLE_BIN=${MAPLE_ROOT}/bin.X86_64_LINUX

.PHONY: clean run

test: openMaple.mli openMaple_stubs.c test.ml
	LD_LIBRARY_PATH=${MAPLE_BIN} ocamlopt -o $@ \
			-ccopt -L${MAPLE_BIN} \
			-ccopt -I${MAPLE_ROOT}/extern/include \
			-cclib -lmaplec \
			$?

clean:
	rm -f *.o *.cm? test

run: test
	MAPLE_PATH=${MAPLE_BIN}/maple LD_LIBRARY_PATH=${MAPLE_BIN} ./test
