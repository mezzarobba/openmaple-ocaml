(* Wrapper for the OpenMaple API *)

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


