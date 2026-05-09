#include "return_checker.hpp"
#include "pattern_matching_boilerplate.hpp"
#include "utils.hpp"
#include <iostream>

// Perform an [and], making sure that invalid states propogate
ReturnStatus logical_and_with_invalids(ReturnStatus s1, ReturnStatus s2) {
    if(s1 == ReturnStatus::INVALID || s2 == ReturnStatus::INVALID) {
        return ReturnStatus::INVALID;
    }
    if((s1 == ReturnStatus::RETURNS) && (s2 == ReturnStatus::RETURNS)) {
        return ReturnStatus::RETURNS;
    }
    else {
        return ReturnStatus::DOESNT_RETURN;
    }
}

ReturnStatus statement_return_status(std::shared_ptr<Stmt> last_statement) {
    return std::visit(Overload{
        [&](const Stmt::Atomic& atomic_block) {
            return statement_list_return_status(atomic_block.body);
        },
        [&](const Stmt::Return& return_stmt) {
            // It is already checked that [return_stmt] has the correct type.
            // Perhaps an assert here will be good, but I guess semantically
            // this just checks that the statement returns.
            return ReturnStatus::RETURNS;
        },
        [&](const Stmt::If& if_stmt) {
            if(if_stmt.then_body.size() == 0) {
                return ReturnStatus::DOESNT_RETURN;
            }
            if(!if_stmt.else_body) {
                return ReturnStatus::DOESNT_RETURN;
            }
            if(if_stmt.else_body->size() == 0) {
                return ReturnStatus::DOESNT_RETURN;
            }
            ReturnStatus if_block_returns = statement_return_status(if_stmt.then_body.back());
            ReturnStatus else_block_returns = statement_return_status(if_stmt.else_body->back());
            return logical_and_with_invalids(if_block_returns, else_block_returns);
        },
        [&](const Stmt::While& while_stmt) {
            ReturnStatus while_returns = statement_list_return_status(while_stmt.body);
            return logical_and_with_invalids(ReturnStatus::DOESNT_RETURN, while_returns);
        },
        [&](const auto&) {return ReturnStatus::DOESNT_RETURN;}
    }, last_statement->t);
}

ReturnStatus statement_list_return_status(const std::vector<std::shared_ptr<Stmt>>& stmt_list) {
    if(stmt_list.size() == 0) {
        return ReturnStatus::DOESNT_RETURN;
    }
    for(size_t i = 0; i + 1 < stmt_list.size(); i++) {
        ReturnStatus stmt_return_status = statement_return_status(stmt_list[i]);
        if(stmt_return_status == ReturnStatus::RETURNS) {
            report_error_location(stmt_list[i]->source_span);
            std::cerr << "There is dead code below this statement" << std::endl;
            return ReturnStatus::INVALID;
        }
        else if(stmt_return_status == ReturnStatus::INVALID) {
            return ReturnStatus::INVALID;
        }
    }
    return statement_return_status(stmt_list.back());
}