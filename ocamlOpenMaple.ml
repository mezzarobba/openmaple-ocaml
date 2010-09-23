type kernel_vector

external plus : int -> int -> int = "my_add_stub"
external start_maple : unit -> kernel_vector = "start_maple_stub"
external exec : kernel_vector -> string -> unit = "exec_maple_statement";;

print_int (plus 1 2);
print_newline();
let kv1 = start_maple () in
  (let kv2 = start_maple() in
     exec kv1 "x := 42;";
     exec kv2 "x := 18;";
     exec kv2 "x;");
  exec kv1 "x;"
