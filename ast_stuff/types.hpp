#pragma once
#include <string>
#include <vector>
#include <variant>
#include <optional>

struct Cap {
    struct Ref {};
    struct Val {};
    struct Iso {};
    struct Locked {std::string name; };
    std::variant<Ref, Val, Iso, Locked> value;
};

struct BasicType {
    struct TInt {};
    struct TFloat {};
    struct TBool {};
    struct TNamed { std::string name; };
    std::variant<TInt, TFloat, TBool, TNamed> value;
};

struct FullType {
    struct Raw { BasicType base; };
    struct Pointer { BasicType base; Cap cap; };
    std::variant<Raw, Pointer> value;
};

struct NameableType {
    struct Basic { BasicType type; };
    struct Struct { std::vector<std::pair<std::string, BasicType>> members; };
    std::variant<Basic, Struct> value;
};

using TypeDef = std::pair<std::string, NameableType>;
