#include "callable_graph_builder.hpp"
#include "sccs_finder.hpp"
#include "compute_lock_info.hpp"
#include "fill_callable_lock_info.hpp"
#include "fill_atomic_lock_info.hpp"
#include <functional>
#include <assert.h>

void compute_lock_info(Program* root, std::shared_ptr<DeclCollection> decl_collection) {
    // 1. Create the graph
    std::shared_ptr<CallableGraph> callable_graph = build_graph(root, decl_collection);
    // 2. Fill out the connected components
    find_sccs(callable_graph);
    // 3. Fill the function lock info
    fill_all_callable_lock_info(callable_graph, decl_collection);
    // 4. Fill atomic section info
    fill_atomic_lock_info(root, decl_collection);
}