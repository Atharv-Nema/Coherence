#import "@preview/simplebnf:0.1.2": *
#import "@preview/curryst:0.6.0": rule, prooftree
#set text(size: 12pt, font: "Linux Libertine")
#set heading(numbering: "1.1")
#set page(paper: "a4", margin: 2cm)
#set par(justify: true)
#show "—": it => sym.wj + it

= Coherence Semantics 
This document describes the semantics on which the Coherence language is based on.  

= Basic cut-down version of the language
This cutdown version does not have the full features of the language. It is described as below


== Formal Syntax

*Types*

#bnf(
  Prod("FullType", {
    Or[BasicType][]
    Or[BasicType Cap][]
  }),
  Prod("BasicType", {
    Or[Unit][] 
    Or[Int][] 
    Or[Bool][] 
    Or[Named(N)][] 
    Or[Actor(A)][]
  }),
  Prod("Cap", {
    Or[ref][] 
    Or[val][] 
    Or[iso][] 
    Or[$"locked<L>"$][]
  }),
   Prod("NameableType", {
    Or[BasicType][]
    Or[$ "struct {" overline("field" : "BasicType" ";") "}" $][]
  }), 
)

*Top-level items*
These are items that occur in the top-level of a program. They include type definitions, top-level functions, and actor definitions.

#bnf(
  Prod("Program", {
    Or[$overline("TopLevel")$][]
  }),
  Prod("TopLevel", {
    Or[TypeDef][]
    Or[Func][]
    Or[Actor][]
  }),
  Prod("TypeDef", {
    Or[$ "type " N = "NameableType" $][]
  }),
 
  Prod("Actor", {
    Or[$ "actor "A"{" overline("Field;") " "overline("Member") "}" $][]
  }),
  Prod("Field", { Or[$"field : FullType"$][] }),
  Prod("Member", {
    Or[Func][]
    Or[Ctor][]
    Or[Behv][]
  }),
  Prod("Ctor",  { Or[$ "new " k "(" overline("FullType arg") ")" space "{" overline(s) "}" $][] }),
  Prod("Func",  { Or[$ "func " m "(" overline("FullType arg") ")" " => " "FullType" space "{" overline(s) "}" $][] }),
  Prod("Behv",  { Or[$ "be " b "(" overline("FullType arg") ")" space "{" overline(s) "}" $][] }),
)

*Statements*
Statements are parts of the body that are terminated with a semi-colon in the body of a callable (function, behaviour, or constructor). They can have side effects and do not return a value.
#bnf(
  Prod($s$, {
    Or[$ "var " x : "FullType" = e ";" $][Local declaration]
    Or[$ f ":=" e ";" $][Member initialize]
    Or[$ e "->"b"("overline(e)");" $][Behaviour call]
    Or[$ "OUT " e ";" $][Print statement]
    Or[$ "if ("e") {" overline(s) "} else {" overline(s) "}" $][]
    Or[$ "while ("e") {" overline(s) "}" $][]
    Or[$ "atomic {" overline(s) "}" $][]
    Or[$ "return " e ";" $][]
    Or[$ e ";" $][]
  }),
)

#v(1em)
*Expressions*
These are parts of the program that evaluate to a value.
#bnf(
  Prod($e$, {
    Or[()][Unit] 
    Or[$i$][Integers] 
    Or[true][] 
    Or[false][] 
    Or[$x$][Variables]
    Or[$ "{" overline(f = e ";") "}:" N $][Struct literal]
    Or[$ "new " "Cap""[" e "]" "BasicType" "("e")" $][Allocation]
    Or[$ "new " A "." k "(" overline(e) ")" $][Actor construction]
    Or[$ e"["e"]" $][Pointer access]
    Or[$ e "." f $][Field access]
    Or[$ e = e $][Assignment]
    Or[$ m"("overline(e)")" $][Function call]
    Or[$ e " op " e $][Binary operation]
  }),
)

== Type checking
This part of the document describes the rudimentary type-checking carried out initially. In the formal rules below, assume that renaming has been carried out and so all names are distinct and conflict-free.


=== Typing Environment
The environment $Gamma$ consists of the following:
- $Gamma_V$: a mapping from variable names to their types
- $Gamma_L$: a boolean about whether the type-checker is in an atomic section
- $Gamma_A$: a mapping from actor names to an object containing their information (constructors, functions and behaviours).
- $Gamma_C$: mapping to named types.
- $Gamma_F$: mapping from function names to their information.
- $Gamma_"this"$: the type of the current actor.


=== Expression typing
I will not be focusing on binary operation typing as it is kind of simple and not worth mentioning.

#prooftree(
  rule(
    name: [Int],
    $Gamma tack i : "int"$,
  )
)
#prooftree(
  rule(
    name: [Bool],
    $Gamma tack "true" : "bool"$,
  )
)
#prooftree(
  rule(
    name: [Bool],
    $Gamma tack "false" : "bool"$,
  )
)


#prooftree(
  rule(
    name: [Var],
    $Gamma_V (x) = T$,
    $Gamma tack x : T$
  )
)

#v(1em)

