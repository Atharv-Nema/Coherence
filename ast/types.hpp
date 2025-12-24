#pragma once
#include <string>
#include <vector>
#include <variant>
#include <optional>

struct Cap {
    struct Ref {};
    struct Val {};
    struct Iso {};
    struct Iso_cap {};
    struct Locked {std::string lock_name; };
    std::variant<Ref, Val, Iso, Iso_cap, Locked> t;
};

// [BasicType] represents types that can be present textually as members of a struct
struct BasicType {
    struct TUnit {};
    struct TInt {};
    struct TBool {};
    struct TNamed { std::string name; };
    struct TActor {std::string name;};
    std::variant<TUnit, TInt, TBool, TNamed, TActor> t;
};

// [FullType] represent all types that can be textually present in the type of variables/functions
struct FullType {
    // CR: Additional safety by distinguishing between pointers and arrays?
    struct Pointer { BasicType base; Cap cap; };
    std::variant<BasicType, Pointer> t;
};

// [NameableType] represents all types that can occur on the RHS of a type assignment
struct NameableType {
    // CR: Fix this to just use [BasicType] directly
    struct Basic { BasicType type; };
    struct Struct { std::vector<std::pair<std::string, BasicType>> members; };
    std::variant<Basic, Struct> t;
};