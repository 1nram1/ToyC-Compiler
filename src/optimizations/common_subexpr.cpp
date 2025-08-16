#include "optimizations.hpp"
#include <sstream>

// Initialize static counter
int CommonSubexprPass::temp_counter = 0;

void CommonSubexprPass::run(ProgramWithFunctions& program, Context& context) {
    // 遍历所有函数进行公共子表达式消除
    for (const auto& func : program.get_functions()) {
        if (func->has_body()) {
            optimize_function(func.get(), context);
        }
    }
}

void CommonSubexprPass::optimize_function(FunctionDecl* func, Context& context) {
    if (func->get_body()) {
        expr_cache.clear();
        optimize_stmt(func->get_body(), context);
    }
}

void CommonSubexprPass::optimize_stmt(Stmt* stmt, Context& context) {
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
    } else if (auto expr_stmt = dynamic_cast<ExprStmt*>(stmt)) {
        if (auto assign_expr = dynamic_cast<AssignExpr*>(expr_stmt->get_expr())) {
            auto optimized_expr = optimize_expr(assign_expr->get_expr(), context);
            if (optimized_expr) {
                assign_expr->set_expr(std::move(optimized_expr));
            }
        }
    }
}

std::unique_ptr<Expr> CommonSubexprPass::optimize_expr(Expr* expr, Context& context) {
    if (!expr) return nullptr;
    
    // 生成表达式哈希
    std::string hash = generate_expr_hash(expr);
    
    // 检查是否已经计算过这个表达式
    auto it = expr_cache.find(hash);
    if (it != expr_cache.end()) {
        // 找到公共子表达式，替换为临时变量
        std::cout << "# CSE: Replacing duplicate expression with " << it->second.temp_var << std::endl;
        return replace_with_temp(it->second.temp_var);
    }
    
    // 对于复杂的表达式，创建临时变量并缓存
    if (should_cache_expr(expr)) {
        std::string temp_var = generate_temp_var();
        expr_cache[hash] = {hash, std::unique_ptr<Expr>(expr->clone()), temp_var, true};
        
        std::cout << "# CSE: Caching expression as " << temp_var << std::endl;
        
        // 递归优化子表达式
        if (auto binary_op = dynamic_cast<BinaryOpExpr*>(expr)) {
            auto optimized_lhs = optimize_expr(binary_op->get_lhs(), context);
            auto optimized_rhs = optimize_expr(binary_op->get_rhs(), context);
            
            if (optimized_lhs) {
                binary_op->set_lhs(std::move(optimized_lhs));
            }
            if (optimized_rhs) {
                binary_op->set_rhs(std::move(optimized_rhs));
            }
        } else if (auto unary_op = dynamic_cast<UnaryOpExpr*>(expr)) {
            auto optimized_subexpr = optimize_expr(unary_op->get_expr(), context);
            if (optimized_subexpr) {
                unary_op->set_expr(std::move(optimized_subexpr));
            }
        }
        
        return replace_with_temp(temp_var);
    }
    
    // 递归优化子表达式
    if (auto binary_op = dynamic_cast<BinaryOpExpr*>(expr)) {
        auto optimized_lhs = optimize_expr(binary_op->get_lhs(), context);
        auto optimized_rhs = optimize_expr(binary_op->get_rhs(), context);
        
        if (optimized_lhs) {
            binary_op->set_lhs(std::move(optimized_lhs));
        }
        if (optimized_rhs) {
            binary_op->set_rhs(std::move(optimized_rhs));
        }
    } else if (auto unary_op = dynamic_cast<UnaryOpExpr*>(expr)) {
        auto optimized_subexpr = optimize_expr(unary_op->get_expr(), context);
        if (optimized_subexpr) {
            unary_op->set_expr(std::move(optimized_subexpr));
        }
    }
    
    return nullptr;
}

std::string CommonSubexprPass::generate_expr_hash(Expr* expr) const {
    if (!expr) return "";
    
    std::ostringstream oss;
    
    if (auto int_expr = dynamic_cast<IntegerExpr*>(expr)) {
        oss << "INT:" << int_expr->get_value();
    } else if (auto id_expr = dynamic_cast<IdExpr*>(expr)) {
        oss << "ID:" << id_expr->get_id();
    } else if (auto binary_op = dynamic_cast<BinaryOpExpr*>(expr)) {
        oss << "BIN:" << binary_op->get_op() << "(" 
            << generate_expr_hash(binary_op->get_lhs()) << ","
            << generate_expr_hash(binary_op->get_rhs()) << ")";
    } else if (auto unary_op = dynamic_cast<UnaryOpExpr*>(expr)) {
        oss << "UNARY:" << unary_op->get_op() << "(" 
            << generate_expr_hash(unary_op->get_expr()) << ")";
    } else if (auto func_call = dynamic_cast<FunctionCallExpr*>(expr)) {
        oss << "CALL:" << func_call->get_function_name() << "(";
        const auto& args = func_call->get_arguments();
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) oss << ",";
            oss << generate_expr_hash(args[i].get());
        }
        oss << ")";
    }
    
    return oss.str();
}

bool CommonSubexprPass::should_cache_expr(Expr* expr) const {
    if (!expr) return false;
    
    // 缓存复杂的表达式
    if (auto binary_op = dynamic_cast<BinaryOpExpr*>(expr)) {
        // 缓存算术和逻辑运算
        const std::string& op = binary_op->get_op();
        return op == "+" || op == "-" || op == "*" || op == "/" || op == "%" ||
               op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=" ||
               op == "&&" || op == "||";
    } else if (auto unary_op = dynamic_cast<UnaryOpExpr*>(expr)) {
        // 缓存一元运算
        const std::string& op = unary_op->get_op();
        return op == "-" || op == "!";
    } else if (auto func_call = dynamic_cast<FunctionCallExpr*>(expr)) {
        // 缓存函数调用（假设是纯函数）
        return true;
    }
    
    return false;
}

std::unique_ptr<Expr> CommonSubexprPass::replace_with_temp(const std::string& temp_var) const {
    return std::make_unique<IdExpr>(temp_var);
}

std::string CommonSubexprPass::generate_temp_var() const {
    return "_cse_" + std::to_string(++temp_counter);
}
