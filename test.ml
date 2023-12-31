let _ =
  OpenMaple.start_maple ();
  let series = OpenMaple.eval_proc_3 (OpenMaple.global_name "series")
  and sin = OpenMaple.eval_proc_1 (OpenMaple.global_name "sin")
  and x = OpenMaple.global_name "x" in
    print_string (OpenMaple.sprint 
                    (series (sin x) x (OpenMaple.algeb_of_int 3)));
    print_newline ()

let test_lists_expseqs () =
  OpenMaple.start_maple ();
  print_string
    (OpenMaple.sprint
       (OpenMaple.list_of_array
          (Array.map OpenMaple.global_name [| "x"; "y"; "z"; "t" |])));
  print_newline ();
  print_string
    (OpenMaple.sprint
       (OpenMaple.eval_procedure
          (OpenMaple.global_name "series")
          (OpenMaple.expseq_of_array
             [| OpenMaple.eval_statement "1/(1-x):"; 
                OpenMaple.global_name "x";
                OpenMaple.algeb_of_int 6 |])));
  print_newline ();
  let mylist = OpenMaple.eval_statement ("[1, 2, 3]:")
  and myexpseq = OpenMaple.eval_statement ("a, b, c:")
  in Printf.printf "%s %s\n"
       (OpenMaple.sprint (OpenMaple.list_select mylist 1))
       (OpenMaple.sprint (OpenMaple.expseq_select myexpseq 2));
     try
       ignore (OpenMaple.sprint (OpenMaple.expseq_select myexpseq 4))
     with OpenMaple.MapleError _ -> ()

let test_sprint () =
  OpenMaple.start_maple ();
  let a = OpenMaple.eval_statement "x^2+1:"
  and b = OpenMaple.algeb_of_int 42
  in
    List.iter print_string
      [ OpenMaple.algeb_sprintf "coucou\n" [];
        OpenMaple.algeb_sprintf "coucou %a\n" [a];
        OpenMaple.algeb_sprintf "coucou %a %a\n" [a;b];
        OpenMaple.algeb_sprintf "coucou %a %a %a\n" [a;a;b];
        OpenMaple.algeb_sprintf "coucou %a %a %a %a\n" [a;b;b;b];
        OpenMaple.sprint a
      ];
    ignore (OpenMaple.algeb_sprintf "erreur" [a;b;b;b;b])

let test_compare_hash () =
  OpenMaple.start_maple ();
  let mylist = List.map OpenMaple.algeb_of_int [5; 4; 3; -1; 0; 10; 2] in
  let h = Hashtbl.create 5 in
    List.iter (fun x -> Hashtbl.add h x (OpenMaple.int_of_algeb x)) mylist;
    List.iter (fun a -> print_int (Hashtbl.find h a); print_string " ")
      (List.sort compare mylist);
    print_newline ()


let test_eval_proc () =
  OpenMaple.start_maple ();
  let ifactor = 
    let f = OpenMaple.eval_procedure (OpenMaple.global_name "ifactor") in
      function n -> f (OpenMaple.algeb_of_int n) in
    OpenMaple.dbg_print (ifactor 42);
    OpenMaple.dbg_print (ifactor 17)

let test_boolean () =
  OpenMaple.start_maple ();
  List.iter (fun b -> OpenMaple.dbg_print (OpenMaple.algeb_of_mbool b))
    [ OpenMaple.True; OpenMaple.False; OpenMaple.Fail ];
  List.iter (fun b -> OpenMaple.dbg_print (OpenMaple.algeb_of_bool b))
    [ true; false ];
  List.iter (fun s -> if OpenMaple.bool_of_algeb (OpenMaple.eval_statement s)
             then print_string "TRUE\n" else print_string "FALSE\n")
    [ "true:"; "false:"; "FAIL:" ]

