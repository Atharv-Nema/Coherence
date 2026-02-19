#pragma once
#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <memory>

struct Cap {
    struct Tag {};
    struct Ref {};
    struct Val {};
    struct Iso {};
    struct Iso_cap {};
    struct Locked {std::string lock_name; };
    std::variant<Tag, Ref, Val, Iso, Iso_cap, Locked> t;
};


struct Type {
    struct TUnit {};
    struct TNullptr {};
    struct TInt {};
    struct TBool {};
    struct TNamed { std::string name; };
    struct TActor {std::string name;};
    struct Pointer {
        std::shared_ptr<const Type> base_type;
        Cap cap;
    };
    std::variant<TUnit, TNullptr, TInt, TBool, TNamed, TActor, Pointer> t;
    std::optional<Cap> viewpoint;
};

// [NameableType] represents all types that can occur on the RHS of a type assignment
struct NameableType {
    struct Struct { 
        std::vector<std::pair<std::string, std::shared_ptr<const Type>>> members; 
    };
    std::variant<std::shared_ptr<const Type>, Struct> t;
};