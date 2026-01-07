#include "sccs_finder.hpp"
#include "stage_utils.hpp"
#include <assert.h>
#include <functional>
#include <ranges>

CallableGraph reverse_graph(std::shared_ptr<CallableGraph> graph) {
    CallableGraph rev_graph;
    for(auto& [node, neighbours]: *graph) {
        for(auto neighbour: neighbours) {
            rev_graph[neighbour].insert(node);
        }
    }
    return rev_graph;
}

void find_sccs(std::shared_ptr<CallableGraph> graph) {
    std::vector<SyncCallable> visit_order;
    std::unordered_set<SyncCallable> visited;
    std::function<void(SyncCallable)> dfs;
    // First iteration
    dfs = [&](SyncCallable callable) {
        assert(!visited.contains(callable));
        visited.insert(callable);
        for(SyncCallable neighbour: (*graph)[callable]) {
            if(!visited.contains(neighbour)) {
                dfs(neighbour);
            }
        }
        visit_order.push_back(callable);
    };
    for(auto& [callable, _]: *graph) {
        if(!visited.contains(callable)) {
            dfs(callable);
        }
    }

    // Reverse the graph:
    CallableGraph rev_graph = reverse_graph(graph);
    // Second iteration
    std::shared_ptr<std::unordered_set<std::string>> callable_locks = 
        std::make_shared<std::unordered_set<std::string>>(); 
    std::function<void(SyncCallable)> label_components;
    label_components = [&](SyncCallable sync_callable) {
        assert(get_callable_locks(sync_callable) == nullptr);
        set_callable_locks(sync_callable, callable_locks);
        for(SyncCallable neighbour: rev_graph[sync_callable]) {

            if(get_callable_locks(neighbour) == nullptr) {
                label_components(neighbour);
            }
        }
    };
    for(auto& sync_callable: std::views::reverse(visit_order)) {
        if(get_callable_locks(sync_callable) == nullptr) {
            label_components(sync_callable);
            callable_locks = std::make_shared<std::unordered_set<std::string>>();
        }
    }
}