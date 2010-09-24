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

#define ALGEB_val(v) (*((ALGEB *) Data_custom_val(v)))

/* Custom operations.  Quoting from the OCaml manual: "Do not use
 * CAMLparam to register the parameters to these functions, and do not
 * use CAMLreturn to return the result." */

static void
finalize_ALGEB(value v) {
    MapleGcAllow(kv, ALGEB_val(v));
}

static struct custom_operations ALGEB_ops = {
    "net.mezzarobba.openmaple-ocaml.ALGEB-v0.1",
    &finalize_ALGEB,
    custom_compare_default,
    custom_hash_default,
    custom_serialize_default,
    custom_deserialize_default
};

/* I see no simple estimate for the amount of resources on the Maple
 * size corresponding to an ALGEB value.  For now, let's finalize the
 * Caml values pointing to Maple objects quite aggressively.  Should we
 * also have OCaml's GC call Maple's?  (Damien?) */
static const unsigned int MAX_DANGLING_ALGEB = 100;

static value
ALGEB_to_value(ALGEB a) {
    /* Le manuel de Maple suggère d'utiliser MaplePointerSetMarkFunction
     * plutôt que MapleGcProtect, mais je ne comprends pas comment on
     * est censé faire pour les libérer un MaplePointer. J'ai
     * l'impression que ceux-ci ne conviennent en fait que pour les
     * objets externes (contenant eux-mêmes des pointeurs vers des
     * objets Maple) qu'on veut mettre sous le contrôle du GC de Maple.
     */
    if (MapleGcIsProtected(kv, a) == TRUE)
        caml_failwith("Unable to protect the value from Maple's GC");
    MapleGcProtect(kv, a);
    value v = caml_alloc_custom(&ALGEB_ops, sizeof(ALGEB), 1,
                                                    MAX_DANGLING_ALGEB);
    ALGEB_val(v) = a;
    return v;
}

CAMLprim value
EvalMapleStatement_stub(value statement) {
    CAMLparam1(statement);
    ALGEB a = EvalMapleStatement(kv, String_val(statement));
    CAMLreturn (ALGEB_to_value(a));
}