// Pointer Access (The Locked Cap Check)
#prooftree(
  rule(
    name: [Ptr-Access],
    $Gamma tack e_p : T space "locked<L>"$,
    $Gamma tack e_i : "Int"$,
    // The condition for Locked capabilities
    $Gamma_A = 1$,
    $Gamma tack e_p [e_i] : T$,
  )
)

// Field Access
#prooftree(
  rule(
    name: [Field],
    $Gamma tack e : N$,
    $S(N) = "struct" { overline(f : B) }$,
    $f_i in overline(f)$,
    $Gamma tack e.f_i : B_i$,
  )
)

#v(1em)

// Assignment (LHS check and Capability Assignability)
#prooftree(
  rule(
    name: [Assign],
    $Gamma tack e_1 : T_1$,
    $Gamma tack e_2 : T_2$,
    $Gamma tack "type_assignable"(T_1, T_2)$,
    $Gamma tack "appear_lhs"(e_1)$,
    $Gamma tack e_1 = e_2 : "unalias"(T_1)$,
  )
)

#v(1em)

// Actor Construction
#prooftree(
  rule(
    name: [Actor-New],
    $A in Gamma_S$,
    $k in A_"ctor"$,
    $Gamma tack overline(e) : overline(T_"arg")$,
    $"type_assignable"(k_"params", overline(T_"arg"))$,
    $Gamma tack "new "A "." k (overline(e)) : "Actor"(A)$,
  )
)

=== Statement typing
$Gamma tack s tack.l Gamma'$ says that s is valid and after that, the environment becomes $Gamma'$.

#block[
#v(1em)

// Rule: Local Variable Declaration
#prooftree(
  rule(
    name: [Var-Decl],
    $Gamma tack e : T_e$,
    $"assignable"(T, T_e)$,
    $Gamma tack "var " x : T = e ";" tack.l Gamma[V |-> V[x |-> T]]$
  )
)

#v(2em)

// Rule: Member Initialization (f := e)
#prooftree(
  rule(
    name: [Member-Init],
    $Gamma_"this" = A$,
    $A_"fields" (f) = T_f$,
    $Gamma tack e : T_e$,
    $"assignable"(T_f, T_e)$,
    $Gamma tack f ":=" e tack.l Gamma$
  )
)

#v(2em)

#prooftree(
  rule(
    name: [Out],
    $Gamma tack e : "Int"$,
    $Gamma tack "OUT " e ";" tack.l Gamma$
  )
)

#v(2em)

#prooftree(
  rule(
    name: [Raw expression],
    $Gamma tack e : T$,
    $Gamma tack "e;" tack.l Gamma$
  )
)


#v(2em)

#prooftree(
  rule(
    name: [Atomic],
    $Gamma[L |-> "true"] tack overline(s) tack.l Gamma'$,
    $Gamma tack "atomic {" overline(s) "}" tack.l Gamma$
  )
)

#v(2em)

// Rule: While Loop
#prooftree(
  rule(
    name: [While],
    $Gamma tack e : "Bool"$,
    $Gamma tack overline(s) tack.l Gamma'$,
    $Gamma tack "while (" e ") {" overline(s) "}" tack.l Gamma$
  )
)

#v(2em)

// Rule: Behavior Call (Arrow notation)
#prooftree(
  rule(
    name: [Behv-Call],
    $Gamma tack e_a : "Actor"(A_"name")$,
    $b in Gamma_A (A_"name")$,
    $Gamma tack overline(e) : overline(T_e)$,
    $"type_assignable"(b_"params", overline(T_e))$,
    $Gamma tack e_a"->"b "(" overline(e) ");" tack.l Gamma$
  )
)

#prooftree(
  rule(
    name: [Return],
    $Gamma_"curr_func" = F$,
    $Gamma tack "type_assignable("F_"ret", T")"$,
    $Gamma tack e : T$,
    $Gamma tack "return "e";" tack.l Gamma$
  )
)
]

=== Top-level typing

// Rule: Type Definition
#prooftree(
  rule(
    name: [TypeDef],
    $Gamma tack "NameableType" : "valid"$,
    $Gamma tack "type " N = "NameableType" tack Gamma[C |-> C[N |-> "NameableType"]]$
  )
)

#v(2em)

// Rule: Global Function Definition
#prooftree(
  rule(
    name: [T-Func],
    $m not in "dom"(Gamma_F)$,
    $Gamma' = Gamma[F |-> F[m |-> (overline(phi_p) => phi_r)]][V |-> {overline(x : phi_p)}][m_"curr" |-> m]$,
    $Gamma' tack overline(s) "returns" phi_r$,
    $Gamma tack "func " m(overline(x : phi_p)) " => " phi_r { overline(s) } tack Gamma[F |-> F[m |-> (overline(phi_p) => phi_r)]]$
  )
)

#v(2em)

// Rule: Actor Definition
#prooftree(
  rule(
    name: [T-Actor],
    $A not in "dom"(Gamma_A)$,
    $Gamma_"base" = Gamma[A |-> A[A |-> "Actor"(A)]]$,
    $forall M in "Members": Gamma_"base"[Gamma_"this" |-> A] tack M : "valid"$,
    $Gamma tack "actor " A { overline(F) overline(M) } tack Gamma_"base"$
  )
)