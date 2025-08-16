#include "optimizations.hpp"
#include <cmath>

void StrengthReductionPass::run(ProgramWithFunctions& program, Context& context) {
    // 遍历所有函数进行强度缩减优化
    for (const auto& func : program.get_functions()) {
        if (func->has_body()) {
            optimize_function(func.get(), context);
        }
    }
}

void StrengthReductionPass::optimize_function(FunctionDecl* func, Context& context) {
    if (func->get_body()) {
        optimize_stmt(func->get_body(), context);
    }
}

void StrengthReductionPass::optimize_stmt(Stmt* stmt, Context& context) {
    if (auto block = dynamic_cast<BlockStmt*>(stmt)) {
        for (auto& s : block->get_stmts()) {
            optimize_stmt(s.get(), context);
        }
    } else if (auto if_stmt = dynamic_cast<IfStmt*>(stmt)) {
        optimize_stmt(if_stmt->get_then_stmt(), context);
        if (if_stmt->get_else_stmt()) {
            optimize_stmt(if_stmt->get_else_stmt(), context);
        }
    } else if (auto while_stmt = dynamic_cast<WhileStmt*>(stmt)) {
        optimize_stmt(while_stmt->get_body(), context);
    } else if (auto expr_stmt = dynamic_cast<ExprStmt*>(stmt)) {
        auto optimized_expr = optimize_expr(expr_stmt->get_expr(), context);
        if (optimized_expr) {
            expr_stmt->set_expr(std::move(optimized_expr));
        }
    } else if (auto return_stmt = dynamic_cast<ReturnStmt*>(stmt)) {
        if (return_stmt->get_expr()) {
            auto optimized_expr = optimize_expr(return_stmt->get_expr(), context);
            if (optimized_expr) {
                return_stmt->set_expr(std::move(optimized_expr));
            }
        }
    } else if (auto assign_stmt = dynamic_cast<AssignExpr*>(stmt)) {
        auto optimized_expr = optimize_expr(assign_stmt->get_expr(), context);
        if (optimized_expr) {
            assign_stmt->set_expr(std::move(optimized_expr));
        }
    }
}

std::unique_ptr<Expr> StrengthReductionPass::optimize_expr(Expr* expr, Context& context) {
    if (auto binary_op = dynamic_cast<BinaryOpExpr*>(expr)) {
        return optimize_binary_op(binary_op, context);
    } else if (auto unary_op = dynamic_cast<UnaryOpExpr*>(expr)) {
        auto optimized_subexpr = optimize_expr(unary_op->get_expr(), context);
        if (optimized_subexpr) {
            unary_op->set_expr(std::move(optimized_subexpr));
        }
    }
    return nullptr;
}

