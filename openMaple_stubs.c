/* OCaml OpenMaple wrapper stubs. C99.
 * Written by Marc Mezzarobba <marc@mezzarobba.net>, 2010 */

/* TODO:
 * - encore plein de fonctions utiles à encapsuler
 * - eval_int, eval_bool, assign_int, assign_bool, etc. qui évitent de
 *   passer à Caml l'ALGEB intermédiaire
 * - améliorer l'interaction des deux GC
 * - arithmétique de base ?
 * - big_int ?
 * - ...
 *
 * NOTES:
 * - les fonctions non documentées (?) GetMapleID, MapleNew, MapleCreate,
 *   createALGEB, MapleGetElems (=op ?) ont l'air intéressantes
 * - voir aussi les xx.*
 */

/* À chaque fois que l'on passe une chaîne de caractères de Maple à OCaml, on la
 * copie. Ce n'est pas un drame, mais c'est un peu bête vu qu'on ne veut en
 * général y accéder qu'en lecture, et ça pourrait devenir sensible en cas de
 * grosses sorties ou d'utilisation intensive de textCallBack. Il serait en
 * principe possible de se faire un type de chaînes immuables (et dont la
 * longueur serait calculée différemment des chaînes Caml) qui puisse embarquer
 * directement une chaîne sous le contrôle du GC de Maple... */

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

#define CAML_NAME_SPACE

#define PACKAGE "net.mezzarobba.openmaple-ocaml"

