(* Objective Caml bindings for OpenMaple
 * Marc Mezzarobba <marc@mezzarobba.net>, 2010 *)

(* À moyen terme je voudrai sans doute avoir d'une part un wrapper « trivial »
 * pour OpenMaple (~ ce module), et d'autre part des utilitaires de plus haut
 * niveau reposant dessus. *)

let package = "net.mezzarobba.openmaple-ocaml"

type algeb

(* Raised by the default error callback to indicate parsing and execution
 * errors *)
exception SyntaxError of int * string
exception MapleError of string
(* Raised when trying to use the Maple "boolean" FAIL as a Caml boolean *)
exception BooleanFail
(* Raised by some conversions from algeb to concrete Caml data types when the
 * algeb value to be converted does not have the expected type (i.e., in most
 * cases, when IsMapleFoo() returns false).  Many conversion functions still
 * raise Failure in this case. *)
exception TypeError of string

let _ = 
  Callback.register_exception
    (package ^ ".OpenMaple.MapleError")
    (MapleError "");
  Callback.register_exception
    (package ^ ".OpenMaple.SyntaxError")
    (SyntaxError (0, ""));
  Callback.register_exception
    (package ^ ".OpenMaple.BooleanFail")
    BooleanFail;
  Callback.register_exception
    (package ^ ".OpenMaple.TypeError")
    (TypeError "")

let _ =
  Callback.register
    (package ^ ".caml_gc_full_major")
    Gc.full_major

(* Debugging *)

external dbg_print : algeb -> unit = "dbg_print"
external get_algeb_wrapper_count : unit -> int = "get_ALGEB_wrapper_count"

(* Maple callbacks *)

type text_output_tag =  (* order matters! *)
  | TextDiag
  | TextMisc
  | TextOutput
  | TextQuit
  | TextWarning
  | TextError
  | TextStatus
  | TextPretty
  | TextHelp
  | TextDebug

let describe_text_output_tag = function
  | TextDiag    -> "diagnostic"
  | TextMisc    -> "miscellaneous output"
  | TextOutput  -> "text output"
  | TextQuit    -> "exit message"
  | TextWarning -> "warning"
  | TextError   -> "error"
  | TextStatus  -> "status"
  | TextPretty  -> "pretty-printed text" (* when interface(pretty) >= 1 *)
  | TextHelp    -> "help text"
  | TextDebug   -> "debug"

type text_callback      = text_output_tag -> string -> unit
type error_callback     = int -> string -> unit
type status_callback    = int -> int -> float -> unit
type read_line_callback = bool -> string
type redirect_callback  = (string -> string -> bool) * (unit -> bool)
type stream_callback    = string -> string array -> string option
type query_interrupt    = unit -> bool
type callback_callback  = string -> string option

let default_error_callback offset msg =
  if offset >= 0 then
    raise (SyntaxError (offset, msg))
  else
    raise (MapleError msg)

let default_read_line_callback dbg =
  print_string (if dbg then "\nDBG ---> " else "\n---> ");
  read_line ()

(* Evaluation *)

external eval : algeb -> algeb = "MapleEval_stub"
external unique : algeb -> algeb = "MapleUnique_stub"
(*external name_value : algeb -> algeb = "MapleNameValue_stub"*)

external eval_statement : string -> algeb = "EvalMapleStatement_stub"

external eval_procedure : algeb -> algeb -> algeb = "EvalMapleProcedure_stub"
external eval_proc : algeb -> algeb array -> algeb = "EvalMapleProc_like_stub"
let eval_proc_1 = eval_procedure
let eval_proc_2 proc a b = eval_proc proc [| a; b |]
let eval_proc_3 proc a b c = eval_proc proc [| a; b; c |]
let eval_proc_4 proc a b c d = eval_proc proc [| a; b; c; d |]

(* Start/stop/restart *)

external start_maple_doit : string array
  -> (bool * bool * bool * bool * bool * bool * bool * bool)
  -> unit = "StartMaple_stub"

external run_maple_gc : unit -> unit = "run_maple_gc"

let gc_alarm = ref None

let remove_gc_alarm () =
  match !gc_alarm with
    | Some alarm -> Gc.delete_alarm alarm; gc_alarm := None
    | None -> ()

let create_gc_alarm () =
    gc_alarm := Some (Gc.create_alarm run_maple_gc)

let start_maple
      ?(argv = [| "maple" |]) (* With argv[0] set to "maple", Maple uses $MAPLE
                               to search for libraries and stuff. *)
      ?(text_callback      : text_callback option          = None)
      ?(error_callback     : error_callback option
                               = Some default_error_callback)
      ?(status_callback    : status_callback option        = None)
      ?(read_line_callback : read_line_callback option
                               = Some default_read_line_callback)
      ?(redirect_callback  : redirect_callback option      = None)
      ?(stream_callback    : stream_callback option        = None)
      ?(query_interrupt    : query_interrupt option        = None)
      ?(callback_callback  : callback_callback option      = None)
      ?(use_gc_alarm = true)
      () =
  let register name = function
    | None -> false
    | Some closure ->
        Callback.register (package ^ name) closure;
        true
  in
  let callback_mask = 
    begin
      register ".textCallBack"     text_callback,
      register ".errorCallBack"    error_callback,
      register ".statusCallBack"   status_callback,
      register ".readLineCallBack" read_line_callback,
      register ".redirectCallBack" redirect_callback,
      register ".streamCallBack"   stream_callback,
      register ".queryInterrupt"   query_interrupt,
      register ".callBackCallBack" callback_callback 
    end
  in
    start_maple_doit argv callback_mask;
    if use_gc_alarm then create_gc_alarm ()

