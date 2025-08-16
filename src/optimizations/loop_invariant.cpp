#include "optimizations.hpp"

void LoopInvariantPass::run(ProgramWithFunctions& program, Context& context) {
    // 遍历所有函数进行循环不变量外提
    for (const auto& func : program.get_functions()) {
        if (func->has_body()) {
            optimize_function(func.get(), context);
        }
    }
}

void LoopInvariantPass::optimize_function(FunctionDecl* func, Context& context) {
    if (func->get_body()) {
        auto loops = find_loops(func->get_body());
        for (const auto& loop_info : loops) {
            optimize_loop(loop_info, context);
        }
    }
}

std::vector<LoopInvariantPass::LoopInfo> LoopInvariantPass::find_loops(Stmt* stmt) const {
    std::vector<LoopInfo> loops;
    
    if (auto while_stmt = dynamic_cast<WhileStmt*>(stmt)) {
        LoopInfo info;
        info.loop = while_stmt;
        info.loop_vars = collect_loop_variables(while_stmt);
        loops.push_back(info);
        
        // 递归查找嵌套循环
        auto nested_loops = find_loops(const_cast<Stmt*>(while_stmt->get_body()));
        loops.insert(loops.end(), nested_loops.begin(), nested_loops.end());
    } else if (auto block = dynamic_cast<BlockStmt*>(stmt)) {
        for (const auto& s : block->get_stmts()) {
            auto nested_loops = find_loops(s.get());
            loops.insert(loops.end(), nested_loops.begin(), nested_loops.end());
        }
    } else if (auto if_stmt = dynamic_cast<IfStmt*>(stmt)) {
        auto then_loops = find_loops(const_cast<Stmt*>(if_stmt->get_then_stmt()));
        loops.insert(loops.end(), then_loops.begin(), then_loops.end());
        
        if (if_stmt->get_else_stmt()) {
            auto else_loops = find_loops(const_cast<Stmt*>(if_stmt->get_else_stmt()));
            loops.insert(loops.end(), else_loops.begin(), else_loops.end());
        }
    }
    
    return loops;
}

void LoopInvariantPass::optimize_loop(const LoopInfo& loop_info, Context& context) {
    WhileStmt* loop = loop_info.loop;
    const auto& loop_vars = loop_info.loop_vars;
    
    // 提取循环不变量
    auto invariant_stmts = extract_invariants(loop, context);
    
    if (!invariant_stmts.empty()) {
        // 创建新的循环体，移除不变量
        auto new_loop_body = create_optimized_loop_body(loop, invariant_stmts);
        
        // 替换循环体
        loop->set_body(std::move(new_loop_body));
    }
}

std::vector<std::unique_ptr<Stmt>> LoopInvariantPass::extract_invariants(WhileStmt* loop, Context& context) {
    std::vector<std::unique_ptr<Stmt>> invariant_stmts;
    std::vector<std::unique_ptr<Stmt>> non_invariant_stmts;
    
    auto loop_body = dynamic_cast<BlockStmt*>(const_cast<Stmt*>(loop->get_body()));
    if (!loop_body) {
        // 如果循环体不是块，包装成块
        auto wrapper_block = std::make_unique<BlockStmt>(std::vector<std::unique_ptr<Stmt>>{});
        wrapper_block->add_stmt(std::unique_ptr<Stmt>(loop->get_body()->clone()));
        loop->set_body(std::move(wrapper_block));
        loop_body = dynamic_cast<BlockStmt*>(loop->get_body());
    }
    
    const auto& stmts = loop_body->get_stmts();
    for (const auto& stmt : stmts) {
        if (is_invariant_stmt(stmt.get(), loop)) {
            invariant_stmts.push_back(std::unique_ptr<Stmt>(stmt->clone()));
        } else {
            non_invariant_stmts.push_back(std::unique_ptr<Stmt>(stmt->clone()));
        }
    }
    
    // 更新循环体，只保留非不变量语句
    loop_body->clear_stmts();
    for (auto& stmt : non_invariant_stmts) {
        loop_body->add_stmt(std::move(stmt));
    }
    
    return invariant_stmts;
}