#define GET_CLOSURE(CLOSURE, NAME) \
    static value *CLOSURE = NULL; \
    if (CLOSURE == NULL) \
        CLOSURE = caml_named_value(PACKAGE #NAME );

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
 * Caml exceptions
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
static void
raise_MapleError(char *msg) {
    caml_raise_with_string(
            *caml_named_value(
                PACKAGE ".OpenMaple.MapleError"),
            msg);
}

static void
raise_SyntaxError(long offset, char *msg) {
    value args[] = { Val_long(offset), caml_copy_string(msg)};
    caml_raise_with_args(
            *caml_named_value(
                PACKAGE ".OpenMaple.SyntaxError"),
            2, args);
}
*/

static inline void
raise_BooleanFail() {
    caml_raise(*caml_named_value(PACKAGE ".OpenMaple.BooleanFail"));
}

static void
raise_TypeError(char *msg) {
    caml_raise_with_string(
            *caml_named_value( PACKAGE ".OpenMaple.TypeError"),
            msg);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
 * Running the Maple kernel
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* OpenMaple does not support starting several kernels from the same
 * process (be it at once or one after another), although the API seems
 * to be designed to allow it in the future.  Let's keep things simple
 * for now and hide MKernelVectors from Caml. */

static MKernelVector kv;

/* How many pointers to Maple values we hold */
static unsigned long ALGEB_wrapper_count = 0;

/* Redirect OpenMaple callbacks to Caml.
 *
 * We ignore the user_data parameters: they are not much use to us, since the
 * callback functions we register from Caml are closures, not mere function
 * pointers. */

static inline int
maple_to_caml_text_output_tag(int maple_tag) {
    switch (maple_tag) {
        case MAPLE_TEXT_DIAG:    return 0;
        case MAPLE_TEXT_MISC:    return 1;
        case MAPLE_TEXT_OUTPUT:  return 2;
        case MAPLE_TEXT_QUIT:    return 3;
        case MAPLE_TEXT_WARNING: return 4;
        case MAPLE_TEXT_ERROR:   return 5;
        case MAPLE_TEXT_STATUS:  return 6;
        case MAPLE_TEXT_PRETTY:  return 7;
        case MAPLE_TEXT_HELP:    return 8;
        case MAPLE_TEXT_DEBUG:   return 9;
        default: caml_failwith("unknown text output tag");
    }
}

static inline value
maple_to_caml_bool(M_BOOL b) {
    CAMLparam0 ();
    if (b == TRUE)
        CAMLreturn(Val_true);
    else if (b == FALSE)
        CAMLreturn(Val_false);
    else
        raise_BooleanFail();
    CAMLreturn(Val_false); /* prevent warning */
}

static void M_DECL
textCallBack_caml(void *data, int tag, char *output) {
    GET_CLOSURE(closure, .textCallBack);
    int caml_tag = maple_to_caml_text_output_tag(tag);
    caml_callback2(*closure, Val_int(caml_tag), caml_copy_string(output));
}

static void M_DECL
errorCallBack_caml(void *data, M_INT offset, char *msg) {
    GET_CLOSURE(closure, .errorCallBack);
    caml_callback2(*closure, Val_long(offset), caml_copy_string(msg));
}

static void M_DECL
statusCallBack_caml(void *data, long kilobytesUsed, long kilobytesAlloc, 
        double cpuTime) {
    GET_CLOSURE(closure, .statusCallBack);
    caml_callback3(*closure, Val_long(kilobytesUsed), Val_long(kilobytesAlloc),
            caml_copy_double(cpuTime));
}

static char * M_DECL
readLineCallBack_caml(void *data, M_BOOL debug) {
    CAMLparam0 ();
    CAMLlocal1(ans);
    GET_CLOSURE(closure, .readLineCallBack);
    ans = caml_callback(*closure, maple_to_caml_bool(debug));
    CAMLreturnT(char *, String_val(ans));
}

/* non testé */
static M_BOOL M_DECL
redirectCallBack_caml(void *data, char *name, char *mode) {
    GET_CLOSURE(closures, .redirectCallBack);
    if (name != NULL)
        return Bool_val(caml_callback2(Field(*closures, 0),
                    caml_copy_string(name), caml_copy_string(mode)));
    else
        return Bool_val(caml_callback(Field(*closures, 1), Val_int(0)));
}

static inline char *
string_or_null(value option) {
    CAMLparam1(option);
    /* 'option' must be a Caml value of type string option */
    if (Is_long(option)) /* constant constructor, i.e. None */
        CAMLreturnT(char*, NULL);
    else
        CAMLreturnT(char*, String_val(Field(option, 0)));
}

/* non testé */
static char * M_DECL
streamCallBack_caml(void *data, char *name, int nargs, char **args) {
    CAMLlocal1(args_value);
    GET_CLOSURE(closure, .streamCallBack);
    args_value = caml_alloc_tuple(nargs);
    for (int i = 0; i < nargs; i++)
        Store_field(args_value, i, caml_copy_string(args[i]));
    return string_or_null(caml_callback2(*closure, caml_copy_string(name),
                args_value));
}

/* XXX: On dirait que queryInterrupt() n'est jamais appelé, je ne comprends pas
 * pourquoi. Idem avec le programme d'exemple fourni. */
static M_BOOL M_DECL
queryInterrupt_caml(void *data) {
    GET_CLOSURE(closure, .queryInterrupt);
    return maple_to_caml_bool(caml_callback(*closure, Int_val(0)));
}

/* non testé */
static char * M_DECL
callBackCallBack_caml(void *data, char *output) {
    GET_CLOSURE(closure, .callBackCallBack);
    return string_or_null(caml_callback(*closure, caml_copy_string(output)));
} 

/* void M_DECL
errorCallBack_raise_caml_exception(void *data, M_INT offset, char *msg) {
    if (offset >= 0)
        raise_SyntaxError(offset, msg);
    else
        raise_MapleError(msg);
} */

/* Start, stop, restart */

CAMLprim void
StartMaple_stub(value args, value cbmask) {
    CAMLparam2 (args, cbmask);
    int argc = Wosize_val(args);
    char *argv[argc];
    for (int i = 0; i < argc; i++)
        argv[i] = String_val(Field(args, i));
    /* Install those callbacks that are set in cbmask */
    MCallBackVectorDesc cb = {
        Bool_val(Field(cbmask,0)) ? &textCallBack_caml     : 0,
        Bool_val(Field(cbmask,1)) ? &errorCallBack_caml    : 0,
        Bool_val(Field(cbmask,2)) ? &statusCallBack_caml   : 0,
        Bool_val(Field(cbmask,3)) ? &readLineCallBack_caml : 0,
        Bool_val(Field(cbmask,4)) ? &redirectCallBack_caml : 0,
        Bool_val(Field(cbmask,5)) ? &streamCallBack_caml   : 0,
        Bool_val(Field(cbmask,6)) ? &queryInterrupt_caml   : 0,
        Bool_val(Field(cbmask,7)) ? &callBackCallBack_caml : 0
    };
    const int err_size = 2048;
    char err[err_size];  /* used to report errors during initialization */
    kv = StartMaple(argc, argv, &cb, NULL, NULL, err);
    if (kv == NULL) {
        const char fmt[] = "Unable to start Maple: %s\n";
        const int msg_size = err_size + sizeof(fmt);
        char msg[msg_size];
        snprintf(msg, msg_size, fmt, err);
        caml_failwith(msg);
    } 
    CAMLreturn0;
} 

CAMLprim void
StopMaple_unsafe_stub(void) {
    CAMLparam0 ();
    StopMaple(kv);
    CAMLreturn0;
}

CAMLprim void
RestartMaple_unsafe_stub(void) {
    CAMLparam0 ();
    const int err_size = 2048;
    char err[err_size];
    if (RestartMaple(kv, err) == FALSE) {
        const char fmt[] = "restart_maple: error while trying to restart: %s\n";
        const int msg_size = err_size + sizeof(fmt);
        char msg[msg_size];
        snprintf(msg, msg_size, fmt, err);
        caml_failwith(msg);
    }
    CAMLreturn0;
}

#define DEF_STOP_RESTART_UNSAFE(ACTION, LC_ACTION) \
    CAMLprim void \
    ACTION ## Maple_safe_stub(void) { \
        CAMLparam0 (); \
        if (ALGEB_wrapper_count > 0) { \
            GET_CLOSURE(closure, .caml_gc_full_major); \
            caml_callback(*closure, Val_int(0)); \
        } \
        if (ALGEB_wrapper_count > 0) \
            caml_failwith(#LC_ACTION "_maple: will not " #LC_ACTION " Maple " \
                    "with Maple objects still accessible from Caml code"); \
        ACTION ## Maple_unsafe_stub(); \
        CAMLreturn0; \
    }

DEF_STOP_RESTART_UNSAFE(Stop, stop)
DEF_STOP_RESTART_UNSAFE(Restart, restart)

#undef DEF_STOP_RESTART_UNSAFE

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
 * ALGEBs and ALGEBwrappers
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* ALGEB is the C data type of pointers to abstract Maple objects.  They (seem
 * to) correspond to the addresses used by addressof and friends.  The objects
 * returned as ALGEB values by OpenMaple API functions may still get
 * garbage-collected as soon as the control goes back to the Maple server,
 * unless they are protected using MapleGcProtect().  (This is not completely
 * explicit in the manual, but clear from experiments: juste create a large
 * Maple expression, call gc(), and try to access it again.)
 *
 * (AFAICT there is no way to test whether an [unprotected] ALGEB is still valid
 * without risking crashing the Maple server.  So no "weak Maple pointers".)
 *
 * To hand ALGEBs to OCaml, we encapsulate them in OCaml custom blocks. Since
 * the ALGEBs we receive from Maple may already be protected (this happens,
 * e.g., when they represent small integers), we also include in each custom
 * block a MaplePointer that we protect and use to mark our ALGEB using
 * MaplePointerSetMarkFunction().
 *
 * (Restricting the use of MaplePointers to ALGEBs that are already protected
 * when we get them would likely be more efficient, but the Maple manual
 * recommends using MaplePointers when possible.  This is an easy change anyway.
 * Yet another (better?) strategy would be to use a single MaplePointer and keep
 * track ourselves of the aliveness(?) of the wrappers.) */

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
    --ALGEB_wrapper_count;
    MapleGcAllow(kv, ALGEB_wrapper_val(v)->ptr);
}

