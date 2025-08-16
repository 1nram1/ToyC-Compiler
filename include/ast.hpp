#ifndef TOYC_AST_HPP
#define TOYC_AST_HPP

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <unordered_set>
#include "context.hpp"

// Forward declarations for context and other classes
class Context;

// Base class for all AST nodes
class Node {
public:
    virtual ~Node() = default;
    // Pure virtual function for generating code
    virtual void generate_code(std::ostream& os, Context& context) const = 0;
    // Virtual function for collecting variable names (default: do nothing)
    virtual void collect_variables(std::unordered_set<std::string>& variables) const { (void)variables; }
    virtual void scan_const_variables(Context& context) const { (void)context; };
    virtual void scan_unused(Context& context) const { (void)context; };
};

// Base class for all expressions
class Expr : public Node {
public:
    // Virtual function to check if expression is constant
    virtual bool is_constant(Context& context) const { (void)context; return false; }
    // Virtual function to evaluate constant expression
    virtual int evaluate_constant(Context& context) const { (void)context; return 0; }
    // New: Virtual function to check if expression is pure (no side effects)
    virtual bool is_pure() const { return true; }
    // New: Virtual function for strength reduction analysis
    virtual bool can_strength_reduce() const { return false; }
    // New: Virtual function to get complexity score for optimization
    virtual int get_complexity() const { return 1; }
    virtual std::string get_expr_type() const { return ""; }
    // New: Virtual function to clone expression
    virtual Expr* clone() const = 0;
    int result_reg = 0;
};

// Base class for all statements
class Stmt : public Node {
public:
    // Virtual function to clone statement
    virtual Stmt* clone() const = 0;
};

// Type alias for a list of statements
using StmtList = std::vector<std::unique_ptr<Stmt>>;

// Represents an integer literal
class IntegerExpr : public Expr {
private:
    int value;
public:
    IntegerExpr(int val, int res_reg = 0) : value(val) {result_reg = res_reg;}
    void generate_code(std::ostream& os, Context& context) const override;
    bool is_constant(Context& context) const override { (void)context; return true; }
    int evaluate_constant(Context& context) const override { (void)context; return value; }
    int get_value() const { return value; }
    std::string get_expr_type() const override { return "Integer"; }
    Expr* clone() const override { return new IntegerExpr(value, result_reg); }
};

// Represents a variable identifier
class IdExpr : public Expr {
private:
    std::string id;
public:
    IdExpr(std::string _id) : id(std::move(_id)) {}
    const std::string& get_id() const { return id; }
    void generate_code(std::ostream& os, Context& context) const override;
    void collect_variables(std::unordered_set<std::string>& variables) const override {
        variables.insert(id);
    }
    std::string get_expr_type() const override { return "Id"; }
    bool is_constant(Context& context) const override {
        return (context.is_const.find(id) == context.is_const.end() ? false : context.is_const.at(id));
    }
    int evaluate_constant(Context& context) const override {
        return (context.const_val.find(id) == context.const_val.end() ? 0 : context.const_val.at(id));
    }
    void scan_unused(Context& context) const override;
    Expr* clone() const override { return new IdExpr(id); }
};

