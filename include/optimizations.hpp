#ifndef TOYC_OPTIMIZATIONS_HPP
#define TOYC_OPTIMIZATIONS_HPP

#include <iostream>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "ast.hpp"
#include "context.hpp"

// Forward declarations
class Node;
class Expr;
class Stmt;
class BinaryOpExpr;
class FunctionDecl;

// Base class for all optimization passes
class OptimizationPass {
public:
    virtual ~OptimizationPass() = default;
    virtual void run(ProgramWithFunctions& program, Context& context) = 0;
    virtual std::string get_name() const = 0;
};

// Optimization manager
class OptimizationManager {
private:
    std::vector<std::unique_ptr<OptimizationPass>> passes;
    Context& context;

public:
    explicit OptimizationManager(Context& ctx) : context(ctx) {}
    
    void add_pass(std::unique_ptr<OptimizationPass> pass) {
        passes.push_back(std::move(pass));
    }
    
    void run_all(ProgramWithFunctions& program) {
        for (const auto& pass : passes) {
            std::cout << "# Running optimization: " << pass->get_name() << std::endl;
            pass->run(program, context);
        }
    }
};

// Strength reduction optimization
class StrengthReductionPass : public OptimizationPass {
public:
    void run(ProgramWithFunctions& program, Context& context) override;
    std::string get_name() const override { return "Strength Reduction"; }
    
private:
    void optimize_function(FunctionDecl* func, Context& context);
    void optimize_stmt(Stmt* stmt, Context& context);
    std::unique_ptr<Expr> optimize_expr(Expr* expr, Context& context);
    std::unique_ptr<Expr> optimize_binary_op(BinaryOpExpr* expr, Context& context);
    bool is_power_of_two(int value) const;
    int get_power_of_two(int value) const;
};

// Tail recursion optimization
class TailRecursionPass : public OptimizationPass {
public:
    void run(ProgramWithFunctions& program, Context& context) override;
    std::string get_name() const override { return "Tail Recursion"; }
    
private:
    bool is_tail_recursive(FunctionDecl* func) const;
    std::unique_ptr<Stmt> convert_to_loop(FunctionDecl* func, Context& context);
    bool is_tail_call(Stmt* stmt, const std::string& func_name) const;
};

// Common subexpression elimination
class CommonSubexprPass : public OptimizationPass {
public:
    void run(ProgramWithFunctions& program, Context& context) override;
    std::string get_name() const override { return "Common Subexpression Elimination"; }
    
private:
    struct ExprInfo {
        std::string hash;
        std::unique_ptr<Expr> expr;
        std::string temp_var;
        bool used = false;
    };
    
    void optimize_function(FunctionDecl* func, Context& context);
    void optimize_stmt(Stmt* stmt, Context& context);
    std::unique_ptr<Expr> optimize_expr(Expr* expr, Context& context);
    bool should_cache_expr(Expr* expr) const;
    
    std::unordered_map<std::string, ExprInfo> expr_cache;
    std::string generate_expr_hash(Expr* expr) const;
    std::unique_ptr<Expr> replace_with_temp(const std::string& temp_var) const;
    std::string generate_temp_var() const;
    static int temp_counter;
};

// Loop invariant code motion
class LoopInvariantPass : public OptimizationPass {
public:
    void run(ProgramWithFunctions& program, Context& context) override;
    std::string get_name() const override { return "Loop Invariant Code Motion"; }
    
private:
    struct LoopInfo {
        WhileStmt* loop;
        std::unordered_set<std::string> loop_vars;
        std::vector<std::unique_ptr<Stmt>> invariant_stmts;
        
        // 添加移动构造函数和移动赋值运算符
        LoopInfo() = default;
        LoopInfo(LoopInfo&&) = default;
        LoopInfo& operator=(LoopInfo&&) = default;
        
        // 删除复制构造函数和复制赋值运算符
        LoopInfo(const LoopInfo&) = delete;
        LoopInfo& operator=(const LoopInfo&) = delete;
    };
    
    void optimize_function(FunctionDecl* func, Context& context);
    void optimize_loop(const LoopInfo& loop_info, Context& context);
    std::unique_ptr<Stmt> create_optimized_loop_body(WhileStmt* loop, 
                                                   const std::vector<std::unique_ptr<Stmt>>& invariant_stmts);
    std::vector<LoopInfo> find_loops(Stmt* stmt) const;
    bool is_invariant(Expr* expr, const std::unordered_set<std::string>& loop_vars) const;
    bool is_invariant(Expr* expr, WhileStmt* loop) const;
    bool is_invariant_stmt(Stmt* stmt, WhileStmt* loop) const;
    std::vector<std::unique_ptr<Stmt>> extract_invariants(WhileStmt* loop, Context& context);
    std::unordered_set<std::string> collect_loop_variables(WhileStmt* loop) const;
    void collect_assigned_vars(Stmt* stmt, std::unordered_set<std::string>& vars) const;
};

// Initialize static counter
// int CommonSubexprPass::temp_counter = 0;

#endif // TOYC_OPTIMIZATIONS_HPP
