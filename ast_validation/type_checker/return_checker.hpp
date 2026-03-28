#include <memory>
#include "stmts.hpp"

enum class ReturnStatus {
    INVALID, RETURNS, DOESNT_RETURN
};
ReturnStatus statement_list_return_status(const std::vector<std::shared_ptr<Stmt>>& stmt_list);