// Represents a binary operation
class BinaryOpExpr : public Expr {
private:
    std::string op;
    std::unique_ptr<Expr> lhs;
    std::unique_ptr<Expr> rhs;
public:
    BinaryOpExpr(std::string _op, std::unique_ptr<Expr> _lhs, std::unique_ptr<Expr> _rhs)
        : op(std::move(_op)), lhs(std::move(_lhs)), rhs(std::move(_rhs)) {}
    void generate_code(std::ostream& os, Context& context) const override;
    void collect_variables(std::unordered_set<std::string>& variables) const override {
        lhs->collect_variables(variables);
        rhs->collect_variables(variables);
    }
    bool is_constant(Context& context) const override {
        return lhs->is_constant(context) && rhs->is_constant(context);
    }
    void scan_const_variables(Context& context) const override;
    void scan_unused(Context& context) const override;
    int evaluate_constant(Context& context) const override {
        if (!is_constant(context)) return 0;
        int l = lhs->evaluate_constant(context);
        int r = rhs->evaluate_constant(context);
        if (op == "+") return l + r;
        if (op == "-") return l - r;
        if (op == "*") return l * r;
        if (op == "/" && r != 0) return l / r;
        if (op == "%" && r != 0) return l % r;
        if (op == "==") return l == r ? 1 : 0;
        if (op == "!=") return l != r ? 1 : 0;
        if (op == "<") return l < r ? 1 : 0;
        if (op == ">") return l > r ? 1 : 0;
        if (op == "<=") return l <= r ? 1 : 0;
        if (op == ">=") return l >= r ? 1 : 0;
        if (op == "&&") return (l && r) ? 1 : 0;
        if (op == "||") return (l || r) ? 1 : 0;
        return 0;
    }
    // Accessor methods for optimizations
    const Expr* get_lhs() const { return lhs.get(); }
    const Expr* get_rhs() const { return rhs.get(); }
    Expr* get_lhs() { return lhs.get(); }
    Expr* get_rhs() { return rhs.get(); }
    const std::string& get_op() const { return op; }
    std::string get_expr_type() const override { return "BinaryOp"; }
    Expr* clone() const override { 
        return new BinaryOpExpr(op, std::unique_ptr<Expr>(lhs->clone()), std::unique_ptr<Expr>(rhs->clone())); 
    }
    // Setter methods for optimizations
    void set_lhs(std::unique_ptr<Expr> new_lhs) { lhs = std::move(new_lhs); }
    void set_rhs(std::unique_ptr<Expr> new_rhs) { rhs = std::move(new_rhs); }
};

// Represents a unary operation
class UnaryOpExpr : public Expr {
private:
    std::string op;
    std::unique_ptr<Expr> expr;
public:
    UnaryOpExpr(std::string _op, std::unique_ptr<Expr> _expr)
        : op(std::move(_op)), expr(std::move(_expr)) {}
    void generate_code(std::ostream& os, Context& context) const override;
    void collect_variables(std::unordered_set<std::string>& variables) const override {
        expr->collect_variables(variables);
    }
    bool is_constant(Context& context) const override {
        return expr->is_constant(context);
    }
    int evaluate_constant(Context& context) const override {
        if (!is_constant(context)) return 0;
        int val = expr->evaluate_constant(context);
        if (op == "-") return -val;
        if (op == "+") return val;
        if (op == "!") return val==0;
        return val;
    }
    // Accessor methods for optimizations
    const Expr* get_expr() const { return expr.get(); }
    Expr* get_expr() { return expr.get(); }
    const std::string& get_op() const { return op; }
    std::string get_expr_type() const override { return "UnaryOp"; }
    Expr* clone() const override { 
        return new UnaryOpExpr(op, std::unique_ptr<Expr>(expr->clone())); 
    }
    void scan_const_variables(Context& context) const override;
    void scan_unused(Context& context) const override;
    // Setter methods for optimizations
    void set_expr(std::unique_ptr<Expr> new_expr) { expr = std::move(new_expr); }
};

// Represents an assignment expression
class AssignExpr : public Expr {
private:
    std::unique_ptr<IdExpr> id;
    std::unique_ptr<Expr> expr;
public:
    AssignExpr(std::unique_ptr<IdExpr> _id, std::unique_ptr<Expr> _expr)
        : id(std::move(_id)), expr(std::move(_expr)) {}
    void generate_code(std::ostream& os, Context& context) const override;
    void collect_variables(std::unordered_set<std::string>& variables) const override {
        id->collect_variables(variables);
        expr->collect_variables(variables);
    }
    void scan_const_variables(Context& context) const override;
    void scan_unused(Context& context) const override;
    // Accessor methods for optimizations
    const IdExpr* get_id() const { return id.get(); }
    const Expr* get_expr() const { return expr.get(); }
    IdExpr* get_id() { return id.get(); }
    Expr* get_expr() { return expr.get(); }
    std::string get_expr_type() const override { return "UnaryOp"; }
    Expr* clone() const override { 
        return new AssignExpr(std::unique_ptr<IdExpr>(new IdExpr(id->get_id())), 
                             std::unique_ptr<Expr>(expr->clone())); 
    }
    // Setter methods for optimizations
    void set_expr(std::unique_ptr<Expr> new_expr) { expr = std::move(new_expr); }
};

