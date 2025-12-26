# Codegen Naming and Calling Conventions

This document describes how the compiler maps ast-level entities (functions, behaviours, constructors, structs, types) into LLVM-level entities, and which implicit registers/arguments are passed through generated code.


## LLVM Symbol Naming

### Functions

**Function within an actor**
```
<func_name>.<actor_name>.func
```

**Top-level / outside an actor**
```
<func_name>.func
```

### Behaviours
```
<be_name>.<actor_name>.be
```

### Constructors
```
<constructor_name>.<actor_name>.constr
```

## Generated Struct Names

### Actor struct
```
<actor_name>.struct
```

### Behaviour message struct
```
<be_name>.<actor_name>.be.struct
```

## Generated Type Names

### Struct types
A source struct `T` becomes:
```
T.struct
```

## Implicit/Additional Registers Passed In

For **functions** and **constructors**, the generated functions have the following implicit parameters:

- `%this.id`  
  The ID of the current actor instance.

- `%sync_actor.id`  
  The ID of the actor instance on which lock operations must be performed.


## Calling Rules

### 1) Calling Functions

When generating a call to a function, pass:

- `%this.id`  = the `%this.id` from the **current scope**
- `%sync_actor.id` = the `%sync_actor.id` from the **current scope**

### 2) Calling Constructors

When generating a call to a constructor, pass:

- `%this.id`  = the instance ID for the **newly created object**
- `%sync_actor.id` = the `%sync_actor.id` from the **current scope**

---

## Behaviour Message Layout

Behaviour “message structs” contain, at the end, the following fields:

- `%this.id`
