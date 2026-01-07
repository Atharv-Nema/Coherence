#include "callable_graph_builder.hpp"
#include "sccs_finder.hpp"
#include "compute_lock_info.hpp"
#include "fill_lock_info.hpp"
#include <functional>
#include <assert.h>

void dfs_filling(
    std::shared_ptr<CallableGraph> callable_graph,
    std::shared_ptr<DeclCollection> decl_collection) {
    std::unordered_set<SyncCallable> visited;
    std::function<void(SyncCallable)> dfs;
    dfs = [&](SyncCallable sync_callable) {
        assert(!visited.contains(sync_callable));
        visited.insert(sync_callable);
        for(auto neighbour: callable_graph->at(sync_callable)) {
            if(!visited.contains(neighbour)) {
                dfs(neighbour);
            }
        }
        fill_lock_info(sync_callable, decl_collection);
    };
    for(auto& [sync_callable, _]: *callable_graph) {
        if(!visited.contains(sync_callable)) {
            dfs(sync_callable);
        }
    }
}

void compute_lock_info(Program* root, std::shared_ptr<DeclCollection> decl_collection) {
    // 1. Create the graph
    std::shared_ptr<CallableGraph> callable_graph = build_graph(root, decl_collection);
    // 2. Fill out the connected components
    find_sccs(callable_graph);
    // 3. Need to do a dfs twice so that the atomic information is filled correctly 
    // CR: Seems like a terribly hacky solution. Think of a better way perhaps?
    dfs_filling(callable_graph, decl_collection);
    dfs_filling(callable_graph, decl_collection);
}