#set text(size: 12pt, font: "Linux Libertine")
#set heading(numbering: "1.1")
#set page(paper: "a4", margin: 2cm)
#set par(justify: true)
#show "—": it => sym.wj + it

Things that I want to talk about:
Main part:
- Formal theory:
  - Syntax
  - Type system
- Implementation:
  - Lexer, parser and AST structure: Talk about the tools that I used
  - Talk about AST validation
    - Include a flow chart of all the different stages of validation
    - Then dive deeper into the different stages
      - The different patterns I used with a time complexity analysis
      - Talk about kosaraju's algorithm
    - Talk about the runtime
      - Initial attempts of using lock-free style. Then just giving up and using a lock-based approach.
    - Talk about code generation
      - This will be quite short. Simply talk about how I generate code for the different constructs.

Extension:
- Adding pointers to pointers
  - Motivate this extension.
  - Modifications to the type system (with a reference to the evaluation sections)
  - How to verify that the type system is correct.
  - Briefly talk about the changes to the implementation




