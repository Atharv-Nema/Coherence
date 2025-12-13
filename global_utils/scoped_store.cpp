#include <unordered_map>
#include <variant>
#include <unordered_set>
#include <vector>
#include <optional>

template <typename T>
struct ScopeGuard {
private:
    T& scoped_context;
public:
    template <typename... Args>
    explicit ScopeGuard(T& scoped_context, Args&&... scope_args): scoped_context(scoped_context) {
        scoped_context.create_new_scope(std::forward<Args>(scope_args)...);
    }
    ~ScopeGuard() {
        scoped_context.pop_scope();
    }
};

template <typename K, typename V, typename Metadata = std::monostate>
class ScopedStore {
private:
    struct Scope {
        std::unordered_set<K> scope_keys;
        Metadata scope_metadata;
        template <typename... Args>
        explicit Scope(Args&&... args): scope_metadata(std::forward<Args>(args)...) {}
    };
    std::vector<Scope> scope_list;
    std::unordered_map<K, std::vector<V>> key_val_map;
public:
    ScopedStore() {}
    template <typename... Args>
    void create_new_scope(Args&&... meta_args) {
        scope_list.emplace_back(std::forward<Args>(meta_args)...);
    }
    // creates a copy and stores it in the map
    void insert(const K& key, const V& value) {
        // Note that insert assumes that [key] is not in the latest scope
        assert(scope_list.size() > 0);
        assert(scope_list.back().scope_keys.find(key) == scope_list.back().scope_keys.end());
        scope_list.back().scope_keys.insert(key);
        key_val_map[key].push_back(value);
    }
    size_t num_scopes() { return scope_list.size(); }
    std::optional<V> get_value(const K& key) {
        if(key_val_map.find(key) == key_val_map.end()) {
            return std::nullopt;
        }
        assert(key_val_map[key].size() > 0);
        return key_val_map[key].back();
    }

    bool key_in_curr_scope(const K& key) {
        assert(scope_list.size() > 0);
        return scope_list.back().scope_keys.find(key) != scope_list.back().scope_keys.end();
    }

    Metadata& get_current_scope_metadata() {
        assert(!scope_list.empty());
        return scope_list.back().scope_metadata;
    }

    void pop_scope() {
        assert(scope_list.size() > 0);
        for(const K& k: scope_list.back().scope_keys) {
            assert(key_val_map.find(k) != key_val_map.end());
            assert(key_val_map[k].size() > 0);
            key_val_map[k].pop_back();
        }
        scope_list.pop_back();
    }
};