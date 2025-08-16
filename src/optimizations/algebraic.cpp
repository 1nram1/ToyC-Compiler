#include "optimizations.hpp"

void AlgebraicPass::run(ProgramWithFunctions& program, Context& context) {
    // 遍历所有函数进行代数优化
    for (const auto& func : program.get_functions()) {
        if (func->has_body()) {
            optimize_function(func.get(), context);
        }
    }
}

void AlgebraicPass::optimize_function(FunctionDecl* func, Context& context) {
    if (func->get_body()) {
        optimize_stmt(const_cast<Stmt*>(func->get_body()), context);
    }
}

void AlgebraicPass::optimize_stmt(Stmt* stmt, Context& context) {
    if (auto block = dynamic_cast<BlockStmt*>(stmt)) {
        for (auto& s : block->get_stmts()) {
            optimize_stmt(s.get(), context);
        }
    } else if (auto if_stmt = dynamic_cast<IfStmt*>(stmt)) {
        optimize_stmt(const_cast<Stmt*>(if_stmt->get_then_stmt()), context);
        if (if_stmt->get_else_stmt()) {
            optimize_stmt(const_cast<Stmt*>(if_stmt->get_else_stmt()), context);
        }
    } else if (auto while_stmt = dynamic_cast<WhileStmt*>(stmt)) {
        optimize_stmt(const_cast<Stmt*>(while_stmt->get_body()), context);
    } else if (auto expr_stmt = dynamic_cast<ExprStmt*>(stmt)) {
        auto optimized_expr = optimize_expr(const_cast<Expr*>(expr_stmt->get_expr()), context);
        if (optimized_expr) {
            expr_stmt->set_expr(std::move(optimized_expr));
        }
    } else if (auto return_stmt = dynamic_cast<ReturnStmt*>(stmt)) {
        if (return_stmt->get_expr()) {
            auto optimized_expr = optimize_expr(const_cast<Expr*>(return_stmt->get_expr()), context);
            if (optimized_expr) {
                return_stmt->set_expr(std::move(optimized_expr));
            }
        }
    } else if (auto assign_stmt = dynamic_cast<AssignExpr*>(stmt)) {
        auto optimized_expr = optimize_expr(const_cast<Expr*>(assign_stmt->get_expr()), context);
        if (optimized_expr) {
            assign_stmt->set_expr(std::move(optimized_expr));
        }
    }
}

std::unique_ptr<Expr> AlgebraicPass::optimize_expr(Expr* expr, Context& context) {
    if (!expr) return nullptr;
    
    // 常量折叠
    if (is_constant_foldable(expr, context)) {
        int result = fold_constant(expr, context);
        return std::make_unique<IntegerExpr>(result);
    }
    
    if (auto binary_op = dynamic_cast<BinaryOpExpr*>(expr)) {
        return optimize_binary_op(binary_op, context);
    } else if (auto unary_op = dynamic_cast<UnaryOpExpr*>(expr)) {
        auto optimized_subexpr = optimize_expr(const_cast<Expr*>(unary_op->get_expr()), context);
        if (optimized_subexpr) {
            unary_op->set_expr(std::move(optimized_subexpr));
        }
    }
    
    return nullptr;
}

