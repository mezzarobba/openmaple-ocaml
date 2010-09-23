MAPLE_ROOT=${HOME}/opt/maple/13
MAPLE_BIN=${MAPLE_ROOT}/bin.X86_64_LINUX

.PHONY: run

test: ocamlOpenMaple.ml ocamlOpenMaple_stubs.c
	LD_LIBRARY_PATH=${MAPLE_BIN} ocamlopt -o $@ \
			-ccopt -L${MAPLE_BIN} \
			-ccopt -I${MAPLE_ROOT}/extern/include \
			-cclib -lmaplec \
			$?

run: test
	LD_LIBRARY_PATH=${MAPLE_BIN} ./test