/* Sort by address, à la Maple. */
static int
compare_ALGEB_wrapper(value v1, value v2) {
    return (int) (((long) ALGEB_val(v1)) - ((long) ALGEB_val(v2)));
}

static long
hash_ALGEB_wrapper(value v) {
    return (long) ALGEB_val(v);
}

/* On pourrait imaginer si nécessaire de supporter serialize/deserialize en
 * appelant save() ou XMLTools:-Serialize(), mais ça ne permet AFAII que de
 * sauvegarder des valeurs, pas l'état entier de la session distante. J'ai
 * l'impression qu'il vaut mieux laisser ça à l'application. */

static struct custom_operations ALGEB_wrapper_ops = {
    ".ALGEB_wrapper-v0.1",
    &finalize_ALGEB_wrapper,
    &compare_ALGEB_wrapper,
    &hash_ALGEB_wrapper,
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
    ++ALGEB_wrapper_count;
    return v;
}

CAMLprim value
get_ALGEB_wrapper_count (void) {
    CAMLparam0 ();
    CAMLreturn (Val_long(ALGEB_wrapper_count));
}

/* This function is intended to be called at the end of a Caml GC cycle and must
 * not create any new ALGEB_wrapper, so that ALGEB_wrapper_count can reach 0 and
 * that Maple can be restarted safely. */