// Represents a statement that is just an expression
class ExprStmt : public Stmt {
private:
    std::unique_ptr<Expr> expr;
public:
    ExprStmt(std::unique_ptr<Expr> _expr) : expr(std::move(_expr)) {}
    void generate_code(std::ostream& os, Context& context) const override;
    void collect_variables(std::unordered_set<std::string>& variables) const override {
        expr->collect_variables(variables);
    }
    void scan_const_variables(Context& context) const override;
    void scan_unused(Context& context) const override;
    // Accessor method for optimizations
    const Expr* get_expr() const { return expr.get(); }
    Expr* get_expr() { return expr.get(); }
    // Setter method for optimizations
    void set_expr(std::unique_ptr<Expr> new_expr) { expr = std::move(new_expr); }
    // Clone method
    Stmt* clone() const override {
        return new ExprStmt(std::unique_ptr<Expr>(expr->clone()));
    }
};

// Represents a return statement
class ReturnStmt : public Stmt {
private:
    std::unique_ptr<Expr> expr; // Can be nullptr
public:
    ReturnStmt(std::unique_ptr<Expr> _expr) : expr(std::move(_expr)) {}
    void generate_code(std::ostream& os, Context& context) const override;
    void collect_variables(std::unordered_set<std::string>& variables) const override {
        if (expr) expr->collect_variables(variables);
    }
    void scan_const_variables(Context& context) const override;
    void scan_unused(Context& context) const override;
    // Accessor method for optimizations
    const Expr* get_expr() const { return expr.get(); }
    Expr* get_expr() { return expr.get(); }
    // Setter method for optimizations
    void set_expr(std::unique_ptr<Expr> new_expr) { expr = std::move(new_expr); }
    // Clone method
    Stmt* clone() const override {
        return new ReturnStmt(expr ? std::unique_ptr<Expr>(expr->clone()) : nullptr);
    }
};

// Represents a block of statements
class BlockStmt : public Stmt {
private:
    StmtList stmts;
public:
    BlockStmt(StmtList _stmts) : stmts(std::move(_stmts)) {}
    void generate_code(std::ostream& os, Context& context) const override;
    void collect_variables(std::unordered_set<std::string>& variables) const override {
        for (const auto& stmt : stmts) {
            stmt->collect_variables(variables);
        }
    }
    void scan_const_variables(Context& context) const override;
    void scan_unused(Context& context) const override;
    // Accessor methods for optimizations
    const StmtList& get_stmts() const { return stmts; }
    StmtList& get_stmts() { return stmts; }
    // Setter methods for optimizations
    void add_stmt(std::unique_ptr<Stmt> stmt) { stmts.push_back(std::move(stmt)); }
    void clear_stmts() { stmts.clear(); }
    // Clone method
    Stmt* clone() const override {
        StmtList cloned_stmts;
        for (const auto& stmt : stmts) {
            cloned_stmts.push_back(std::unique_ptr<Stmt>(stmt->clone()));
        }
        return new BlockStmt(std::move(cloned_stmts));
    }
};

