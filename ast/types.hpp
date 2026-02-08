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


struct Type {
    struct TUnit {};
    struct TInt {};
    struct TBool {};
    struct TNamed { std::string name; };
    struct TActor {std::string name;};
    struct Pointer {
        std::shared_ptr<Type> base_type;
        Cap cap;
    };
    std::variant<TUnit, TInt, TBool, TNamed, TActor, Pointer> t;
    std::optional<Cap> viewpoint;
};

// [NameableType] represents all types that can occur on the RHS of a type assignment
struct NameableType {
    struct Struct { 
        std::vector<std::pair<std::string, std::shared_ptr<Type>>> members; 
    };
    std::variant<Type, Struct> t;
};