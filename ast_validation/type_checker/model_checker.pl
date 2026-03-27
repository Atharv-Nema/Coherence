% lock names
lock(a).
lock(b).

% All valid capabilities
capability(iso).
capability(iso_cap).
capability(val).
capability(ref).
capability(tag).
capability(locked(X)) :- lock(X).

% view(Origin, Field, Result)
view(iso, ref, iso).
view(iso, val, val).
view(iso, tag, tag).
view(iso, iso, iso).
view(iso_cap, ref, iso_cap).    
view(iso_cap, val, val).    
view(iso_cap, tag, tag).
view(iso_cap, iso, iso_cap).
view(val, ref, val).
view(val, val, val).
view(val, tag, tag).
view(val, iso, val).
view(ref, ref, ref).
view(ref, val, val).
view(ref, tag, tag).
view(ref, iso, iso).
view(tag, ref, tag).
view(tag, val, tag).
view(tag, iso, tag).
view(tag, tag, tag).
view(locked(X), tag, tag) :- lock(X).
view(locked(X), ref, locked(X)) :- lock(X).
view(locked(X), val, val) :- lock(X).
view(locked(X), iso, iso) :- lock(X).
view(locked(X), locked(Y), locked(Y)) :- lock(X), lock(Y).

% find_nonassociative(X, Y, Z) suceeds if (X |> Y) |> Z is not X |> (Y |> Z)
find_nonassociative(X, Y, Z) :-
    view(X, Y, T1), view(T1, Z, L), view(Y, Z, T2), \+ view(X, T2, L).

% assign(Target, Source): Source can be assigned to Target
assign(iso, iso_cap).
assign(val, iso_cap).
assign(val, val).
assign(ref, iso_cap).
assign(ref, ref).
assign(tag, iso).
assign(tag, iso_cap).
assign(tag, val).
assign(tag, ref).
assign(tag, tag).
assign(locked(X), locked(X)) :- lock(X).

% subtype(Target, Source): Source capability can be treated as Target capability
subtype(iso, iso).
subtype(X, Y) :- assign(X, Y).


% check_nested_type_inconsistencies(C1, C2, I) succeeds if it can find 
% capabilities C1, C2 and I such that assign(C1, C2) and not subtype(C1 |> I, C2 |> I)

check_nested_type_inconsistencies(C1, C2, I) :- 
    assign(C1, C2), view(C1, I, I1), view(C2, I, I2), \+ subtype(I1, I2).

% alias(C1, C2): succeeds if C2 is the alias of C1 with maximal permissions (in the 
% preorder defined by assign).
% alias(iso, tag).
% alias(val, val).
% alias(ref, ref).
% alias(tag, tag).
% alias(locked(X), locked(X)) :- lock(X).

% find_invalid_aliasing(C1, C2) 


% Things to add:
% Raw reference capability assignment does not violate the invariants

% Do for each of the capabilities
% 1. Ref cannot have a mutable reference outside
% Things that can be done:
% a) Put it in a "wrapped" structure
% b) Perform following operations
% - Consume it: (Eg iso)
% - Alias the outer guy with a local reference
% - Send it to another actor, and in the other actor do stuff on it 
% c) Access the field and get a violation
% Notice that if each stage is safe for the start and the end reference capabilities, 
% the entire thing is safe

% The concept of safety needs to be specified properly. I think I just need to make sure that the 
% invariance/covariance stuff is satisfied. (This should be done instead of the simple compatibility check being done currently).
% I think that this is great news!
% 2. Val cannot have a mutable reference
% 3. Locked cannot be unprotected
% 4. Iso has to have a unique alias (hardest one to model)