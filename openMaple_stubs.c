/* C99 */

#include <stdio.h>
#include <stdlib.h>

#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/alloc.h>
#include <caml/custom.h>
#include <caml/fail.h>

#include "maplec.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
 * Running the Maple kernel
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* OpenMaple does not support starting several kernels from the same
 * process (be it at once or one after another), although the API seems
 * to be designed with this feature in mind.  Let's keep things simple
 * and hide MKernelVectors from Caml. */

static MKernelVector kv;

void
StartMaple_stub(void) {
    CAMLparam0 ();
    /* With argv[0] set to "maple", Maple uses $MAPLE to search for
     * libraries and stuff. See
     * http://www.mapleprimes.com/questions/42618-Seeking-C-Library-For-Symbolic-Manipulation
     */
    char *argv[] = { "maple" };
    const int err_size = 2048;
    char err[err_size];  // used to report errors during initialization
    static MCallBackVectorDesc cb = {
        0,   /* textCallBack */
        0,   /* errorCallBack not used */
        0,   /* statusCallBack not used */
        0,   /* readLineCallBack not used */
        0,   /* redirectCallBack not used */
        0,   /* streamCallBack not used */
        0,   /* queryInterrupt not used */
        0    /* callBackCallBack not used */
    };
    if( (kv = StartMaple(1, argv, &cb, NULL, NULL, err)) == NULL ) {
        const int msg_size = err_size + 25;
        char msg[msg_size];
        snprintf(msg, msg_size, "Unable to start Maple: %s\n", err);
        caml_failwith(msg);
    } 
    CAMLreturn0;
} 

CAMLprim void
StopMaple_stub(void) {
    CAMLparam0 ();
    StopMaple(kv);
    CAMLreturn0;
}

CAMLprim void
RestartMaple_stub(void) {
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

/* ALGEB is the C data type of pointers to abstract Maple objects.  The
 * objects returned as ALGEB values by OpenMaple API functions may still
 * get garbage-collected as soon as the control goes back to the Maple
 * server, unless they are protected using MapleGcProtect().  (This is
 * not completely explicit in the manual, but clear from experiments:
 * juste create a large Maple expression, call gc(), and try to access
 * it again.)
 *
 * (AFAICT there is no way to test whether an [unprotected] ALGEB is
 * still valid without risking crashing the Maple server.  So no "weak
 * Maple pointers".)
 *
 * To hand ALGEBs to OCaml, we protect them and encapsulate them in Caml
 * custom blocks.  The following macro accesses the ALGEB part of such a
 * block. */

typedef struct ALGEB_wrapper {
    ALGEB val;      /* the actual ALGEB object we want to access from Caml */
    ALGEB ptr;  /* MaplePointer to the struct itself, needed to
                       protect val from Maple garbage collection */
} ALGEB_wrapper;

//#define ALGEB_wrapper_val(v) (*((ALGEB_wrapper *) Data_custom_val(v)))

static inline ALGEB_wrapper*
ALGEB_wrapper_val(value v) {
    return (ALGEB_wrapper *) Data_custom_val(v);
}

static inline ALGEB
MaplePointer_to_ALGEB(ALGEB p) {
    return  *((ALGEB *)MapleToPointer(kv, p));
}


/* Custom operations.  Quoting from the OCaml manual: "Do not use
 * CAMLparam to register the parameters to these functions, and do not
 * use CAMLreturn to return the result." */

static void
finalize_ALGEB_wrapper(value v) {
    printf("FINALIZE %lu\n", v);
    MapleGcAllow(kv, ALGEB_wrapper_val(v)->ptr);
}


static struct custom_operations ALGEB_wrapper_ops = {
    "net.mezzarobba.openmaple-ocaml.ALGEB_wrapper-v0.1",
    &finalize_ALGEB_wrapper,
    custom_compare_default,
    custom_hash_default,
    custom_serialize_default,
    custom_deserialize_default
};

static void M_DECL
mark_ALGEB(ALGEB maple_pointer) {
    MapleGcMark(kv, MaplePointer_to_ALGEB(maple_pointer));
}

static const M_INT ALGEB_WRAPPER_POINTER = (M_INT) &mark_ALGEB;

/* I see no simple estimate for the amount of resources on the Maple
 * size corresponding to an ALGEB value.  For now, let's finalize the
 * Caml values pointing to Maple objects quite aggressively.  Should we
 * also have OCaml's GC call Maple's?  (Damien?) */
static const unsigned int MAX_DANGLING_ALGEB = 100;

static value
new_ALGEB_wrapper(ALGEB a) {
    value v = caml_alloc_custom(&ALGEB_wrapper_ops,
                          sizeof(ALGEB_wrapper), 1, MAX_DANGLING_ALGEB);
    ALGEB p = ToMaplePointer(kv, Data_custom_val(v), ALGEB_WRAPPER_POINTER);
    if (MapleGcIsProtected(kv, p) == TRUE)
        caml_failwith("Got a GcProtected MaplePointer from Maple?!");
    MapleGcProtect(kv, p);
    MaplePointerSetMarkFunction(kv, p, &mark_ALGEB);
    ALGEB_wrapper_val(v)->val = a;
    ALGEB_wrapper_val(v)->ptr = p;
    return v;
}

CAMLprim value
EvalMapleStatement_stub(value statement) {
    CAMLparam1(statement);
    ALGEB a = EvalMapleStatement(kv, String_val(statement));
    CAMLreturn (new_ALGEB_wrapper(a));
}

CAMLprim void 
my_print (value v) {
    CAMLparam1(v);
    printf("would print %lu\n", v);
    //MapleALGEB_Printf(kv, "%a\n", ALGEB_wrapper_val(v)->val);
    CAMLreturn0;
}

#if 0

/* Voyons comment protéger un objet via un MaplePointer */
CAMLprim void
ploum(void) {
    CAMLparam0 ();
    ALGEB * a = malloc(sizeof(ALGEB));
    *a = EvalMapleStatement(kv, "diff(x^(x^(x^(x^x))), x, x, x, x, x);");
    ALGEB p = ToMaplePointer(kv, a, MY_MAPLE_POINTER_TYPE);
    EvalMapleStatement(kv, "a;");
    EvalMapleStatement(kv, "a;");
    EvalMapleStatement(kv, "a;");
    EvalMapleStatement(kv, "a;");
    EvalMapleStatement(kv, "a;");
    EvalMapleStatement(kv, "a;");
    if (MapleGcIsProtected(kv, p) == TRUE)
        caml_failwith("deja protege");
    //MapleGcProtect(kv, p);
    MaplePointerSetMarkFunction(kv, p, &mark_ALGEB);
    EvalMapleStatement(kv, "gc();");
    MapleALGEB_Printf(kv, "%a", MaplePointer_to_ALGEB(p));
    MapleALGEB_Printf(kv, "%a", p);
    CAMLreturn0;
}

#endif
