/* OCaml OpenMaple wrapper stubs. C99.
 * Written by Marc Mezzarobba <marc@mezzarobba.net>, 2010 */

/* TODO:
 * - faire sortir les erreurs Maple comme des exceptions Caml
 * - rediriger la sortie texte
 * - callbacks divers
 * - encore plein de fonctions utiles à encapsuler
 * - eval_int, eval_bool, assign_int, assign_bool, etc. qui évitent de
 *   passer à Caml l'ALGEB intermédiaire
 * - comparaison pour les objets ALGEB
 * - ...
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/alloc.h>
#include <caml/custom.h>
#include <caml/fail.h>
#include <caml/callback.h>

#include "maplec.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
 * Running the Maple kernel
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* OpenMaple does not support starting several kernels from the same
 * process (be it at once or one after another), although the API seems
 * to be designed to allow it in the future.  Let's keep things simple
 * for now and hide MKernelVectors from Caml. */

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
 * ALGEBs and ALGEBwrappers
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
 * To hand ALGEBs to OCaml, we encapsulate them in OCaml custom blocks.
 * Since the ALGEBs we receive from Maple may already be protected (this
 * happens, e.g., when they represent small integers), we also include
 * in each custom block a MaplePointer that we protect and use to mark
 * our ALGEB using MaplePointerSetMarkFunction().
 *
 * (Restricting the use of MaplePointers to ALGEBs that are already
 * protected when we get them would likely be more efficient, but the
 * Maple manual recommends using MaplePointers when possible.  This is
 * an easy change anyway.  Yet another (better?) strategy would be to
 * use a single MaplePointer and keep track ourselves of the
 * aliveness(?) of the wrappers.) */

/* The data part of our custom blocks */
typedef struct ALGEB_wrapper {
    ALGEB val;  /* the actual ALGEB object we want to access from Caml */
    ALGEB ptr;  /* MaplePointer to the struct itself, needed to
                       protect val from Maple garbage collection */
} ALGEB_wrapper;

static inline ALGEB_wrapper*
ALGEB_wrapper_val(value v) {
    return (ALGEB_wrapper *) Data_custom_val(v);
}