std::unique_ptr<Expr> StrengthReductionPass::optimize_binary_op(BinaryOpExpr* expr, Context& context) {
    const std::string& op = expr->get_op();
    
    // 优化乘法运算
    if (op == "*") {
        // 检查左操作数是否为常量
        if (auto int_lhs = dynamic_cast<IntegerExpr*>(expr->get_lhs())) {
            int value = int_lhs->get_value();
            if (value == 0) {
                // x * 0 -> 0
                return std::make_unique<IntegerExpr>(0);
            } else if (value == 1) {
                // x * 1 -> x
                return std::unique_ptr<Expr>(expr->get_rhs()->clone());
            } else if (is_power_of_two(value)) {
                // x * 2^n -> x << n
                int shift = get_power_of_two(value);
                auto shift_expr = std::make_unique<BinaryOpExpr>(
                    "<<", 
                    std::unique_ptr<Expr>(expr->get_rhs()->clone()),
                    std::make_unique<IntegerExpr>(shift)
                );
                return shift_expr;
            }
        }
        
        // 检查右操作数是否为常量
        if (auto int_rhs = dynamic_cast<IntegerExpr*>(expr->get_rhs())) {
            int value = int_rhs->get_value();
            if (value == 0) {
                // x * 0 -> 0
                return std::make_unique<IntegerExpr>(0);
            } else if (value == 1) {
                // x * 1 -> x
                return std::unique_ptr<Expr>(expr->get_lhs()->clone());
            } else if (is_power_of_two(value)) {
                // x * 2^n -> x << n
                int shift = get_power_of_two(value);
                auto shift_expr = std::make_unique<BinaryOpExpr>(
                    "<<", 
                    std::unique_ptr<Expr>(expr->get_lhs()->clone()),
                    std::make_unique<IntegerExpr>(shift)
                );
                return shift_expr;
            }
        }
    }
    
    // 优化除法运算
    else if (op == "/") {
        // 检查右操作数是否为2的幂
        if (auto int_rhs = dynamic_cast<IntegerExpr*>(expr->get_rhs())) {
            int value = int_rhs->get_value();
            if (value == 1) {
                // x / 1 -> x
                return std::unique_ptr<Expr>(expr->get_lhs()->clone());
            } else if (is_power_of_two(value)) {
                // x / 2^n -> x >> n (对于正数)
                int shift = get_power_of_two(value);
                auto shift_expr = std::make_unique<BinaryOpExpr>(
                    ">>", 
                    std::unique_ptr<Expr>(expr->get_lhs()->clone()),
                    std::make_unique<IntegerExpr>(shift)
                );
                return shift_expr;
            }
        }
    }
    
    // 优化取模运算
    else if (op == "%") {
        // 检查右操作数是否为2的幂
        if (auto int_rhs = dynamic_cast<IntegerExpr*>(expr->get_rhs())) {
            int value = int_rhs->get_value();
            if (value == 1) {
                // x % 1 -> 0
                return std::make_unique<IntegerExpr>(0);
            } else if (is_power_of_two(value)) {
                // x % 2^n -> x & (2^n - 1)
                int mask = value - 1;
                auto and_expr = std::make_unique<BinaryOpExpr>(
                    "&", 
                    std::unique_ptr<Expr>(expr->get_lhs()->clone()),
                    std::make_unique<IntegerExpr>(mask)
                );
                return and_expr;
            }
        }
    }
    
    // 优化加法运算
    else if (op == "+") {
        if (auto int_lhs = dynamic_cast<IntegerExpr*>(expr->get_lhs())) {
            if (int_lhs->get_value() == 0) {
                // 0 + x -> x
                return std::unique_ptr<Expr>(expr->get_rhs()->clone());
            }
        }
        if (auto int_rhs = dynamic_cast<IntegerExpr*>(expr->get_rhs())) {
            if (int_rhs->get_value() == 0) {
                // x + 0 -> x
                return std::unique_ptr<Expr>(expr->get_lhs()->clone());
            }
        }
    }
    
    // 优化减法运算
    else if (op == "-") {
        if (auto int_rhs = dynamic_cast<IntegerExpr*>(expr->get_rhs())) {
            if (int_rhs->get_value() == 0) {
                // x - 0 -> x
                return std::unique_ptr<Expr>(expr->get_lhs()->clone());
            }
        }
    }
    
    // 递归优化子表达式
    auto optimized_lhs = optimize_expr(expr->get_lhs(), context);
    auto optimized_rhs = optimize_expr(expr->get_rhs(), context);
    
    if (optimized_lhs) {
        expr->set_lhs(std::move(optimized_lhs));
    }
    if (optimized_rhs) {
        expr->set_rhs(std::move(optimized_rhs));
    }
    
    return nullptr;
}

bool StrengthReductionPass::is_power_of_two(int value) const {
    return value > 0 && (value & (value - 1)) == 0;
}

int StrengthReductionPass::get_power_of_two(int value) const {
    if (!is_power_of_two(value)) return 0;
    int power = 0;
    while (value > 1) {
        value >>= 1;
        power++;
    }
    return power;
}
