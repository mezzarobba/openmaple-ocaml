#include <stdio.h>
#include <stdlib.h>

#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/alloc.h>
#include <caml/custom.h>

//TBI
#include "maplec.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
 * Test
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static int
my_add(int x, int y) {
    return x + y;
}

CAMLprim value
my_add_stub(value x, value y) {
    CAMLparam2 (x, y);
    CAMLreturn (Val_int(my_add(Int_val(x), Int_val(y))));
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
 * Maple kernel and kernel vector
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* OpenMaple does not support starting several kernels from the same
 * process (be it at once or one after another), although the API seems
 * to be designed to allow it later.  Hence, for now, we hide
 * MKernelVectors completely. */

static MKernelVector kv;

void
start_maple(void) {
    /* Maple searches for libraries and stuff using paths relative to
     * argv[0], which must therefore contain something like
     * ${MAPLE_ROOT}/bin.${ARCH}/maple */
    char *argv[] = { // TBI
        //"/home/mezzarob/opt/maple/13/bin.X86_64_LINUX/maple"
        getenv("MAPLE_PATH")
    };
    char err[2048];  // used to report errors during initialization
    static MCallBackVectorDesc cb = {
        0, // textCallBack, 
        0,   /* errorCallBack not used */
        0,   /* statusCallBack not used */
        0,   /* readLineCallBack not used */
        0,   /* redirectCallBack not used */
        0,   /* streamCallBack not used */
        0,   /* queryInterrupt not used */
        0    /* callBackCallBack not used */
    };
    if( (kv=StartMaple(1,argv,&cb,NULL,NULL,err)) == NULL ) {
        printf("Unable to start Maple: %s\n",err);
        exit( 1 );
    } 
    EvalMapleStatement(kv, "\"it works\";");
} 

CAMLprim void
start_maple_stub(void) {
    CAMLparam0 ();
    start_maple();
    CAMLreturn0;
}

CAMLprim void
stop_maple_stub(void) {
    CAMLparam0 ();
    StopMaple(kv);
    CAMLreturn0;
}

CAMLprim void
restart_maple_stub(void) {
    CAMLparam0 ();
    char err[2048];
    if (RestartMaple(kv, err) == FALSE) {
        // déclencher une exception ?
        printf("Unable to restart Maple: %s\n", err);
    }
    CAMLreturn0;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
 * The ALGEB type
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#if 0

// Custom operations. "Do not use CAMLparam to register the parameters to
// these functions, and do not use CAMLreturn to return the result."

// Accessing the WINDOW * part of a Caml custom block
#define MKernelVector_val(v) (*((MKernelVector *) Data_custom_val(v)))

void
finalize_kernel_vector(value v) {
    MKernelVector kv = MKernelVector_val(v);
    StopMaple(kv);
    printf("bye %lu\n", (unsigned long) kv);
}

static struct custom_operations kernel_vector_ops = {
    "net.mezzarobba.ocaml-openmaple.kernel_vector",
    &finalize_kernel_vector,
    custom_compare_default,
    custom_hash_default,
    custom_serialize_default,
    custom_deserialize_default
};

static value
alloc_kernel_vector(MKernelVector kv) {
    value v = alloc_custom(&kernel_vector_ops, sizeof(MKernelVector), 0, 1);
    MKernelVector_val(v) = kv;
    return v;
}

#endif

// il faudrait que je crée un type abstrait pour ALGEB, et que j'étende
// cette fonction pour renvoyer le résultat !
CAMLprim void
exec_maple_statement(value statement) {
    CAMLparam1(statement);
    EvalMapleStatement(kv, String_val(statement));
    CAMLreturn0;
}


