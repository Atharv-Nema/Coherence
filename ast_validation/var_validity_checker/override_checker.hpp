#include "top_level.hpp"

bool override_check_callable(
    std::shared_ptr<TopLevelItem::Actor> curr_actor,
    std::vector<TopLevelItem::VarDecl>& params,
    std::vector<std::shared_ptr<Stmt>>& callable_body);