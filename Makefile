MAPLE_ROOT=${HOME}/opt/maple/13
MAPLE_BIN=${MAPLE_ROOT}/bin.X86_64_LINUX

.PHONY: clean run

test: openMaple.mli openMaple_stubs.c test.ml
	LD_LIBRARY_PATH=${MAPLE_BIN} ocamlopt -o $@ \
			-ccopt -L${MAPLE_BIN} \
			-ccopt -I${MAPLE_ROOT}/extern/include \
			-cclib -lmaplec \
			-cclib -lrt \
			$?
	rm -f openMaple_stubs.o  test.o openMaple.cmi  test.cmi \
	    test.cmx

run: test
	MAPLE=${MAPLE_ROOT} LD_LIBRARY_PATH=${MAPLE_BIN} ./test
