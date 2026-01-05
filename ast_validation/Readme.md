# Ast validation: Design specification

The ast validation is divided into multiple stages. Each stage involves traversing the entire program AST.

## 1. Variable validity checks

It performs the following:

1. Checks that no type names are repeated.
2. Checks whether every variable definition is valid:
   - No variables are defined twice with the same name in the same scope  
     (function parameters are considered to be in the top-level scope).
   - Actor members are not overridden.
   - No variable is defined to be called `"this"`.
3. Alpha-renames all variables so that different variables have different names.
4. Runs the consume checker to prevent unsafe uses of consumed variables.

---

## 2. Type-Checking Stage

This stage involves moving from top to bottom, maintaining information about what has already been seen
(functions, types, actors, etc.), and then type-checking individual callables.

### Callable Type Checking Pipeline

1. **Core type checking**
   - Ensures types are used correctly.
   - Fills out the type of every expression.

2. **Initialization check (constructors only)**
   - Run only when type checking a constructor.
   - Ensures constructors correctly initialize members and does not do anything unsafe before initialization is complete.

3. **Return checking (functions only)**
   - Checks that the last statement of a function returns.

---

## 3. Atomic Section Pass

After type checking is complete, run the atomic section pass.

- Fills out the locking information of every atomic section.
- This will become non-trivial once forward declarations are added.

---
