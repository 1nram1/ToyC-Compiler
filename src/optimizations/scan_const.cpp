#include <iostream>
#include "context.hpp"
#include "ast.hpp"

void BinaryOpExpr::scan_const_variables(Context& context) const {
    lhs->scan_const_variables(context);
    rhs->scan_const_variables(context);
}

void UnaryOpExpr::scan_const_variables(Context& context) const {
    expr->scan_const_variables(context);
}

void AssignExpr::scan_const_variables(Context& context) const {
    auto id = this->get_id()->get_id();
    // std::cout<< "# scan const func called from assign "<< id << std::endl;
    context.is_const[id] = false;
}

void ExprStmt::scan_const_variables(Context& context) const {
    expr->scan_const_variables(context);
}
void ReturnStmt::scan_const_variables(Context& context) const {
    expr->scan_const_variables(context);
}
void BlockStmt::scan_const_variables(Context& context) const {
    // std::cout<< "# scan const func called from block" << std::endl;
    for (const auto& stmt : stmts) {
        stmt->scan_const_variables(context);
    }
}
void IfStmt::scan_const_variables(Context& context) const {
    condition->scan_const_variables(context);
    then_stmt->scan_const_variables(context);
    if(else_stmt) else_stmt->scan_const_variables(context);
}
void WhileStmt::scan_const_variables(Context& context) const {
    body->scan_const_variables(context);
    condition->scan_const_variables(context);
}
void DeclStmt::scan_const_variables(Context& context) const {
    auto id = this->id->get_id();
    if(context.variables.find(id) != context.variables.end()){
        // already declared, scope shadowing
        // std::cout<< "# scan const func called from shadow decl "<< id << std::endl;
        // if(expr)context.is_const[id] = expr->is_constant(context);
        // else context.is_const[id] = false;
        std::cout<< "# "<< id << (context.is_const[id]?" is ":" is not ") << "constant"<< std::endl;
    }
    else {
        // std::cout<< "# scan const func called from decl "<< id << std::endl;
        if(expr)context.is_const[id] = expr->is_constant(context);
        else context.is_const[id] = false;
        // std::cout<< "# "<< id << (context.is_const[id]?" is ":" is not ") << "constant"<< std::endl;
    }
}
void FunctionDecl::scan_const_variables(Context& context) const {
    // std::cout<< "# scan const func called from "<< name << std::endl;
    if(body)body -> scan_const_variables(context);
}
void ProgramWithFunctions::scan_const_variables(Context& context) const {
    for (const auto& func : functions) {
        func->scan_const_variables(context);
    }
}
void FunctionCallExpr::scan_const_variables(Context& context) const {
    
}