// Represents an if-else statement
class IfStmt : public Stmt {
private:
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> then_stmt;
    std::unique_ptr<Stmt> else_stmt; // Can be nullptr
public:
    IfStmt(std::unique_ptr<Expr> _cond, std::unique_ptr<Stmt> _then, std::unique_ptr<Stmt> _else)
        : condition(std::move(_cond)), then_stmt(std::move(_then)), else_stmt(std::move(_else)) {}
    void generate_code(std::ostream& os, Context& context) const override;
    void collect_variables(std::unordered_set<std::string>& variables) const override {
        condition->collect_variables(variables);
        then_stmt->collect_variables(variables);
        if (else_stmt) else_stmt->collect_variables(variables);
    }
    void scan_const_variables(Context& context) const override;
    void scan_unused(Context& context) const override;
    // Accessor methods for optimizations
    const Expr* get_condition() const { return condition.get(); }
    const Stmt* get_then_stmt() const { return then_stmt.get(); }
    const Stmt* get_else_stmt() const { return else_stmt.get(); }
    Expr* get_condition() { return condition.get(); }
    Stmt* get_then_stmt() { return then_stmt.get(); }
    Stmt* get_else_stmt() { return else_stmt.get(); }
    // Setter methods for optimizations
    void set_then_stmt(std::unique_ptr<Stmt> new_then) { then_stmt = std::move(new_then); }
    void set_else_stmt(std::unique_ptr<Stmt> new_else) { else_stmt = std::move(new_else); }
    // Clone method
    Stmt* clone() const override {
        std::unique_ptr<Stmt> cloned_else = else_stmt ? std::unique_ptr<Stmt>(else_stmt->clone()) : nullptr;
        return new IfStmt(
            std::unique_ptr<Expr>(condition->clone()),
            std::unique_ptr<Stmt>(then_stmt->clone()),
            std::move(cloned_else)
        );
    }
};

// Represents a while loop
class WhileStmt : public Stmt {
private:
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> body;
public:
    WhileStmt(std::unique_ptr<Expr> _cond, std::unique_ptr<Stmt> _body)
        : condition(std::move(_cond)), body(std::move(_body)) {}
    void generate_code(std::ostream& os, Context& context) const override;
    void collect_variables(std::unordered_set<std::string>& variables) const override {
        condition->collect_variables(variables);
        body->collect_variables(variables);
    }
    void scan_const_variables(Context& context) const override;
    void scan_unused(Context& context) const override;
    // Accessor methods for optimizations
    const Expr* get_condition() const { return condition.get(); }
    const Stmt* get_body() const { return body.get(); }
    Expr* get_condition() { return condition.get(); }
    Stmt* get_body() { return body.get(); }
    // Setter methods for optimizations
    void set_body(std::unique_ptr<Stmt> new_body) { body = std::move(new_body); }
    // Clone method
    Stmt* clone() const override {
        return new WhileStmt(
            std::unique_ptr<Expr>(condition->clone()),
            std::unique_ptr<Stmt>(body->clone())
        );
    }
};

// Represents a break statement
class BreakStmt : public Stmt {
public:
    void generate_code(std::ostream& os, Context& context) const override;
    Stmt* clone() const override { return new BreakStmt(); }
};

// Represents a continue statement
class ContinueStmt : public Stmt {
public:
    void generate_code(std::ostream& os, Context& context) const override;
    Stmt* clone() const override { return new ContinueStmt(); }
};

// Represents a variable declaration
class DeclStmt : public Stmt {
private:
    std::unique_ptr<IdExpr> id;
    std::unique_ptr<Expr> expr; // Initializer
public:
    DeclStmt(std::unique_ptr<IdExpr> _id, std::unique_ptr<Expr> _expr)
        : id(std::move(_id)), expr(std::move(_expr)) {}
    void generate_code(std::ostream& os, Context& context) const override;
    void collect_variables(std::unordered_set<std::string>& variables) const override {
        id->collect_variables(variables);
        if(expr!=nullptr)expr->collect_variables(variables);
    }
    void scan_const_variables(Context& context) const override;
    void scan_unused(Context& context) const override;
    // Accessor methods for optimizations
    const IdExpr* get_id() const { return id.get(); }
    const Expr* get_expr() const { return expr.get(); }
    IdExpr* get_id() { return id.get(); }
    Expr* get_expr() { return expr.get(); }
    // Clone method
    Stmt* clone() const override {
        return new DeclStmt(
            std::unique_ptr<IdExpr>(new IdExpr(id->get_id())),
            expr ? std::unique_ptr<Expr>(expr->clone()) : nullptr
        );
    }
};

