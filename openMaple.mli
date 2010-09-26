(* Low-level interface to Maple (wrapper for the OpenMaple API) *)

external ploum : unit -> unit = "ploum"

external start_maple : unit -> unit = "StartMaple_stub"
external stop_maple : unit -> unit = "StopMaple_stub"
external restart_maple : unit -> unit = "RestartMaple_stub"

type algeb

external eval : string -> algeb = "EvalMapleStatement_stub"

external print : algeb -> unit = "my_print"