external stop_maple_unsafe_doit : unit -> unit = "StopMaple_unsafe_stub"
external stop_maple_safe_doit : unit -> unit = "StopMaple_safe_stub"

let stop_maple_unsafe () =
  remove_gc_alarm ();
  stop_maple_unsafe_doit ()

let stop_maple () =
  match !gc_alarm with
    | Some alarm ->
        (remove_gc_alarm ();
        try stop_maple_safe_doit ()
        with Failure msg ->
          create_gc_alarm ();
          failwith msg)
    | None -> stop_maple_safe_doit ()

external restart_maple_unsafe : unit -> unit = "RestartMaple_unsafe_stub"
external restart_maple : unit -> unit = "RestartMaple_safe_stub"

(* Raising Maple errors *)

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

(* Assignment *)

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

(* Booleans *)

type mbool = Fail | False | True  (* order matters! *)
external mbool_of_algeb : algeb -> mbool = "MapleToM_BOOL_stub"
external algeb_of_mbool : mbool -> algeb = "ToMapleBoolean_stub"
exception BooleanFail
let bool_of_mbool = function
  | True -> true
  | False -> false
  | Fail -> raise BooleanFail
let mbool_of_bool = function true -> True | false -> False
let algeb_of_bool b = algeb_of_mbool (mbool_of_bool b)
let bool_of_algeb b = bool_of_mbool (mbool_of_algeb b)

(* Integers *)

external algeb_of_int : int -> algeb = "ToMapleInteger_stub_unboxed"
external int_of_algeb : algeb -> int = "MapleToM_INT_stub_unboxed"
external algeb_of_nativeint : nativeint -> algeb = "ToMapleInteger_stub_native"
external nativeint_of_algeb : algeb -> nativeint = "MapleToM_INT_stub_native"
external algeb_of_int32 : int32 -> algeb = "ToMapleInteger32_stub"
external int32_of_algeb : algeb -> int32 = "MapleToInteger32_stub"
external algeb_of_int64 : int64 -> algeb = "ToMapleInteger64_stub"
external int64_of_algeb : algeb -> int64 = "MapleToInteger64_stub"

(* Strings *)

external string_of_algeb : algeb -> string = "MapleToString_stub"
external algeb_of_string : string -> algeb = "ToMapleString_stub"

(* Names *)

external to_maple_name : string -> bool -> algeb = "ToMapleName_stub"

let global_name name = to_maple_name name true
let fresh_local_name name = to_maple_name name false

(* Lists and expression sequences *)

external list_select : algeb -> int -> algeb = "MapleListSelect_stub"
external expseq_select : algeb -> int -> algeb = "MapleExpseqSelect_stub"

external array_of_expseq : algeb -> algeb array = "array_of_Expseq_stub"
external array_of_list : algeb -> algeb array = "array_of_List_stub"

external expseq_of_array : algeb array -> algeb = "Expseq_of_array_stub"
external list_of_array : algeb array -> algeb = "List_of_array_stub"

(* SPrintf *)

external maple_algeb_sprintf_0 : string -> string = "MapleALGEB_SPrintf0_stub"
external maple_algeb_sprintf_1 : string -> algeb -> string
                                                    = "MapleALGEB_SPrintf1_stub"
external maple_algeb_sprintf_2 : string -> algeb -> algeb -> string
                                                    = "MapleALGEB_SPrintf2_stub"
external maple_algeb_sprintf_3 : string -> algeb -> algeb -> algeb -> string
                                                    = "MapleALGEB_SPrintf3_stub"
external maple_algeb_sprintf_4 : string -> algeb -> algeb -> algeb -> algeb
                                          -> string = "MapleALGEB_SPrintf3_stub"

let algeb_sprintf fmt = function
  (* I see little point in forcing nargs <= 4 through typing since we have no
   * way to check that the args match the references in the format string. *)
  | []                       -> maple_algeb_sprintf_0 fmt
  | [arg1]                   -> maple_algeb_sprintf_1 fmt arg1
  | [arg1; arg2]             -> maple_algeb_sprintf_2 fmt arg1 arg2
  | [arg1; arg2; arg3]       -> maple_algeb_sprintf_3 fmt arg1 arg2 arg3
  | [arg1; arg2; arg3; arg4] -> maple_algeb_sprintf_4 fmt arg1 arg2 arg3 arg4
  | _ -> raise (Invalid_argument "algeb_sprintf_list")

let sprint a = maple_algeb_sprintf_1 "%a" a
