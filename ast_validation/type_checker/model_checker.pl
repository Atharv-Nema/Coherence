% The maximum occurences of reference capabilities in the 
% predicates below is 3, and hence only 3 different locks are 
% needed.
lock(a).
lock(b).
lock(c).

% All valid capabilities
capability(iso).
capability(iso_cap).
capability(val).
capability(ref).
capability(tag).
capability(locked(X)) :- lock(X).

% view(K1, K2, K3) succeeds if K1 > K2 = K3, 
% where > is the viewpoint 
adaptation operator
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

% check_nonassociativity(K1, K2, K3): succeeds if (K1 > K2) > K3 is
% not equal to (K1 > (K2 > K3)).
check_nonassociativity(K1, K2, K3) :- 
    view(K1, K2, K1_2), view(K1_2, K3, K1_2_3), 
    view(K2, K3, K2_3), \+ view(K1, K2_3, K1_2_3).

% covariant(K1, K2): succeeds if K1 is covariant to K2
covariant(tag, X) :- capability(X).
covariant(X, X) :- capability(X), \+ (X = iso_cap).
covariant(X, iso_cap) :- capability(X), \+ (X = iso_cap).

% invariant(K1, K2): succeeds if K1 is invariant to K2
invariant(X, X) :- capability(X), \+ (X = iso_cap).
invariant(X, iso_cap) :- capability(X), \+ (X = iso_cap).

% assignable(K1, K2): succeeds if K2 is assignable to K1
assignable(tag, X) :- capability(X).
assignable(X, X) :- capability(X), \+ (member(X, [iso, iso_cap])).
assignable(X, iso_cap) :- capability(X), \+ (X = iso_cap).

% mutable(K): succeeds if K is a mutable capability
mutable(K) :- 
    capability(K), member(K, [ref, iso, iso_cap, locked(L)]).

% readable(K): succeeds if K can be read but not written
readable(val).

% opaque(K): succeeds if K can neither be written to nor read 
% from
opaque(tag).

% ptr_assignable(K, K1, K2): succeeds if the type (T K) K2 is 
% assignable to (T K) K1
ptr_assignable(K, K1, K2) :- assignable(K1, K2),
    view(K1, K, ViewK1), view(K2, K, ViewK2),
    mutable(K1), invariant(ViewK1, ViewK2).
ptr_assignable(K, K1, K2) :- 
    assignable(K1, K2), view(K1, K, ViewK1), view(K2, K, ViewK2), 
    readable(K1), covariant(ViewK1, ViewK2).
ptr_assignable(K, K1, K2) :- 
    assignable(K1, K2), capability(K), capability(K2), opaque(K1).

% check_nested_assignment_inconsistencies(K, K1, K2): succeeds 
% if K2 is assignable to K1, but the pointer type (T K) K2 is 
% not assignable to (T K) K1.
check_nested_assignment_inconsistencies(K, K1, K2) :- 
    assignable(K1, K2), \+ ptr_assignable(K, K1, K2).

% ptr_covariant(K, K1, K2): succeeds if the type (T K) K1 is 
% covariant to (T K) K2
ptr_covariant(K, K1, K2) :- 
    covariant(K1, K2), view(K1, K, ViewK1), view(K2, K, ViewK2), 
    mutable(K1), invariant(ViewK1, ViewK2).
ptr_covariant(K, K1, K2) :- 
    covariant(K1, K2), view(K1, K, ViewK1), view(K2, K, ViewK2), 
    readable(K1), covariant(ViewK1, ViewK2).
ptr_covariant(K, K1, K2) :- 
    covariant(K1, K2), capability(K), capability(K2), opaque(K1).

% check_nested_covariant_inconsistencies(K, K1, K2): succeeds if 
% K1 is covariant to K2, but the pointer type (T K) K1 is not 
% covariant to (T K) K2.
check_nested_covariant_inconsistencies(K, K1, K2) :- 
    covariant(K1, K2), \+ ptr_covariant(K, K1, K2).

% ptr_invariant(K, K1, K2): succeeds if the type (T K) K1 is 
% invariant to (T K) K2
ptr_invariant(K, K1, K2) :- 
    invariant(K1, K2), capability(K), capability(K2), opaque(K1).
ptr_invariant(K, K1, K2) :- 
    invariant(K1, K2), view(K1, K, ViewK1), 
    view(K2, K, ViewK2), invariant(ViewK1, ViewK2).

% check_nested_invariant_inconsistencies(K, K1, K2): succeeds if 
% K1 is invariant to K2, but the pointer type (T K) K1 is not 
% invariant to (T K) K2.
check_nested_invariant_inconsistencies(K, K1, K2) :- 
    invariant(K1, K2), \+ ptr_invariant(K, K1, K2).

% check_iso_unalias_inconsistencies(K): succeeds if the type 
% iso > K is not compatible with iso_cap > K.
check_iso_unalias_inconsistencies(K) :- 
    view(iso, K, iso), view(iso_cap, K, iso_cap), 
    \+ (invariant(IsoK, IsocapK)).
check_iso_unalias_inconsistencies(K) :-
    view(iso, K, IsoK), view(iso_cap, K, iso_cap), 
    \+ (covariant(IsocapK, IsoK)).


% sendable(K): succeeds if K is sendable
sendable(K) :- 
    capability(K), member(K, [iso, val, locked(L), tag]).
% iso_holdable(K): succeeds if having an iso reference as a 
% member of a pointer type protected by K does not break the
% alias invariant of iso. The idea is that only synchronized 
% types, or types guaranteeing the single alias invariant are 
% allowed to hold and send an iso.
iso_holdable(K) :- capability(K), member(K, [iso, locked(L)]).

% check_sendable_inconsistencies(K1, K2) succeeds if the rule 
% for K1 > K2 is leads to unsoundness when a pointer of type (T 
% K2) K1 is present as a parameter of a behaviour. 
check_sendable_inconsistencies(K1, K2) :- 
    sendable(K1), view(K1, K2, ref).
check_sendable_inconsistencies(K1, K2) :- 
    sendable(K1), \+ (iso_holdable(K1)), view(K1, K2, iso).

% Checks all the inconsistencies

check_inconsistencies :- check_nonassociativity(K1, K2, K3).
check_inconsistencies :- 
    check_nested_assignment_inconsistencies(K, K1, K2).
check_inconsistencies :- 
    check_nested_covariant_inconsistencies(K, K1, K2).
check_inconsistencies :- 
    check_nested_invariant_inconsistencies(K, K1, K2).
check_inconsistencies :- check_iso_unalias_inconsistencies(K).
check_inconsistencies :- check_sendable_inconsistencies(K1, K2).