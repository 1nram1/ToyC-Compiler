#include <iostream>
#include "context.hpp"
#include "ast.hpp"

void IdExpr::scan_unused(Context& context) const {
    context.add_used_variable(id);
}
void BinaryOpExpr::scan_unused(Context& context) const {
    lhs->scan_unused(context);
    rhs->scan_unused(context);
}

void UnaryOpExpr::scan_unused(Context& context) const {
    expr->scan_unused(context);
}

void AssignExpr::scan_unused(Context& context) const {
    auto id = this->get_id()->get_id();
    // std::cout<< "# scan unused func called from assign "<< id << std::endl;
    context.add_used_variable(id);
    // if(context.is_const.find(id) != context.is_const.end() && context.is_const.at(id)){
    //     // is const which means expr is not used
    //     // std::cout<< "# "<< id <<"'s expr is unused"<< std::endl;
    // }
    // else {
        expr->scan_unused(context);
    // }
}

void ExprStmt::scan_unused(Context& context) const {
    expr->scan_unused(context);
}
void ReturnStmt::scan_unused(Context& context) const {
    expr->scan_unused(context);
}
void BlockStmt::scan_unused(Context& context) const {
    // std::cout<< "# scan unused func called from block" << std::endl;
    for (const auto& stmt : stmts) {
        stmt->scan_unused(context);
    }
}
void IfStmt::scan_unused(Context& context) const {
    condition->scan_unused(context);
    then_stmt->scan_unused(context);
    if(else_stmt) else_stmt->scan_unused(context);
}
void WhileStmt::scan_unused(Context& context) const {
    body->scan_unused(context);
    condition->scan_unused(context);
}
void DeclStmt::scan_unused(Context& context) const {
    // std::cout<< "# scan unused func called from decl "<< id->get_id() << std::endl;
    if(expr){
        if(context.is_const.find(id->get_id()) != context.is_const.end() && context.is_const.at(id->get_id())){
            // is const which means expr is not used
            // std::cout<< "# "<< id <<"'s expr is unused"<< std::endl;
        }
        else {
            expr->scan_unused(context);
        }
    }
}
void FunctionDecl::scan_unused(Context& context) const {
    // std::cout<< "# scan const func called from "<< name << std::endl;
    if(body)body -> scan_unused(context);
}
void ProgramWithFunctions::scan_unused(Context& context) const {
    for (const auto& func : functions) {
        func->scan_unused(context);
    }
}
void FunctionCallExpr::scan_unused(Context& context) const {
    for (const auto& arg : arguments){
        arg->scan_unused(context);
    }
}