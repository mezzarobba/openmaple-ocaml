#include <stdio.h>

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
 * Kernel vectors
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

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

MKernelVector
start_maple(void) {
    /* Maple searches for libraries and stuff using paths relative to
     * argv[0], which must therefore contain something like
     * ${MAPLE_ROOT}/bin.${ARCH}/maple */
    static char *argv[] = { // TBI
        "/home/mezzarob/opt/maple/13/bin.X86_64_LINUX/maple"
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
    MKernelVector kv;
    if( (kv=StartMaple(1,argv,&cb,NULL,NULL,err)) == NULL ) {
        printf("Unable to start Maple: %s\n",err);
        exit( 1 );
    } 
    printf("Maple kernel %lu started\n", (unsigned long) kv);
    EvalMapleStatement(kv, "\"it works\";");
    return kv;
    // Hum, il y a un truc louche dans la doc, je ne comprends pas si on
    // peut lancer plusieurs noyaux Maple ou non.
} 

CAMLprim value
start_maple_stub(void) {
    CAMLparam0 ();
    CAMLreturn (alloc_kernel_vector(start_maple()));
}

// il faudrait que je crée un type abstrait pour ALGEB, et que j'étende
// cette fonction pour renvoyer le résultat !
CAMLprim void
exec_maple_statement(value kv, value statement) {
    CAMLparam2(kv, statement);
    EvalMapleStatement(MKernelVector_val(kv), String_val(statement));
    CAMLreturn0;
}
