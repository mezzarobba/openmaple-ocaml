(* Low-level interface to Maple (wrapper for the OpenMaple API) *)

external plus : int -> int -> int = "my_add_stub"

external start_maple : unit -> unit = "start_maple_stub"
external stop_maple : unit -> unit = "stop_maple_stub"
external restart_maple : unit -> unit = "restart_maple_stub"
external exec : string -> unit = "exec_maple_statement"

