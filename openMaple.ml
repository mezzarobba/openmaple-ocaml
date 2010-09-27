(* Wrapper for the OpenMaple API *)

(* À moyen terme je voudrai sans doute avoir d'une part un wrapper « trivial »
 * pour OpenMaple (~ ce module), et d'autre part des utilitaires de plus haut
 * niveau reposant dessus. *)

type algeb

external dbg_print : algeb -> unit = "dbg_print"

external start_maple : unit -> unit = "StartMaple_stub"
external stop_maple : unit -> unit = "StopMaple_stub"
external restart_maple : unit -> unit = "RestartMaple_stub"

external eval : string -> algeb = "EvalMapleStatement_stub"

external maple_raise_error : string -> unit = "MapleRaiseError_stub"
external maple_raise_error_1 : string -> algeb -> unit = "MapleRaiseError1_stub"
external maple_raise_error_2 : string -> algeb -> algeb -> unit 
                                                       = "MapleRaiseError2_stub"

let raise_error ?arg1 ?arg2 msg =
  match (arg1, arg2) with
    | None, None     -> maple_raise_error   msg
    | Some a, None   -> maple_raise_error_1 msg a
    | Some a, Some b -> maple_raise_error_2 msg a b
    | None, Some _   -> raise (Invalid_argument "OpenMaple.raise_error")

external maple_assign : algeb -> algeb -> unit = "MapleAssign_stub"
external maple_assign_indexed : algeb -> int array -> algeb -> unit 
                                                     = "MapleAssignIndexed_stub"

let assign ?indices lhs rhs =
  match indices with
    | None -> maple_assign lhs rhs
    | Some l ->
        let a = Array.of_list l in
          maple_assign_indexed lhs a rhs

(* créer plutôt un sous-module ALGEB avec of_int, to_int, etc. ? *)

external algeb_of_int : int -> algeb = "ToMapleInteger_stub_unboxed"
external int_of_algeb : algeb -> int = "MapleToM_INT_stub_unboxed"
external algeb_of_nativeint : nativeint -> algeb = "ToMapleInteger_stub_native"
external nativeint_of_algeb : algeb -> nativeint = "MapleToM_INT_stub_native"
external algeb_of_int32 : int32 -> algeb = "ToMapleInteger32_stub"
external int32_of_algeb : algeb -> int32 = "MapleToInteger32_stub"
external algeb_of_int64 : int64 -> algeb = "ToMapleInteger64_stub"
external int64_of_algeb : algeb -> int64 = "MapleToInteger64_stub"

external string_of_algeb : algeb -> string = "MapleToString_stub"
external algeb_of_string : string -> algeb = "ToMapleString_stub"

external string_to_name : string -> bool -> algeb = "ToMapleName_stub"

let global_name name = string_to_name name true
let new_local_name name = string_to_name name false
