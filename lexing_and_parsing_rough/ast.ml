(* ---------- Types ---------- *)

type cap =
  | Ref
  | Val
  | Iso
  | Locked of string

(* Fundamental types (built-in and composites) *)
type basic_type =
  | TInt
  | TFloat
  | TBool
  | TNamed of string

(* A “complete” type that can include pointer or capability qualifiers *)
type full_type =
  | Raw of basic_type
  | Pointer of basic_type * cap

(* User-defined type declarations *)
type nameable_type = 
  | Basic of basic_type
  | Struct of (string * basic_type) list

type type_def = string * nameable_type
(* ---------- Values ---------- *)

type basic_value =
  | VInt of int
  | VFloat of float
  | VBool of bool
  | VVar of string

type new_instance = {
  type_ : basic_type;
  cap : cap;
  default_val : basic_value option;
}

and value =
  | Basic of basic_value
  | New of new_instance
  | Array of int * new_instance
  | Struct of (string * value) list

(* ---------- Expressions ---------- *)

type bin_op =
  | Add | Sub | Mul | Div
  | Geq | Leq | Eq | Neq | Gt | Lt

type assignable_expr =
  | Var of string
  | Deref of assignable_expr
  | Field of assignable_expr * (string list)

type value_expr =
  | Value of value
  | BinOp of value_expr * bin_op * value_expr
  | Not of value_expr
  | Consume of string
  | Call of string * value_expr list
  | Assignable of assignable_expr

(* ---------- Statements ---------- *)

type assignment = {
  assignable_expr : assignable_expr;
  rhs : value_expr;
}

type var_decl_with_init = {
  var_name : string;
  type_ : full_type;
  init : value;
}

type if_stmt = {
  cond : value_expr;
  then_body : stmt list;
  else_body : stmt list option;
}

and while_stmt = {
  cond : value_expr;
  body : stmt list;
}

and atomic_stmt = {
  locks: string list;
  body: stmt list
}

and stmt =
  | Decl_init of var_decl_with_init
  | Assign of assignment
  | Expr of value_expr
  | If of if_stmt
  | While of while_stmt
  | Return of value_expr option
  | Atomic of atomic_stmt

  (* ---------- Functions ---------- *)

type var_decl = {
  var_name : string;
  type_ : full_type;
}

type func = {
  name : string;
  return_type : full_type;
  params : var_decl list;
  body : stmt list;
}

  (* ---------- Actor ---------- *)

type behaviour = {
  name: string;
  params: var_decl list;
  body: stmt list
}

type actor = {
  name: string;
  member_vars: var_decl list;
  member_funcs: func list;
  member_behaviours: behaviour
}

(* Expression for entire program *)
type top_level_items =
  | TypeDef of type_def 
  | FuncDef of func
  | ActorDef of actor


type program = (string list) * (top_level_items list)