static inline ALGEB
ALGEB_val(value v) {
    return ALGEB_wrapper_val(v)->val;
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
 * side corresponding to an ALGEB value.  For now, let's finalize the
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

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
 * Eval, Assign and friends
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */ 

CAMLprim value
EvalMapleStatement_stub(value statement) {
    CAMLparam1(statement);
    ALGEB a = EvalMapleStatement(kv, String_val(statement));
    CAMLreturn (new_ALGEB_wrapper(a));
}

/* faudra un wrapper qui prenne une chaîne... */
CAMLprim void
MapleAssign_stub(value lhs, value rhs) {
    CAMLparam2(lhs, rhs);
    MapleAssign(kv, ALGEB_val(lhs), ALGEB_val(rhs));
    CAMLreturn0;
}

CAMLprim void
MapleAssignIndexed_stub(value name, /* array */ value indices, value rhs) {
    CAMLparam3(name, indices, rhs);
    M_INT dim = Wosize_val(indices);
    M_INT ind[dim];
    for (int i=0; i<dim; i++)
        ind[i] = Int_val(Field(indices, i));
    MapleAssignIndexed(kv, ALGEB_val(name), dim, ind, ALGEB_val(rhs));
    CAMLreturn0;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
 * Errors
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */ 

/* Raising Maple errors */

CAMLprim void
MapleRaiseError_stub(value msg) {
    CAMLparam1(msg);
    MapleRaiseError(kv, String_val(msg));
    CAMLreturn0;
}

CAMLprim void
MapleRaiseError1_stub(value msg, value arg1) {
    CAMLparam2(msg, arg1);
    MapleRaiseError1(kv, String_val(msg), ALGEB_val(arg1));
    CAMLreturn0;
}

CAMLprim void
MapleRaiseError2_stub(value msg, value arg1, value arg2) {
    CAMLparam3(msg, arg1, arg2);
    MapleRaiseError2(kv, String_val(msg), ALGEB_val(arg1), ALGEB_val(arg2));
    CAMLreturn0;
}

/* Raising custom Caml exceptions */

void
raise_MapleError(char *msg) {
    caml_raise_with_string(
            *caml_named_value(
                "net.mezzarobba.openmaple-ocaml.OpenMaple.MapleError"),
            msg);
}

void
raise_SyntaxError(long offset, char *msg) {
    value args[] = { Val_long(offset), caml_copy_string(msg)};
    caml_raise_with_args(
            *caml_named_value(
                "net.mezzarobba.openmaple-ocaml.OpenMaple.SyntaxError"),
            2, args);
}

CAMLprim void
ploum(void) {
    CAMLparam0 ();
    raise_SyntaxError(42, "coucou");
    CAMLreturn0;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
 * Conversions from and to ALGEB
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */ 

/* Template for trivial conversions */

#define MAPLE_TO(CAML_NAME, TYPE, TO_CAML) \
    CAMLprim value \
    MapleTo ## TYPE ## _stub(value v) { \
        CAMLparam1(v); \
        ALGEB a = ALGEB_val(v); \
        if (!IsMaple ## TYPE (kv, a)) \
            caml_failwith( #CAML_NAME ); \
        value res = TO_CAML(MapleTo ## TYPE (kv, a)); \
        CAMLreturn (res); \
    }

/* (unboxed) int*/

CAMLprim value
ToMapleInteger_stub_unboxed(value n) {
    CAMLparam1(n);
    ALGEB a = ToMapleInteger(kv, (M_INT) Long_val(n));
    CAMLreturn (new_ALGEB_wrapper(a));
}

typedef struct {
    ALGEB a;
    M_INT res;
} MapleToM_INT_wrapper_data;

static void*
MapleToM_INT_wrapper(void* arg) {
    MapleToM_INT_wrapper_data *data = (MapleToM_INT_wrapper_data *) arg;
    data->res = MapleToM_INT(kv, data->a);
    return NULL;
}

CAMLprim value
MapleToM_INT_stub_unboxed(value v) {
    CAMLparam1(v);
    MapleToM_INT_wrapper_data data;
    data.a = ALGEB_val(v);
    M_BOOL errflag = FALSE;
    MapleTrapError(kv, &MapleToM_INT_wrapper, &data, &errflag);
    if (errflag == TRUE || data.res < Min_long || data.res > Max_long)
        caml_failwith("int_of_algeb");
    CAMLreturn (Val_long(data.res));
}

/* nativeint */

CAMLprim value
ToMapleInteger_stub_native(value n) {
    CAMLparam1(n);
    ALGEB a = ToMapleInteger(kv, (M_INT) Nativeint_val(n));
    CAMLreturn (new_ALGEB_wrapper(a));
}

CAMLprim value
MapleToM_INT_stub_native(value v) {
    CAMLparam1(v);
    MapleToM_INT_wrapper_data data;
    data.a = ALGEB_val(v);
    M_BOOL errflag = FALSE;
    MapleTrapError(kv, &MapleToM_INT_wrapper, &data, &errflag);
    if (errflag == TRUE)
        caml_failwith("nativeint_of_algeb");
    CAMLreturn (caml_copy_nativeint(data.res));
}

/* int32, int64 */

#define TO_MAPLE_INTEGER(SIZE, TO_MAPLE) \
    CAMLprim value \
    ToMapleInteger ## SIZE ## _stub(value n) { \
        CAMLparam1(n); \
        ALGEB a = TO_MAPLE(kv, (M_INT) Int ## SIZE ## _val(n)); \
        CAMLreturn (new_ALGEB_wrapper(a)); \
    }

MAPLE_TO(int32_of_algeb, Integer32, caml_copy_int32)
TO_MAPLE_INTEGER(32, ToMapleInteger)
MAPLE_TO(int64_of_algeb, Integer64, caml_copy_int64)
TO_MAPLE_INTEGER(64, ToMapleInteger64)

#undef TO_MAPLE_INTEGER

/* Strings and Maple names */

CAMLprim value 
ToMapleString_stub(value v) {
    CAMLparam1(v);
    if (caml_string_length(v) > strlen(String_val(v)))
        /* null char in the middle of the string */
        caml_failwith("algeb_of_string");
    /* copies the string */
    ALGEB res = ToMapleString(kv, String_val(v));
    CAMLreturn (new_ALGEB_wrapper(res));
}

MAPLE_TO(string_of_algeb, String, caml_copy_string)

CAMLprim value
ToMapleName_stub(value name, value global) {
    CAMLparam2(name, global);
    if (caml_string_length(name) > strlen(String_val(name)))
        /* null char in the middle of the string */
        caml_failwith("OpenMaple.string_to_name");
    ALGEB res = ToMapleName(kv, String_val(name), Int_val(global));
    CAMLreturn (new_ALGEB_wrapper(res));
}

#undef MAPLE_TO


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
 * Debug & test
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */ 

CAMLprim void 
dbg_print (value v) {
    CAMLparam1(v);
    printf("wrapper=%lu, value=", v);
    MapleALGEB_Printf(kv, "%a\n", ALGEB_wrapper_val(v)->val);
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