std::unique_ptr<Expr> AlgebraicPass::optimize_binary_op(BinaryOpExpr* expr, Context& context) {
    const std::string& op = expr->get_op();
    
    // 代数恒等式优化
    if (op == "+") {
        // (x + 0) -> x, (0 + x) -> x
        if (auto int_lhs = dynamic_cast<IntegerExpr*>(const_cast<Expr*>(expr->get_lhs()))) {
            if (int_lhs->get_value() == 0) {
                return std::unique_ptr<Expr>(expr->get_rhs()->clone());
            }
        }
        if (auto int_rhs = dynamic_cast<IntegerExpr*>(const_cast<Expr*>(expr->get_rhs()))) {
            if (int_rhs->get_value() == 0) {
                return std::unique_ptr<Expr>(expr->get_lhs()->clone());
            }
        }
    } else if (op == "-") {
        // (x - 0) -> x, (x - x) -> 0
        if (auto int_rhs = dynamic_cast<IntegerExpr*>(const_cast<Expr*>(expr->get_rhs()))) {
            if (int_rhs->get_value() == 0) {
                return std::unique_ptr<Expr>(expr->get_lhs()->clone());
            }
        }
        // 检查 x - x
        if (auto id_lhs = dynamic_cast<IdExpr*>(const_cast<Expr*>(expr->get_lhs()))) {
            if (auto id_rhs = dynamic_cast<IdExpr*>(const_cast<Expr*>(expr->get_rhs()))) {
                if (id_lhs->get_id() == id_rhs->get_id()) {
                    return std::make_unique<IntegerExpr>(0);
                }
            }
        }
    } else if (op == "*") {
        // (x * 0) -> 0, (x * 1) -> x, (0 * x) -> 0, (1 * x) -> x
        if (auto int_lhs = dynamic_cast<IntegerExpr*>(const_cast<Expr*>(expr->get_lhs()))) {
            int val = int_lhs->get_value();
            if (val == 0) {
                return std::make_unique<IntegerExpr>(0);
            } else if (val == 1) {
                return std::unique_ptr<Expr>(expr->get_rhs()->clone());
            }
        }
        if (auto int_rhs = dynamic_cast<IntegerExpr*>(const_cast<Expr*>(expr->get_rhs()))) {
            int val = int_rhs->get_value();
            if (val == 0) {
                return std::make_unique<IntegerExpr>(0);
            } else if (val == 1) {
                return std::unique_ptr<Expr>(expr->get_lhs()->clone());
            }
        }
    } else if (op == "/") {
        // (x / 1) -> x, (0 / x) -> 0
        if (auto int_lhs = dynamic_cast<IntegerExpr*>(const_cast<Expr*>(expr->get_lhs()))) {
            if (int_lhs->get_value() == 0) {
                return std::make_unique<IntegerExpr>(0);
            }
        }
        if (auto int_rhs = dynamic_cast<IntegerExpr*>(const_cast<Expr*>(expr->get_rhs()))) {
            if (int_rhs->get_value() == 1) {
                return std::unique_ptr<Expr>(expr->get_lhs()->clone());
            }
        }
    } else if (op == "%") {
        // (x % 1) -> 0
        if (auto int_rhs = dynamic_cast<IntegerExpr*>(const_cast<Expr*>(expr->get_rhs()))) {
            if (int_rhs->get_value() == 1) {
                return std::make_unique<IntegerExpr>(0);
            }
        }
    }
    
    // 递归优化子表达式
    auto optimized_lhs = optimize_expr(const_cast<Expr*>(expr->get_lhs()), context);
    auto optimized_rhs = optimize_expr(const_cast<Expr*>(expr->get_rhs()), context);
    
    if (optimized_lhs) {
        expr->set_lhs(std::move(optimized_lhs));
    }
    if (optimized_rhs) {
        expr->set_rhs(std::move(optimized_rhs));
    }
    
    return nullptr;
}

bool AlgebraicPass::is_constant_foldable(Expr* expr, Context& context) const {
    if (!expr) return false;
    
    if (auto binary_op = dynamic_cast<BinaryOpExpr*>(expr)) {
        return binary_op->get_lhs()->is_constant(context) && binary_op->get_rhs()->is_constant(context);
    } else if (auto unary_op = dynamic_cast<UnaryOpExpr*>(expr)) {
        return unary_op->get_expr()->is_constant(context);
    }
    
    return false;
}

int AlgebraicPass::fold_constant(Expr* expr, Context& context) const {
    if (!expr) return 0;
    
    if (auto binary_op = dynamic_cast<BinaryOpExpr*>(expr)) {
        int lhs_val = binary_op->get_lhs()->evaluate_constant(context);
        int rhs_val = binary_op->get_rhs()->evaluate_constant(context);
        const std::string& op = binary_op->get_op();
        
        if (op == "+") return lhs_val + rhs_val;
        if (op == "-") return lhs_val - rhs_val;
        if (op == "*") return lhs_val * rhs_val;
        if (op == "/") return rhs_val != 0 ? lhs_val / rhs_val : 0;
        if (op == "%") return rhs_val != 0 ? lhs_val % rhs_val : 0;
        if (op == "==") return lhs_val == rhs_val;
        if (op == "!=") return lhs_val != rhs_val;
        if (op == "<") return lhs_val < rhs_val;
        if (op == ">") return lhs_val > rhs_val;
        if (op == "<=") return lhs_val <= rhs_val;
        if (op == ">=") return lhs_val >= rhs_val;
        if (op == "&&") return lhs_val && rhs_val;
        if (op == "||") return lhs_val || rhs_val;
    } else if (auto unary_op = dynamic_cast<UnaryOpExpr*>(expr)) {
        int val = unary_op->get_expr()->evaluate_constant(context);
        const std::string& op = unary_op->get_op();
        
        if (op == "-") return -val;
        if (op == "!") return !val;
    }
    
    return 0;
}