bool LoopInvariantPass::is_invariant_stmt(Stmt* stmt, WhileStmt* loop) const {
    if (auto expr_stmt = dynamic_cast<ExprStmt*>(stmt)) {
        return is_invariant(const_cast<Expr*>(expr_stmt->get_expr()), loop);
    } else if (auto decl_stmt = dynamic_cast<DeclStmt*>(stmt)) {
        if (decl_stmt->get_expr()) {
            return is_invariant(const_cast<Expr*>(decl_stmt->get_expr()), loop);
        }
        return true; // 没有初始化的声明是不变的
    } else if (auto assign_stmt = dynamic_cast<AssignExpr*>(stmt)) {
        return is_invariant(const_cast<Expr*>(assign_stmt->get_expr()), loop);
    }
    
    return false;
}

bool LoopInvariantPass::is_invariant(Expr* expr, WhileStmt* loop) const {
    if (!expr) return true;
    
    // 收集循环中可能变化的变量
    auto loop_vars = collect_loop_variables(loop);
    
    return is_invariant(expr, loop_vars);
}

bool LoopInvariantPass::is_invariant(Expr* expr, const std::unordered_set<std::string>& loop_vars) const {
    if (!expr) return true;
    
    if (auto int_expr = dynamic_cast<IntegerExpr*>(expr)) {
        return true; // 常量总是不变的
    } else if (auto id_expr = dynamic_cast<IdExpr*>(expr)) {
        // 检查变量是否在循环中变化
        return loop_vars.find(id_expr->get_id()) == loop_vars.end();
    } else if (auto binary_op = dynamic_cast<BinaryOpExpr*>(expr)) {
        // 二元运算：两个操作数都必须是不变的
                return is_invariant(const_cast<Expr*>(binary_op->get_lhs()), loop_vars) &&
               is_invariant(const_cast<Expr*>(binary_op->get_rhs()), loop_vars);
    } else if (auto unary_op = dynamic_cast<UnaryOpExpr*>(expr)) {
        // 一元运算：操作数必须是不变的
        return is_invariant(const_cast<Expr*>(unary_op->get_expr()), loop_vars);
    } else if (auto func_call = dynamic_cast<FunctionCallExpr*>(expr)) {
        // 函数调用：假设有副作用，不是不变量
        return false;
    }
    
    return false;
}

std::unordered_set<std::string> LoopInvariantPass::collect_loop_variables(WhileStmt* loop) const {
    std::unordered_set<std::string> loop_vars;
    
    // 收集循环体中所有被赋值的变量
    collect_assigned_vars(const_cast<Stmt*>(loop->get_body()), loop_vars);
    
    return loop_vars;
}

void LoopInvariantPass::collect_assigned_vars(Stmt* stmt, std::unordered_set<std::string>& vars) const {
    if (auto block = dynamic_cast<BlockStmt*>(stmt)) {
        for (const auto& s : block->get_stmts()) {
            collect_assigned_vars(s.get(), vars);
        }
    } else if (auto if_stmt = dynamic_cast<IfStmt*>(stmt)) {
        collect_assigned_vars(const_cast<Stmt*>(if_stmt->get_then_stmt()), vars);
        if (if_stmt->get_else_stmt()) {
            collect_assigned_vars(const_cast<Stmt*>(if_stmt->get_else_stmt()), vars);
        }
    } else if (auto while_stmt = dynamic_cast<WhileStmt*>(stmt)) {
        collect_assigned_vars(const_cast<Stmt*>(while_stmt->get_body()), vars);
    } else if (auto assign_stmt = dynamic_cast<AssignExpr*>(stmt)) {
        vars.insert(assign_stmt->get_id()->get_id());
    } else if (auto decl_stmt = dynamic_cast<DeclStmt*>(stmt)) {
        vars.insert(decl_stmt->get_id()->get_id());
    }
}

std::unique_ptr<Stmt> LoopInvariantPass::create_optimized_loop_body(WhileStmt* loop, 
                                                                   const std::vector<std::unique_ptr<Stmt>>& invariant_stmts) {
    // 创建新的循环体，包含不变量语句
    auto new_body = std::make_unique<BlockStmt>(std::vector<std::unique_ptr<Stmt>>{});
    
    // 添加不变量语句到循环开始
    for (const auto& stmt : invariant_stmts) {
        new_body->add_stmt(std::unique_ptr<Stmt>(stmt->clone()));
    }
    
    // 添加原始循环体
    new_body->add_stmt(std::unique_ptr<Stmt>(const_cast<Stmt*>(loop->get_body())->clone()));
    
    return new_body;
}
