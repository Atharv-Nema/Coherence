#include <algorithm>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include <ranges>

#include "callable_lock_info.hpp"
#include "nlohmann/json.hpp"

namespace fs = std::filesystem;
using nlohmann::json;

struct Expectation {
    std::string name;
    std::vector<std::string> locks;
    std::optional<std::string> actor; // null => top-level
    CallableType type;
};

static void print_vec(std::vector<std::string>& vec) {
    if(vec.size() == 0) {
        std::cerr << "[]";
        return;
    }
    std::cerr << "[" << vec[0];
    for(const std::string& ele: std::views::drop(vec, 1)) {
        std::cerr << ", " << ele;
    }
    std::cerr << "]" << std::endl;
}

static void print_err_message(Expectation& e, std::vector<std::string>& received_locks) {
    
}

static std::string slurp(const fs::path& p) {
    std::ifstream in(p);
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

static std::vector<Expectation> load_expectations(const fs::path& p) {
    json arr = json::parse(slurp(p));
    std::vector<Expectation> out;
    out.reserve(arr.size());

    for (const auto& o : arr) {
        Expectation e;
        e.name  = o.at("name").get<std::string>();
        e.locks = o.at("locks").get<std::vector<std::string>>();
        std::sort(e.locks.begin(), e.locks.end());

        if (o.contains("actor") && !o.at("actor").is_null())
            e.actor = o.at("actor").get<std::string>();

        bool is_func = o.at("function").get<bool>();
        e.type = is_func ? CallableType::FUNCTION : CallableType::CONSTRUCTOR;

        out.push_back(std::move(e));
    }

    return out;
}

int main(int argc, char** argv) {
    assert(argc == 2);

    fs::path case_dir = argv[1];
    fs::path prog = case_dir / "prog.coh";
    fs::path exp  = case_dir / "expected.json";

    auto expectations = load_expectations(exp);

    FILE* f = std::fopen(prog.string().c_str(), "rb");
    assert(f && "failed to open prog.coh");

    CallableLockInfo info(f);
    std::fclose(f);
    int ret_code = 0;
    for (Expectation& e : expectations) {
        auto got = info.lock_info(e.name, e.type, e.actor);
        if (got != e.locks) {
            std::cerr << "------Test case-------" << std::endl;
            std::cerr << "Filename: " << case_dir.string() << std::endl;
            std::cerr << "Actor name " << e.actor.value_or("#TOPLEVEL#") << std::endl;
            std::cerr << "Callable name " << e.name << std::endl;
            std::cerr << "Callable is function? " << std::boolalpha << (e.type == CallableType::FUNCTION) 
                << std::noboolalpha << std::endl;
            std::cerr << "Expected locks: ";
            print_vec(e.locks);
            std::cerr << "Present locks: ";
            print_vec(got);
            ret_code = 1;
        }
    }
    return ret_code;
}
