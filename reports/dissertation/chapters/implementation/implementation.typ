#set text(size: 12pt, font: "Linux Libertine")
#set heading(numbering: "1.1")
#set page(paper: "a4", margin: 2cm)
#set par(justify: true)
#show "—": it => sym.wj + it

Things that I want to talk about:

- Runthrough of initial core stage (this is the main part of the chapter)
  - Formal type system
  - Structure of the AST
  - Mention of lexing and parsing
  - Type-checker structure
  - Code generation
  - Runtime implementation
- Including mutual recursion
  - The basic modifications required
  - The main modification to the lock-inference algorithm
- Adding consume 
  - Mention of basic modification to the type system
  - Consume checker algorithm
- Adding pointers to pointers
  - Modifications to the type system (with a reference to the evaluation sections)
  - Other modifications required