let simple_frontend () =
  let my_text_callback tag msg =
    Printf.printf "Maple %s: %s\n" (OpenMaple.describe_text_output_tag tag) msg
  and stop_long_computations () =
    print_string "CHECK INTERRUPT\n";
    true 
  in
  OpenMaple.start_maple
    ~argv: [| "maple"; "-s" |]
    ~query_interrupt: (Some stop_long_computations)
    ~status_callback: (Some (Printf.printf "STATUS: %d %d %f\n"))
    ~text_callback: (Some my_text_callback)
    ~error_callback: None
    ();
  while true do
    ignore (OpenMaple.eval_statement (read_line()));
  done 

let test_names () =
  OpenMaple.start_maple ();
  let g = OpenMaple.global_name "a" in
  let l1 = OpenMaple.fresh_local_name "a" in
  let l2 = OpenMaple.fresh_local_name "a" in
    OpenMaple.assign g (OpenMaple.algeb_of_int 0);
    OpenMaple.assign l1 (OpenMaple.algeb_of_int 1);
    ignore (OpenMaple.eval_statement "a;");
    List.iter OpenMaple.dbg_print [g; l1; l2]


let test_string () =
  OpenMaple.start_maple ();
  let s = "coucou à tous, avec l'accent\n" in
  let a = OpenMaple.algeb_of_string s in
    s.[1] <- 'x';
    print_string s;
    print_string (OpenMaple.string_of_algeb a);
    try ignore (OpenMaple.algeb_of_string "abc\000def")
    with _ -> print_string "ok\n"

let test_native_int () =
  OpenMaple.start_maple ();
  List.iter
    (fun i ->
       if (OpenMaple.nativeint_of_algeb (OpenMaple.algeb_of_nativeint i)) = i then
         print_string "ok\n"
       else
         print_string "FAIL\n")
      [ Nativeint.of_int 0; Nativeint.min_int; Nativeint.max_int ];
  List.iter
    (fun s ->
       try 
         ignore (OpenMaple.nativeint_of_algeb (OpenMaple.eval_statement s));
         print_string "ok\n"
       with _ -> print_string "error\n")
    [ "2^62:"; "-2^62-1:"; "2^65:"; "-2^65:" ]

let test_int () = 
  OpenMaple.start_maple ();
  List.iter
    (fun i ->
       let a = OpenMaple.algeb_of_int i in
         print_int i; print_newline ();
         OpenMaple.dbg_print a;
         print_int (OpenMaple.int_of_algeb a); print_newline())
      [ 0; 1; -1; max_int; min_int ];
  List.iter
    (fun s ->
       try 
         print_int (OpenMaple.int_of_algeb (OpenMaple.eval_statement s))
       with _ -> print_string "error\n")
    [ "2^62:"; "-2^62-1:"; "2^65:"; "-2^65:" ]



let test_assign () =
  OpenMaple.start_maple ();
  let x = OpenMaple.eval_statement "x:" in
    OpenMaple.assign ~indices:[] x (OpenMaple.eval_statement "1:");
    OpenMaple.assign ~indices:[1;2;3] x (OpenMaple.eval_statement "42:");
    OpenMaple.raise_error "une erreur";
    OpenMaple.raise_error 
      ~arg1:(OpenMaple.eval_statement "x[]:")
      ~arg2:(OpenMaple.eval_statement "x[1,2,3]:")
               "une erreur a parametres : %1, %2"

      (*
let test_gc () = 
  OpenMaple.start_maple ();
  OpenMaple.eval_statement "gc();";
  let x = OpenMaple.eval_statement "diff(x^(x^(x^(x^(x^x)))),x,x,x,x, x, x,x,x,x);" in
    (OpenMaple.eval_statement "gc();";
     OpenMaple.eval_statement "kernelopts(gcbytesreturned);";
     Gc.full_major ();
     OpenMaple.eval_statement "gc();";
     OpenMaple.eval_statement "kernelopts(gcbytesreturned);";
     OpenMaple.eval_statement "gc();";
     OpenMaple.eval_statement "kernelopts(gcbytesreturned);";
     OpenMaple.print x);
    Gc.full_major ();
    OpenMaple.eval_statement "gc();";
    OpenMaple.eval_statement "kernelopts(gcbytesreturned);"
       *)