CAMLprim void
run_maple_gc(void) {
    CAMLparam0 ();
    EvalMapleStatement(kv, "gc():");
    CAMLreturn0;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * 
 * Eval, Assign and friends
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */ 

#define MAPLE_EVAL_LIKE(NAME) \
    CAMLprim value \
    Maple ## NAME ## _stub(value v) { \
        CAMLparam1(v); \
        ALGEB a = ALGEB_val(v); \
        ALGEB b = Maple ## NAME(kv, a); \
        if (a == b) \
            CAMLreturn (v); \
        else \
            CAMLreturn (new_ALGEB_wrapper(b)); \
    }

MAPLE_EVAL_LIKE(Eval)
MAPLE_EVAL_LIKE(Unique)

/* Semble utile pour récupérer la valeur associée à un nom, mais ne renvoie pas
 * toujours un ALGEB valide, même quand assigned(v)==TRUE. (Je présume que c'est
 * une histoire de valeurs pas encore chargées depuis une bibliothèque, ou un
 * truc comme ça. Et je ne sais pas tester... */
/* MAPLE_EVAL_LIKE(NameValue) */

#undef MAPLE_EVAL_LIKE

CAMLprim value
EvalMapleProcedure_stub(value proc, value args) {
    CAMLparam2(proc, args);
    /* EvalMapleProcedure is documented in the VB API. */
    ALGEB res = EvalMapleProcedure(kv, ALGEB_val(proc), ALGEB_val(args));
    CAMLreturn (new_ALGEB_wrapper(res));
}

CAMLprim value
EvalMapleProc_like_stub(value proc, value arg_array) {
    CAMLparam2(proc, arg_array);
    int nargs = Wosize_val(arg_array);
    ALGEB arg_seq = NewMapleExpressionSequence(kv, nargs);
    for (int i = 0; i < nargs; i++)
        MapleExpseqAssign(kv, arg_seq, i+1, ALGEB_val(Field(arg_array, i)));
    ALGEB res = EvalMapleProcedure(kv, ALGEB_val(proc), arg_seq);
    CAMLreturn (new_ALGEB_wrapper(res));
}

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
 * Maple Errors
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */ 

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

/* bool and mbool */

CAMLprim value
ToMapleBoolean_stub(value b) {
    CAMLparam1(b);
    ALGEB a = ToMapleBoolean(kv, (M_BOOL) (Int_val(b) - 1));
    CAMLreturn (new_ALGEB_wrapper(a));
}