// Represents an empty statement (just a semicolon)
class EmptyStmt : public Stmt {
public:
    void generate_code(std::ostream& os, Context& context) const override;
    Stmt* clone() const override { return new EmptyStmt(); }
};

// Represents a function parameter
class Parameter : public Node {
private:
    std::string type;
    std::string name;
public:
    Parameter(std::string _type, std::string _name) : type(std::move(_type)), name(std::move(_name)) {}
    const std::string& get_type() const { return type; }
    const std::string& get_name() const { return name; }
    void generate_code(std::ostream& os, Context& context) const override;
    void collect_variables(std::unordered_set<std::string>& variables) const override {
        variables.insert(name);
    }
};

// Type alias for parameter list
using ParameterList = std::vector<std::unique_ptr<Parameter>>;

// Represents a function declaration/definition
class FunctionDecl : public Node {
private:
    std::string return_type;
    std::string name;
    ParameterList parameters;
    std::unique_ptr<Stmt> body; // Can be nullptr for declarations
public:
    FunctionDecl(std::string _return_type, std::string _name, ParameterList _params, std::unique_ptr<Stmt> _body)
        : return_type(std::move(_return_type)), name(std::move(_name)), 
          parameters(std::move(_params)), body(std::move(_body)) {}
    
    const std::string& get_return_type() const { return return_type; }
    const std::string& get_name() const { return name; }
    const ParameterList& get_parameters() const { return parameters; }
    const Stmt* get_body() const { return body.get(); }
    Stmt* get_body() { return body.get(); }
    bool has_body() const { return body != nullptr; }
    // Setter methods for optimizations
    void set_body(std::unique_ptr<Stmt> new_body) { body = std::move(new_body); }
    
    void generate_code(std::ostream& os, Context& context) const override;
    void collect_variables(std::unordered_set<std::string>& variables) const override {
        for (const auto& param : parameters) {
            param->collect_variables(variables);
        }
        if (body) body->collect_variables(variables);
    }
    void scan_const_variables(Context& context) const override;
    void scan_unused(Context& context) const override;
};

// Type alias for function declaration list
using FunctionDeclList = std::vector<std::unique_ptr<FunctionDecl>>;

// Represents a complete program with possible function declarations
class ProgramWithFunctions : public Node {
private:
    FunctionDeclList functions;
public:
    ProgramWithFunctions(FunctionDeclList _functions)
        : functions(std::move(_functions)) {}
    
    const FunctionDeclList& get_functions() const { return functions; }
    FunctionDeclList& get_functions() { return functions; }
    bool has_functions() const { return !functions.empty(); }
    void scan_const_variables(Context& context) const override;
    void scan_unused(Context& context) const override;
    
    void generate_code(std::ostream& os, Context& context) const override;
    void collect_variables(std::unordered_set<std::string>& variables) const override;
};

// Type alias for argument list
using ArgumentList = std::vector<std::unique_ptr<Expr>>;

// Represents a function call expression
class FunctionCallExpr : public Expr {
private:
    std::string function_name;
    ArgumentList arguments;
public:
    FunctionCallExpr(std::string _name, ArgumentList _args)
        : function_name(std::move(_name)), arguments(std::move(_args)) {}
    
    const std::string& get_function_name() const { return function_name; }
    const ArgumentList& get_arguments() const { return arguments; }
    
    void generate_code(std::ostream& os, Context& context) const override;
    void collect_variables(std::unordered_set<std::string>& variables) const override {
        for (const auto& arg : arguments) {
            arg->collect_variables(variables);
        }
    }
    void scan_const_variables(Context& context) const override;
    void scan_unused(Context& context) const override;
    
    // Function calls are not pure (they may have side effects)
    bool is_pure() const override { return false; }
    std::string get_expr_type() const override { return "FunctionCall"; }
    Expr* clone() const override { 
        ArgumentList cloned_args;
        for (const auto& arg : arguments) {
            cloned_args.push_back(std::unique_ptr<Expr>(arg->clone()));
        }
        return new FunctionCallExpr(function_name, std::move(cloned_args)); 
    }
};

#endif // TOYC_AST_HPP
