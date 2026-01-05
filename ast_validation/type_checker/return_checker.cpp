#include "return_checker.hpp"
#include "pattern_matching_boilerplate.hpp"

bool statement_returns(std::shared_ptr<Stmt> last_statement) {
    // Adds error messages here I guess
    return std::visit(Overload{
        [&](const Stmt::Atomic& atomic_block) {
            if(atomic_block.body.size() == 0) {
                return false;
            }
            return statement_returns(atomic_block.body.back());
        },
        [&](const Stmt::Return& return_stmt) {
            // It is already checked that [return_stmt] has the correct type.
            // Perhaps an assert here will be good, but I guess semantically
            // this guy just checks that the statement returns.
            return true;
        },
        [&](const Stmt::If& if_stmt) {
            if(if_stmt.then_body.size() == 0) {
                return false;
            }
            if(!if_stmt.else_body) {
                return false;
            }
            if(if_stmt.else_body->size() == 0) {
                return false;
            }
            bool if_block_returns = statement_returns(if_stmt.then_body.back());
            bool else_block_returns = statement_returns(if_stmt.else_body->back());
            return if_block_returns && else_block_returns;
        },
        [&](const auto&) {return false;}
    }, last_statement->t);
}