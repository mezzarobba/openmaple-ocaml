MAPLE_ROOT=${HOME}/opt/maple/13
MAPLE_BIN=${MAPLE_ROOT}/bin.X86_64_LINUX

.PHONY: clean run

# Ce Makefile est tout pourri, mais j'ai oublié le peu que j'avais
# compris concernant ocamlbuild et tout ça.

test: openMaple_stubs.c openMaple.ml test.ml
	rm -f openMaple_stubs.o  test.o openMaple.cmi openMaple.cmx test.cmi \
	    test.cmx test
	LD_LIBRARY_PATH=${MAPLE_BIN} ocamlopt -g -o $@ \
			-ccopt -std=c99 \
			-ccopt -L${MAPLE_BIN} \
			-ccopt -I${MAPLE_ROOT}/extern/include \
			-cclib -lmaplec \
			-cclib -lrt \
			$^

run: test
	MAPLE=${MAPLE_ROOT} LD_LIBRARY_PATH=${MAPLE_BIN} ./test
