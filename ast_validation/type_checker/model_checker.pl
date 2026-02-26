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

% find_nonassociative(X, Y, Z) suceeds if (X |> Y) |> Z is not X |> (Y |> Z)
find_nonassociative(X, Y, Z) :-
    view(X, Y, T1), view(T1, Z, L), view(Y, Z, T2), \+ view(X, T2, L).

% assign(Target, Source): Source can be assigned to Target
assign(iso, iso).
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


% find_inconsistent_assignment(C1, C2, I) succeeds if it can find 
% capabilities C1, C2 and I such that assign(C1, C2) and not assign(C1 |> I, C2, I)

find_inconsistent_assignment(C1, C2, I) :- 
    assign(C1, C2), view(C1, I, I1), view(C2, I, I2), \+ assign(I1, I2).

% CR: Add all of the other necessary conditions