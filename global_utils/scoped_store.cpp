#include <unordered_map>
#include <variant>
#include <unordered_set>
#include <vector>
#include <optional>



template <typename K, typename V, typename Metadata = std::monostate>
class ScopedStore {
private:
    struct Scope {
        std::unordered_set<K> scope_keys;
        Metadata scope_metadata;
        template <typename... Args>
        explicit Scope(Args&&... args): scope_metadata(std::forward<Args>(args)...) {}
    };
    struct ValEntry {
        V val;
        // zero indexed
        size_t scope_ind;
        ValEntry(const V& val, size_t scope_ind): val(val), scope_ind(scope_ind) {}
    };
    std::vector<Scope> scope_list;
    std::unordered_map<K, std::vector<ValEntry>> key_val_map;
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
        size_t scope_ind = scope_list.size() - 1;
        key_val_map[key].emplace_back(value, scope_ind);
    }
    size_t num_scopes() { return scope_list.size(); }
    std::optional<V> get_value(const K& key) {
        if(key_val_map.find(key) == key_val_map.end()) {
            return std::nullopt;
        }
        assert(key_val_map[key].size() > 0);
        return key_val_map[key].back().val;
    }

    std::optional<size_t> get_scope_ind(const K& key) {
        if(key_val_map.find(key) == key_val_map.end()) {
            return std::nullopt;
        }
        assert(key_val_map[key].size() > 0);
        return key_val_map[key].back().scope_ind;
    }

    bool key_in_curr_scope(const K& key) {
        assert(scope_list.size() > 0);
        return scope_list.back().scope_keys.find(key) != scope_list.back().scope_keys.end();
    }

    Metadata& get_current_scope_metadata() {
        assert(!scope_list.empty());
        return scope_list.back().scope_metadata;
    }

    Metadata& get_scope_metadata(size_t scope_ind) {
        assert(scope_ind < scope_list.size());
        return scope_list[scope_ind];
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

template <typename K, typename V, typename Metadata>
struct ScopeGuard {
private:
    ScopedStore<K, V, Metadata>& scoped_context;
public:
    template <typename... Args>
    explicit ScopeGuard(ScopedStore<K, V, Metadata>& scoped_context, Args&&... scope_args)
    : scoped_context(scoped_context) {
        scoped_context.create_new_scope(std::forward<Args>(scope_args)...);
    }
    ~ScopeGuard() {
        scoped_context.pop_scope();
    }
};