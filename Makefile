MAPLE=${HOME}/opt/maple/13
MAPLE_ARCH=X86_64_LINUX

.PHONY: clean lib run top

lib: openMaple.cma openMaple.cmxa

openMaple_stubs.o: openMaple_stubs.c
	gcc -c -std=c99 -fPIC \
	    -Wl,-rpath=${MAPLE}/bin.${MAPLE_ARCH} \
	    -L${MAPLE}/bin.${MAPLE_ARCH} \
	    -I${MAPLE}/extern/include \
	    $^

openMaple.cma openMaple.cmo openMaple.cmxa: openMaple_stubs.o openMaple.ml
	ocamlmklib -o openMaple \
			-ccopt -L${MAPLE}/bin.${MAPLE_ARCH} \
			-ccopt -Wl,-rpath=${MAPLE}/bin.${MAPLE_ARCH} \
			-cclib -lmaplec \
			-cclib -lrt \
			$^

test: openMaple.cmxa test.ml
	ocamlopt -o $@ -ccopt -L. $^

clean:
	rm -f *.cm* *.[oa] dllopenMaple.so test top

top: openMaple.cma
	 ocamlmktop -custom -o $@ -ccopt -L. $^

run: test
	MAPLE=${MAPLE} ./test

runtop: top
	MAPLE=${MAPLE} ./top