CAMLprim value
MapleToM_BOOL_stub(value v) {
    CAMLparam1(v);
    CAMLreturn (Val_int(MapleToM_BOOL(kv, ALGEB_val(v)) + 1));
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
 * Lists and expression sequences
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define DEF_LIST_LIKE(SHORT_NAME, FULL_NAME, NEW) \
    CAMLprim value \
    Maple ## SHORT_NAME ## Select_stub(value seq, value idx) { \
        CAMLparam2(seq, idx); \
        ALGEB aseq = ALGEB_val(seq); \
        if (!IsMaple ## FULL_NAME (kv, aseq)) \
            raise_TypeError(#SHORT_NAME); \
        ALGEB item = Maple ## SHORT_NAME ## Select(kv, aseq, Long_val(idx)); \
        CAMLreturn (new_ALGEB_wrapper(item)); \
    } \
    \
    CAMLprim value \
    array_of_ ## SHORT_NAME ## _stub(value val) { \
        CAMLparam1(val); \
        CAMLlocal1(res); \
        ALGEB a = ALGEB_val(val); \
        if (!IsMaple ## FULL_NAME (kv, a)) \
            raise_TypeError(#SHORT_NAME); \
        M_INT nops = MapleNumArgs(kv, a); \
        /* arrays are represented like tuples */ \
        res = caml_alloc_tuple(nops); \
        for (int i = 0; i < nops; i++) \
            Store_field(res, i, new_ALGEB_wrapper( \
                        Maple ## SHORT_NAME ## Select(kv, a, i+1))); \
        CAMLreturn (res); \
    } \
    \
    CAMLprim value \
    SHORT_NAME ## _of_array_stub(value array) { \
        CAMLparam1(array); \
        int n = Wosize_val(array); \
        ALGEB seq = NEW(kv, n); \
        for (int i = 0; i < n; i++) \
            Maple ## SHORT_NAME ## Assign(kv, seq, i+1, \
                    ALGEB_val(Field(array, i))); \
        CAMLreturn (new_ALGEB_wrapper(seq)); \
    }

DEF_LIST_LIKE(List, List, MapleListAlloc)
DEF_LIST_LIKE(Expseq, ExpressionSequence, NewMapleExpressionSequence)

#undef LIST_LIKE_SELECT


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Output to strings
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* Here I'm deviating from the OpenMaple API by returning OCaml strings while
 * the C functions return ALGEBs.  Is this a good thing? */

CAMLprim value
MapleALGEB_SPrintf0_stub(value fmt) {
    CAMLparam1(fmt);
    ALGEB res = MapleALGEB_SPrintf0(kv, String_val(fmt));
    CAMLreturn (caml_copy_string(MapleToString(kv, res)));
}

CAMLprim value
MapleALGEB_SPrintf1_stub(value fmt, value arg1) {
    CAMLparam2(fmt, arg1);
    ALGEB res = MapleALGEB_SPrintf1(kv, String_val(fmt), ALGEB_val(arg1));
    CAMLreturn (caml_copy_string(MapleToString(kv, res)));
}

CAMLprim value
MapleALGEB_SPrintf2_stub(value fmt, value arg1, value arg2) {
    CAMLparam3(fmt, arg1, arg2);
    ALGEB res = MapleALGEB_SPrintf2(kv, String_val(fmt), ALGEB_val(arg1),
                                                               ALGEB_val(arg2));
    CAMLreturn (caml_copy_string(MapleToString(kv, res)));
}

CAMLprim value
MapleALGEB_SPrintf3_stub(value fmt, value arg1, value arg2, value arg3) {
    CAMLparam4(fmt, arg1, arg2, arg3);
    ALGEB res = MapleALGEB_SPrintf3(kv, String_val(fmt), ALGEB_val(arg1),
                                              ALGEB_val(arg2), ALGEB_val(arg3));
    CAMLreturn (caml_copy_string(MapleToString(kv, res)));
}

CAMLprim value
MapleALGEB_SPrintf4_stub(value fmt, value arg1, value arg2, value arg3, value
        arg4) {
    CAMLparam5(fmt, arg1, arg2, arg3, arg4);
    ALGEB res = MapleALGEB_SPrintf4(kv, String_val(fmt), ALGEB_val(arg1),
                             ALGEB_val(arg2), ALGEB_val(arg3), ALGEB_val(arg4));
    CAMLreturn (caml_copy_string(MapleToString(kv, res)));
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Debugging
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static char* maple_dag_type[] = {
    "null", "INTNEG", "INTPOS", "RATIONAL", "FLOAT", "HFLOAT", "COMPLEX",
    "STRING", "NAME", "MEMBER", "TABLEREF", "DCOLON", "CATENATE", "POWER",
    "PROD", "SERIES", "SUM", "ZPPOLY", "SDPOLY", "FUNCTION", "UNEVAL",
    "EQUATION", "INEQUAT", "LESSEQ", "LESSTHAN", "AND", "NOT", "OR", "XOR",
    "IMPLIES", "EXPSEQ", "LIST", "LOCAL", "PARAM", "LEXICAL", "PROC", "RANGE",
    "SET", "TABLE", "RTABLE", "MODDEF", "MODULE", "ASSIGN", "FOR", "IF", "READ",
    "SAVE", "STATSEQ", "STOP", "ERROR", "TRY", "RETURN", "BREAK", "NEXT", "USE"
};

CAMLprim void 
dbg_print (value v) {
    CAMLparam1(v);
    ALGEB a = ALGEB_val(v);
    printf("wrapper=%lu ALGEB=%lu\n", v, a);
    printf("id=%s value=", maple_dag_type[GetMapleID(kv, a)]);
    MapleALGEB_Printf(kv, "%a\n", a);
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

#undef GET_CLOSURE
