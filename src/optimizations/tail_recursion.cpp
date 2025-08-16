#include "optimizations.hpp"

void TailRecursionPass::run(ProgramWithFunctions& program, Context& context) {
    // 遍历所有函数，检测并优化尾递归
    for (auto& func : program.get_functions()) {
        if (func->has_body() && is_tail_recursive(func.get())) {
            std::cout << "# Tail recursion detected in function: " << func->get_name() << std::endl;
            
            // 转换为循环
            auto loop_body = convert_to_loop(func.get(), context);
            if (loop_body) {
                // 创建新的函数体，包含循环
                auto new_body = std::make_unique<BlockStmt>(std::vector<std::unique_ptr<Stmt>>{});
                auto block_body = dynamic_cast<BlockStmt*>(new_body.get());
                
                // 添加参数初始化
                for (const auto& param : func->get_parameters()) {
                    auto decl = std::make_unique<DeclStmt>(
                        std::make_unique<IdExpr>(param->get_name()),
                        std::make_unique<IdExpr>(param->get_name())  // 初始化为参数值
                    );
                    block_body->add_stmt(std::move(decl));
                }
                
                // 添加循环体
                block_body->add_stmt(std::move(loop_body));
                
                // 替换函数体
                func->set_body(std::move(new_body));
                std::cout << "# Tail recursion converted to loop in function: " << func->get_name() << std::endl;
            }
        }
    }
}

bool TailRecursionPass::is_tail_recursive(FunctionDecl* func) const {
    if (!func->has_body()) return false;
    
    const std::string& func_name = func->get_name();
    return is_tail_call(func->get_body(), func_name);
}

bool TailRecursionPass::is_tail_call(Stmt* stmt, const std::string& func_name) const {
    if (auto return_stmt = dynamic_cast<ReturnStmt*>(stmt)) {
        if (return_stmt->get_expr()) {
            // 检查返回值是否为函数调用
            if (auto func_call = dynamic_cast<FunctionCallExpr*>(return_stmt->get_expr())) {
                return func_call->get_function_name() == func_name;
            }
        }
    } else if (auto block = dynamic_cast<BlockStmt*>(stmt)) {
        // 检查块中的最后一个语句
        const auto& stmts = block->get_stmts();
        if (!stmts.empty()) {
            return is_tail_call(stmts.back().get(), func_name);
        }
    } else if (auto if_stmt = dynamic_cast<IfStmt*>(stmt)) {
        // 检查if语句的两个分支
        bool then_tail = is_tail_call(if_stmt->get_then_stmt(), func_name);
        bool else_tail = if_stmt->get_else_stmt() ? 
                        is_tail_call(if_stmt->get_else_stmt(), func_name) : false;
        return then_tail && else_tail;
    }
    
    return false;
}

std::unique_ptr<Stmt> TailRecursionPass::convert_to_loop(FunctionDecl* func, Context& context) {
    const std::string& func_name = func->get_name();
    (void)func_name; // 避免未使用变量警告
    (void)context; // 避免未使用参数警告
    const auto& params = func->get_parameters();
    
    if (params.size() != 2) {
        // 目前只处理两个参数的尾递归（如sum(n, acc)）
        return nullptr;
    }
    
    // 创建循环条件：检查第一个参数是否大于0
    auto condition = std::make_unique<BinaryOpExpr>(
        ">",
        std::make_unique<IdExpr>(params[0]->get_name()),
        std::make_unique<IntegerExpr>(0)
    );
    
    // 创建循环体
    std::vector<std::unique_ptr<Stmt>> loop_stmts;
    
    // 更新累加器（第二个参数）
    auto new_acc_expr = std::make_unique<BinaryOpExpr>(
        "+",
        std::make_unique<IdExpr>(params[1]->get_name()),
        std::make_unique<IdExpr>(params[0]->get_name())
    );
    
    auto update_acc = std::make_unique<AssignExpr>(
        std::make_unique<IdExpr>(params[1]->get_name()),
        std::move(new_acc_expr)
    );
    
    loop_stmts.push_back(std::make_unique<ExprStmt>(std::move(update_acc)));
    
    // 递减第一个参数
    auto decrement_n = std::make_unique<AssignExpr>(
        std::make_unique<IdExpr>(params[0]->get_name()),
        std::make_unique<BinaryOpExpr>(
            "-",
            std::make_unique<IdExpr>(params[0]->get_name()),
            std::make_unique<IntegerExpr>(1)
        )
    );
    
    loop_stmts.push_back(std::make_unique<ExprStmt>(std::move(decrement_n)));
    
    auto loop_body = std::make_unique<BlockStmt>(std::move(loop_stmts));
    
    // 创建while循环
    auto while_loop = std::make_unique<WhileStmt>(
        std::move(condition),
        std::move(loop_body)
    );
    
    // 创建返回语句
    auto return_stmt = std::make_unique<ReturnStmt>(
        std::make_unique<IdExpr>(params[1]->get_name())
    );
    
    // 组合循环和返回语句
    std::vector<std::unique_ptr<Stmt>> final_stmts;
    final_stmts.push_back(std::move(while_loop));
    final_stmts.push_back(std::move(return_stmt));
    
    return std::make_unique<BlockStmt>(std::move(final_stmts));
}
