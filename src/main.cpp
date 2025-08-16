#include <iostream>
#include <fstream>
#include <string>
#include <cstdio>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include "context.hpp"
#include "ast.hpp"
#include "optimizations.hpp"
#include "optimizations/scan_const.cpp"
#include "optimizations/scan_unused.cpp"
#include "optimizations/strength_reduction.cpp"
#include "optimizations/common_subexpr.cpp"
#include "optimizations/loop_invariant.cpp"
#include "optimizations/tail_recursion.cpp"
#include "optimizations/algebraic.cpp"

// External variables from parser
extern std::unique_ptr<ProgramWithFunctions> parsed_program;
extern FILE* yyin;
extern int yyparse();

// Implementation of the generate_code methods for each AST node

void IntegerExpr::generate_code(std::ostream& os, Context& context) const {
    // 高级优化：更精确的指令选择
    if (value == 0) {
        os << "  mv t"<<result_reg<<", zero" << std::endl; // 使用mv指令，更语义化
    } else if (value >= -2048 && value <= 2047) {
        os << "  addi t"<<result_reg<<", zero, " << value << std::endl;
    } else if ((value & 0xFFF) == 0) {
        // 如果是4K对齐的值，使用lui指令
        os << "  lui t"<<result_reg<<", " << (value >> 12) << std::endl;
    } else {
        os << "  li t"<<result_reg<<", " << value << std::endl;
    }
    context.mark_register_free(result_reg);
    context.mark_register_used(result_reg, T_VAL_INTEGER);
}

void IdExpr::generate_code(std::ostream& os, Context& context) const {
    if(context.optimize_variable_cache){
        int var_reg = context.is_var_in_reg(id);
        if(var_reg != -1){
            os << "# "<<id<<" already in t" << var_reg << std::endl;
            if(result_reg != var_reg){
                os << "  mv t"<<result_reg<<", t" << var_reg << std::endl;
            }
            else {
                os << "# mv t"<<result_reg<<", t" << var_reg << std::endl;
            }
            context.mark_register_free(result_reg);
            context.mark_register_used(result_reg, id);
            return;
        }
        // 常量优化， 如果数值不变则直接加载为整数
        if(context.is_const[id]){
            // os << "# TODO: calculate const val" << id << std::endl;
            int value = context.const_val.at(id);
            if (value == 0) {
                os << "  mv t"<<result_reg<<", zero" << std::endl; // 使用mv指令，更语义化
            } else if (value >= -2048 && value <= 2047) {
                os << "  addi t"<<result_reg<<", zero, " << value << std::endl;
            } else if ((value & 0xFFF) == 0) {
                // 如果是4K对齐的值，使用lui指令
                os << "  lui t"<<result_reg<<", " << (value >> 12) << std::endl;
            } else {
                os << "  li t"<<result_reg<<", " << value << std::endl;
            }
            context.mark_register_free(result_reg);
            context.mark_register_used(result_reg, id);
        }
        else {
            int offset = context.find_variable_offset(id);
            int cached_reg = context.get_next_temp_register_id();
            
            context.mark_register_free(cached_reg);
            context.mark_register_used(cached_reg, id);

            os << "  # " << id << " saved to t"<<cached_reg << std::endl;
            os << "  lw t"<<cached_reg<<", " << offset << "(s0)" << std::endl;
            os << "  mv t"<<result_reg<<", t" << cached_reg << std::endl;
        }
    }
    else{
        // 常量优化， 如果数值不变则直接加载为整数
        if(context.is_const[id]){
            // os << "# TODO: calculate const val" << id << std::endl;
            int value = context.const_val.at(id);
            if (value == 0) {
                os << "  mv t"<<result_reg<<", zero" << std::endl; // 使用mv指令，更语义化
            } else if (value >= -2048 && value <= 2047) {
                os << "  addi t"<<result_reg<<", zero, " << value << std::endl;
            } else if ((value & 0xFFF) == 0) {
                // 如果是4K对齐的值，使用lui指令
                os << "  lui t"<<result_reg<<", " << (value >> 12) << std::endl;
            } else {
                os << "  li t"<<result_reg<<", " << value << std::endl;
            }
            // context.mark_register_free(result_reg);
            // context.mark_register_used(result_reg, id);
        }
        else {
            int offset = context.find_variable_offset(id);
            // int cached_reg = context.get_next_temp_register_id();
            
            // context.mark_register_free(cached_reg);
            // context.mark_register_used(cached_reg, id);

            // os << "  # " << id << " saved to t"<<cached_reg << std::endl;
            os << "  lw t"<<result_reg<<", " << offset << "(s0)" << std::endl;
            // os << "  mv t"<<result_reg<<", t" << cached_reg << std::endl;
        }
    }
    
}

void BinaryOpExpr::generate_code(std::ostream& os, Context& context) const {
    int result_reg = 0;

    // 高级优化：常量传播和表达式优化
    if (is_constant(context)) {
        int result = evaluate_constant(context);
        // 使用IntegerExpr的优化指令选择
        IntegerExpr(result, result_reg).generate_code(os, context);
        context.mark_register_free(result_reg);
        context.mark_register_used(result_reg, T_VAL_BOP_RES);
        return;
    }

    int l_result_reg = 0;
    lhs->result_reg = l_result_reg;
    lhs->generate_code(os, context);

    int r_result_reg = 1;
    rhs->result_reg = r_result_reg;

    std::string false_label, true_label, end_label;

    if (op == "&&") {
        // Short-circuit evaluation for logical AND
        false_label = context.label_factory.create();
        end_label = context.label_factory.create();
        os << "  snez t"<<result_reg<<", t"<<l_result_reg<<"" << std::endl;
        os << "  beqz t"<<result_reg<<", " << false_label << std::endl;
        
    } else if (op == "||") {
        // Short-circuit evaluation for logical OR
        true_label = context.label_factory.create();
        end_label = context.label_factory.create();
        os << "  snez t"<<result_reg<<", t"<<l_result_reg<<"" << std::endl;
        os << "  bnez t"<<result_reg<<", " << true_label << std::endl;
    }
    
    // 高级优化：智能寄存器分配和栈避免
    bool avoid_stack = false;
    bool rhs_is_simple = false;
    
    // 检查右操作数是否简单
    if (auto id_rhs = dynamic_cast<const IdExpr*>(rhs.get())) {
        if (context.is_variable_declared(id_rhs->get_id())) {
            // 检查变量是否已在寄存器中
            int cached_reg = context.is_var_in_reg(id_rhs->get_id());
            if (cached_reg != -1) {
                os << "  mv t" << r_result_reg << ", t" << cached_reg << std::endl;
            } else {
                int offset = context.find_variable_offset(id_rhs->get_id());
                os << "  lw t" << r_result_reg << ", " << offset << "(s0)" << std::endl;
            }
            avoid_stack = true;
            rhs_is_simple = true;
        }
    } else if (auto int_rhs = dynamic_cast<const IntegerExpr*>(rhs.get())) {
        int value = int_rhs->get_value();
        if (value == 0) {
            os << "  mv t" << r_result_reg << ", zero" << std::endl;
        } else if (value >= -2048 && value <= 2047) {
            os << "  addi t" << r_result_reg << ", zero, " << value << std::endl;
        } else if ((value & 0xFFF) == 0) {
            os << "  lui t" << r_result_reg << ", " << (value >> 12) << std::endl;
        } else {
            os << "  li t" << r_result_reg << ", " << value << std::endl;
        }
        avoid_stack = true;
        rhs_is_simple = true;
    }
    
    if (!avoid_stack) {
        // 使用栈的传统方法，但优化栈操作
        os << "  addi sp, sp, -4" << std::endl;
        os << "  sw t"<<l_result_reg<<", 0(sp)" << std::endl;
        rhs->generate_code(os, context);
        os << "  mv t" << r_result_reg << ", t0" << std::endl;
        os << "  lw t" << l_result_reg <<", 0(sp)" << std::endl;
        os << "  addi sp, sp, 4" << std::endl;
    }

    // 高级优化：智能运算指令生成
    if (op == "+") {
        // 优化：如果右操作数是0，直接使用左操作数
        if (rhs_is_simple) {
            auto int_rhs = dynamic_cast<const IntegerExpr*>(rhs.get());
            if (int_rhs && int_rhs->get_value() == 0) {
                os << "  mv t"<<result_reg<<", t" << l_result_reg << std::endl;
            } else {
                os << "  add t"<<result_reg<<", t" << l_result_reg << ", t"<< r_result_reg << std::endl;
            }
        } else {
            os << "  add t"<<result_reg<<", t" << l_result_reg << ", t"<< r_result_reg << std::endl;
        }
    } else if (op == "-") {
        // 优化：如果右操作数是0，直接使用左操作数
        if (rhs_is_simple) {
            auto int_rhs = dynamic_cast<const IntegerExpr*>(rhs.get());
            if (int_rhs && int_rhs->get_value() == 0) {
                os << "  mv t"<<result_reg<<", t" << l_result_reg << std::endl;
            } else {
                os << "  sub t"<<result_reg<<", t" << l_result_reg << ", t"<< r_result_reg << std::endl;
            }
        } else {
            os << "  sub t"<<result_reg<<", t" << l_result_reg << ", t"<< r_result_reg << std::endl;
        }
    } else if (op == "*") {
        // 优化：特殊乘法优化
        if (rhs_is_simple) {
            auto int_rhs = dynamic_cast<const IntegerExpr*>(rhs.get());
            if (int_rhs) {
                int value = int_rhs->get_value();
                if (value == 0) {
                    os << "  mv t"<<result_reg<<", zero" << std::endl;
                } else if (value == 1) {
                    os << "  mv t"<<result_reg<<", t" << l_result_reg << std::endl;
                } else if (value == 2) {
                    os << "  slli t"<<result_reg<<", t" << l_result_reg << ", 1" << std::endl;
                } else if (value == 4) {
                    os << "  slli t"<<result_reg<<", t" << l_result_reg << ", 2" << std::endl;
                } else if (value == 8) {
                    os << "  slli t"<<result_reg<<", t" << l_result_reg << ", 3" << std::endl;
                } else if (value == 16) {
                    os << "  slli t"<<result_reg<<", t" << l_result_reg << ", 4" << std::endl;
                } else if (value == 32) {
                    os << "  slli t"<<result_reg<<", t" << l_result_reg << ", 5" << std::endl;
                } else {
                    os << "  mul t"<<result_reg<<", t" << l_result_reg << ", t"<< r_result_reg << std::endl;
                }
            } else {
                os << "  mul t"<<result_reg<<", t" << l_result_reg << ", t"<< r_result_reg << std::endl;
            }
        } else {
            os << "  mul t"<<result_reg<<", t" << l_result_reg << ", t"<< r_result_reg << std::endl;
        }
    } else if (op == "/") {
        // 优化：特殊除法优化
        if (rhs_is_simple) {
            auto int_rhs = dynamic_cast<const IntegerExpr*>(rhs.get());
            if (int_rhs) {
                int value = int_rhs->get_value();
                if (value == 1) {
                    os << "  mv t"<<result_reg<<", t" << l_result_reg << std::endl;
                } else if (value == 2) {
                    os << "  srai t"<<result_reg<<", t" << l_result_reg << ", 1" << std::endl;
                } else if (value == 4) {
                    os << "  srai t"<<result_reg<<", t" << l_result_reg << ", 2" << std::endl;
                } else if (value == 8) {
                    os << "  srai t"<<result_reg<<", t" << l_result_reg << ", 3" << std::endl;
                } else if (value == 16) {
                    os << "  srai t"<<result_reg<<", t" << l_result_reg << ", 4" << std::endl;
                } else if (value == 32) {
                    os << "  srai t"<<result_reg<<", t" << l_result_reg << ", 5" << std::endl;
                } else {
                    // 增强的除零检查
                    std::string not_div_err_label = context.label_factory.create();
                    os << "  bnez t"<< r_result_reg <<", " << not_div_err_label << std::endl;
                    os << "  j .L_div_error" << std::endl;
                    os << not_div_err_label << ":" << std::endl;
                    os << "  div t"<<result_reg<<", t" << l_result_reg << ", t"<< r_result_reg << std::endl;
                }
            } else {
                // 增强的除零检查
                std::string not_div_err_label = context.label_factory.create();
                os << "  bnez t"<< r_result_reg <<", " << not_div_err_label << std::endl;
                os << "  j .L_div_error" << std::endl;
                os << not_div_err_label << ":" << std::endl;
                os << "  div t"<<result_reg<<", t" << l_result_reg << ", t"<< r_result_reg << std::endl;
            }
        } else {
            // 增强的除零检查
            std::string not_div_err_label = context.label_factory.create();
            os << "  bnez t"<< r_result_reg <<", " << not_div_err_label << std::endl;
            os << "  j .L_div_error" << std::endl;
            os << not_div_err_label << ":" << std::endl;
            os << "  div t"<<result_reg<<", t" << l_result_reg << ", t"<< r_result_reg << std::endl;
        }
    } else if (op == "%") {
        // 优化：特殊取模优化
        if (rhs_is_simple) {
            auto int_rhs = dynamic_cast<const IntegerExpr*>(rhs.get());
            if (int_rhs) {
                int value = int_rhs->get_value();
                if (value == 1) {
                    os << "  mv t"<<result_reg<<", zero" << std::endl;
                } else if (value == 2) {
                    os << "  andi t"<<result_reg<<", t" << l_result_reg << ", 1" << std::endl;
                } else if (value == 4) {
                    os << "  andi t"<<result_reg<<", t" << l_result_reg << ", 3" << std::endl;
                } else if (value == 8) {
                    os << "  andi t"<<result_reg<<", t" << l_result_reg << ", 7" << std::endl;
                } else if (value == 16) {
                    os << "  andi t"<<result_reg<<", t" << l_result_reg << ", 15" << std::endl;
                } else if (value == 32) {
                    os << "  andi t"<<result_reg<<", t" << l_result_reg << ", 31" << std::endl;
                } else {
                    // 增强的除零检查
                    std::string not_div_err_label = context.label_factory.create();
                    os << "  bnez t"<< r_result_reg <<", "<< not_div_err_label << std::endl;
                    os << "  j .L_div_error" << std::endl;
                    os << not_div_err_label << ":" << std::endl;
                    os << "  rem t"<<result_reg<<", t" << l_result_reg << ", t" << r_result_reg << std::endl;
                }
            } else {
                // 增强的除零检查
                std::string not_div_err_label = context.label_factory.create();
                os << "  bnez t"<< r_result_reg <<", "<< not_div_err_label << std::endl;
                os << "  j .L_div_error" << std::endl;
                os << not_div_err_label << ":" << std::endl;
                os << "  rem t"<<result_reg<<", t" << l_result_reg << ", t" << r_result_reg << std::endl;
            }
        } else {
            // 增强的除零检查
            std::string not_div_err_label = context.label_factory.create();
            os << "  bnez t"<< r_result_reg <<", "<< not_div_err_label << std::endl;
            os << "  j .L_div_error" << std::endl;
            os << not_div_err_label << ":" << std::endl;
            os << "  rem t"<<result_reg<<", t" << l_result_reg << ", t" << r_result_reg << std::endl;
        }
    } else if (op == "==") {
        os << "  sub t"<<result_reg<<", t" << l_result_reg << ", t"<< r_result_reg << std::endl;
        os << "  seqz t"<<result_reg<<", t"<<result_reg<<"" << std::endl;
    } else if (op == "!=") {
        os << "  sub t"<<result_reg<<", t" << l_result_reg << ", t"<< r_result_reg << std::endl;
        os << "  snez t"<<result_reg<<", t"<< result_reg <<"" << std::endl;
    } else if (op == ">") {
        os << "  slt t"<<result_reg<<", t" << r_result_reg << ", t" << l_result_reg << std::endl;
    } else if (op == "<") {
        os << "  slt t"<<result_reg<<", t" << l_result_reg << ", t" << r_result_reg << std::endl;
    } else if (op == "<=") {
        os << "  slt t"<<result_reg<<", t" << r_result_reg << ", t" << l_result_reg << std::endl;
        os << "  xori t"<<result_reg<<", t"<<result_reg<<", 1" << std::endl;
    } else if (op == ">=") {
        os << "  slt t"<<result_reg<<", t" << l_result_reg << ", t" << r_result_reg << std::endl;
        os << "  xori t"<<result_reg<<", t"<<result_reg<<", 1" << std::endl;
    } else if (op == "&&") {
        // Short-circuit evaluation for logical AND
        os << "  snez t"<<result_reg<<", t" << r_result_reg << std::endl;
        os << "  beqz t"<<result_reg<<", " << false_label << std::endl;
        os << "  li t"<<result_reg<<", 1" << std::endl;
        os << "  j " << end_label << std::endl;
        os << false_label << ":" << std::endl;
        os << "  li t"<<result_reg<<", 0" << std::endl;
        os << end_label << ":" << std::endl;
    } else if (op == "||") {
        // Short-circuit evaluation for logical OR
        os << "  snez t"<<result_reg<<", t" << r_result_reg << std::endl;
        os << "  bnez t"<<result_reg<<", " << true_label << std::endl;
        os << "  li t"<<result_reg<<", 0" << std::endl;
        os << "  j " << end_label << std::endl;
        os << true_label << ":" << std::endl;
        os << "  li t"<<result_reg<<", 1" << std::endl;
        os << end_label << ":" << std::endl;
    }
    
    context.mark_register_free(result_reg);
    context.mark_register_used(result_reg, T_VAL_BOP_RES);
}

void UnaryOpExpr::generate_code(std::ostream& os, Context& context) const {
    // 优化：常量一元表达式
    if (expr->is_constant(context) && op == "-") {
        int value = -expr->evaluate_constant(context);
        IntegerExpr(value, result_reg).generate_code(os, context);
        context.mark_register_free(result_reg);
        context.mark_register_used(result_reg, T_VAL_UNARY_RES);
        return;
    }
    if (expr->is_constant(context) && op == "+") {
        int value = expr->evaluate_constant(context);
        IntegerExpr(value, result_reg).generate_code(os, context);
        context.mark_register_free(result_reg);
        context.mark_register_used(result_reg, T_VAL_UNARY_RES);
        return;
    }
    if (expr->is_constant(context) && op == "!") {
        int value = expr->evaluate_constant(context) == 0;
        IntegerExpr(value, result_reg).generate_code(os, context);
        context.mark_register_free(result_reg);
        context.mark_register_used(result_reg, T_VAL_UNARY_RES);
        return;
    }

    expr->result_reg = result_reg;
    expr->generate_code(os, context);
    if (op == "-") {
        os << "  neg t"<<result_reg<<", t"<<result_reg<<"" << std::endl;
    }
    if (op == "+") {
        // do nothing
    }
    if (op == "!") {
        os << "  seqz t"<<result_reg<<", t"<<result_reg<<"" << std::endl;
    }
    context.mark_register_free(result_reg);
    context.mark_register_used(result_reg, T_VAL_UNARY_RES);
}

void AssignExpr::generate_code(std::ostream& os, Context& context) const {
    expr->result_reg = result_reg;

    expr->generate_code(os, context);
    const std::string var_id = id->get_id();
    int offset = context.find_variable_offset(var_id);

    if(context.optimize_variable_cache){
        int var_reg = context.is_var_in_reg(var_id);
        if(var_reg != -1){
            // 更新寄存器缓存
            os << "  mv t"<<var_reg<<", t" << result_reg << std::endl;
        }
        os << "  sw t"<<result_reg<<", " << offset << "(s0)" << std::endl;
        context.mark_register_used(result_reg, var_id);
    }
    else{
        os << "  sw t"<<result_reg<<", " << offset << "(s0)" << std::endl;
    }
    
    // Assignment expression returns the assigned value (already in t0)
}

void ExprStmt::generate_code(std::ostream& os, Context& context) const {
    expr->generate_code(os, context);
}

void ReturnStmt::generate_code(std::ostream& os, Context& context) const {
    int result_reg = 0;
    if (expr) {
        expr->result_reg = result_reg;
        expr->generate_code(os, context);
        os << "  mv a0, t"<<result_reg << std::endl;
    } else {
        os << "  mv a0, zero" << std::endl; // 使用mv指令替代li
    }
    os << "  j .L_return_" << context.current_function << std::endl;
}

void BlockStmt::generate_code(std::ostream& os, Context& context) const {
    // 高级优化：作用域管理
    auto saved_offset = context.stack_offset;
    context.push_scope(); // Enter new scope

    // 检测是否有隐含的常量
    scan_const_variables(context);
    // 检测是否有未使用的变量
    scan_unused(context);
    
    for (const auto& s : stmts) {
        s->generate_code(os, context);
    }
    
    context.pop_scope(); // Exit scope
    // 恢复栈偏移（简单的作用域管理）
    context.stack_offset = saved_offset;
}

void IfStmt::generate_code(std::ostream& os, Context& context) const {
    // 高级优化：条件优化
    if (condition->is_constant(context)) {
        int cond_val = condition->evaluate_constant(context);
        if (cond_val != 0) {
            // 条件总是真
            then_stmt->generate_code(os, context);
        } else {
            // 条件总是假
            if (else_stmt) {
                else_stmt->generate_code(os, context);
            }
        }
        return;
    }

    std::string else_label = context.label_factory.create();
    std::string end_label = context.label_factory.create();

    int result_reg = 0;
    condition->result_reg = result_reg;
    condition->generate_code(os, context);
    os << "  beqz t"<<result_reg<<", " << else_label << std::endl;
    auto then_context = context;
    auto else_context = context;
    then_stmt->generate_code(os, then_context);
    
    if (else_stmt) {
        os << "  j " << end_label << std::endl;
        os << else_label << ":" << std::endl;
        else_stmt->generate_code(os, else_context);
        os << end_label << ":" << std::endl;
    } else {
        os << else_label << ":" << std::endl;
    }
}

void WhileStmt::generate_code(std::ostream& os, Context& context) const {
    // 高级优化：循环优化
    if (condition->is_constant(context)) {
        int cond_val = condition->evaluate_constant(context);
        if (cond_val == 0) {
            // 死循环检测：条件总是假，不生成循环代码
            os << "  # Dead loop eliminated (condition always false)" << std::endl;
            return;
        }
    }

    std::string start_label = context.label_factory.create();
    std::string end_label = context.label_factory.create();
    context.push_loop_labels(start_label, end_label);

    // 优化：在循环开始时启用变量缓存
    context.optimize_variable_cache = true;
    
    int result_reg = 0;
    condition->result_reg = result_reg;
    os << start_label << ":" << std::endl;
    condition->generate_code(os, context);
    os << "  beqz t"<<result_reg<<", " << end_label << std::endl;
    body->generate_code(os, context);
    os << "  j " << start_label << std::endl;
    os << end_label << ":" << std::endl;
    
    // 循环结束后清理缓存
    context.clear_cache();
    context.optimize_variable_cache = false;

    context.pop_loop_labels();
}

void BreakStmt::generate_code(std::ostream& os, Context& context) const {
    if (context.loop_labels.empty()) {
        std::cerr << "Error: 'break' statement not in loop" << std::endl;
        exit(1);
    }
    os << "  j " << context.loop_labels.top().second << std::endl;
}

void ContinueStmt::generate_code(std::ostream& os, Context& context) const {
    if (context.loop_labels.empty()) {
        std::cerr << "Error: 'continue' statement not in loop" << std::endl;
        exit(1);
    }
    os << "  j " << context.loop_labels.top().first << std::endl;
}

void DeclStmt::generate_code(std::ostream& os, Context& context) const {
    const std::string var_id = id->get_id();
    // Only check for redefinition in current scope, allow variable shadowing
    if (context.is_variable_in_current_scope(var_id)) {
        std::cerr << "Error: Redefinition of variable '" << var_id << "' in same scope" << std::endl;
        exit(1);
    }

    bool unused = false;
    if (!context.is_variable_used(var_id)){
        // 未使用的变量，不生成decl
        // os << "# TODO: unused var " << var_id << ", do not generate decl" << std::endl;
        // return;
        unused = true;
    }

    int offset = context.stack_offset;
    if(!unused){
        context.add_variable_to_current_scope(var_id, offset);
        context.stack_offset -= 4; // Allocate 4 bytes for the int
    }

    if(expr != nullptr){
        int result_reg = 0;
        expr->result_reg = result_reg;
        if(context.is_const.find(var_id) != context.is_const.end() && context.is_const[var_id]){
            // 是常量，直接读取整数放到t0
            int const_val = expr->evaluate_constant(context);
            context.const_val[var_id] = const_val;
            if (const_val == 0) {
                if(!unused)os << "  sw zero, " << offset << "(s0)" << std::endl;
            }
            else {
                if (const_val >= -2048 && const_val <= 2047) {
                    if(!unused)os << "  addi t"<<result_reg<<", zero, " << const_val << std::endl;
                } else if ((const_val & 0xFFF) == 0) {
                    // 如果是4K对齐的值，使用lui指令
                    if(!unused)os << "  lui t"<<result_reg<<", " << (const_val >> 12) << std::endl;
                } else {
                    if(!unused)os << "  li t"<<result_reg<<", " << const_val << std::endl;
                }
                if(!unused)os << "  sw t"<<result_reg<<", " << offset << "(s0)" << std::endl;
                if(!unused){
                    context.mark_register_free(result_reg);
                    context.mark_register_used(result_reg, var_id);
                }
            }
        }
        else if(expr->is_constant(context)){
            int const_val = expr->evaluate_constant(context);
            if (const_val == 0) {
                if(!unused)os << "  sw zero, " << offset << "(s0)" << std::endl;
            }
            else {
                if (const_val >= -2048 && const_val <= 2047) {
                    if(!unused)os << "  addi t"<<result_reg<<", zero, " << const_val << std::endl;
                } else if ((const_val & 0xFFF) == 0) {
                    // 如果是4K对齐的值，使用lui指令
                    if(!unused)os << "  lui t"<<result_reg<<", " << (const_val >> 12) << std::endl;
                } else {
                    if(!unused)os << "  li t"<<result_reg<<", " << const_val << std::endl;
                }
                if(!unused)os << "  sw t"<<result_reg<<", " << offset << "(s0)" << std::endl;
                if(!unused){
                    context.mark_register_free(result_reg);
                    context.mark_register_used(result_reg, var_id);
                }
            }
        }
        else {
            if(!unused)expr->generate_code(os, context);
            if(!unused)os << "  sw t"<<result_reg<<", " << offset << "(s0)" << std::endl;
            if(!unused){
                context.mark_register_free(result_reg);
                context.mark_register_used(result_reg, var_id);
            }
        }
    }
}

void EmptyStmt::generate_code(std::ostream& os, Context& context) const {
    // Empty statement generates no code
}

// Implementation for new function-related AST nodes

void Parameter::generate_code(std::ostream& os, Context& context) const {
    // Parameters don't generate code directly, they're handled by function declarations
}

void FunctionDecl::generate_code(std::ostream& os, Context& context) const {
    // Calculate required stack frame size based on variables in function body
    std::unordered_set<std::string> variables;
    if (body) {
        body->collect_variables(variables);
    }
    
    context.stack_offset = -12;
    // Add parameters to variables
    for (const auto& param : parameters) {
        variables.insert(param->get_name());
        int offset = context.stack_offset;
        context.add_variable_to_current_scope(param->get_name(), offset);
        context.stack_offset -= 4; // Allocate 4 bytes for the int
    }
    
    size_t variable_count = variables.size();
    
    // Frame size = (variable_count * 4) + 16 (for ra and s0) + alignment
    size_t frame_size = ((variable_count * 4) + 16 + 15) & ~15; // 16-byte aligned
    
    // 优化：如果没有变量，使用最小栈帧
    if (variable_count == 0) {
        frame_size = 16; // 只保存ra和s0
    }
    
    // Function label and prologue
    os << ".globl " << name << std::endl;
    os << name << ":" << std::endl;
    os << "  # Function: " << return_type << " " << name << "(";
    for (size_t i = 0; i < parameters.size(); ++i) {
        if (i > 0) os << ", ";
        os << parameters[i]->get_type() << " " << parameters[i]->get_name();
    }
    os << ")" << std::endl;
    os << "  # Prologue - frame size: " << frame_size << " bytes" << std::endl;
    os << "  addi sp, sp, -" << frame_size << std::endl;
    os << "  sw ra, " << (frame_size - 4) << "(sp)" << std::endl;
    os << "  sw s0, " << (frame_size - 8) << "(sp)" << std::endl;
    os << "  addi s0, sp, " << frame_size << std::endl;
    os << std::endl;
    
    // Set up parameter variables in the context
    Context func_context = context; // Copy context
    func_context.current_function = name; // Set current function name
    func_context.optimize_variable_cache = true; // 启用变量缓存优化
    int param_offset = -12; // Start after ra and s0
    
    // 优化：参数处理
    for (size_t i = 0; i < parameters.size(); ++i) {
        const auto& param = parameters[i];
        func_context.variables[param->get_name()] = param_offset;
        
        // Save parameter from register to stack
        if (i < 8) {
            os << "  sw a" << i << ", " << param_offset << "(s0)" << std::endl;
            // 优化：将参数也缓存到寄存器中
            if (i < 6) { // 使用t0-t5缓存前6个参数
                os << "  mv t" << i << ", a" << i << std::endl;
                func_context.mark_register_used(i, param->get_name());
            }
        } else {
            os << "  lw t0, " << -((i - 8) * 4 + 4) + frame_size + (parameters.size() - 8) * 4<< "(sp)" << std::endl;
            os << "  sw t0, " << param_offset << "(s0)" << std::endl;
        }
        
        param_offset -= 4;
    }
    
    // Generate function body
    if (body) {
        body->generate_code(os, func_context);
    }
    
    // Function epilogue
    os << std::endl;
    os << ".L_return_" << name << ":" << std::endl;
    os << "  # Epilogue" << std::endl;
    os << "  lw ra, " << (frame_size - 4) << "(sp)" << std::endl;
    os << "  lw s0, " << (frame_size - 8) << "(sp)" << std::endl;
    os << "  addi sp, sp, " << frame_size << std::endl;
    os << "  jr ra" << std::endl;
}

void ProgramWithFunctions::generate_code(std::ostream& os, Context& context) const {
    // Generate all function declarations
    for (const auto& func : functions) {
        func->generate_code(os, context);
        os << std::endl;
    }
}

void ProgramWithFunctions::collect_variables(std::unordered_set<std::string>& variables) const {
    for (const auto& func : functions) {
        func->collect_variables(variables);
    }
}

void FunctionCallExpr::generate_code(std::ostream& os, Context& context) const {
    // 优化：减少不必要的注释和调试信息
    if(arguments.size()>8){
        os << "  addi sp, sp, -" << (arguments.size() - 8) * 4 << std::endl;
    }
    
    // 优化：参数评估和传递
    for (size_t i = 0; i < arguments.size(); ++i) {
        arguments[i]->generate_code(os, context);
        // 直接使用a0-a7寄存器传递参数
        if (i < 8) {
            os << "  mv a" << i << ", t0" << std::endl;
        } else {
            // 超过8个参数使用栈
            os << "  sw t0, " << (arguments.size() - 8) * 4 - ((i - 8) * 4 + 4) << "(sp)" << std::endl;
        }
    }
    
    // Call the function
    os << "  call " << function_name << std::endl;
    
    // Result is in a0, move to result register
    os << "  mv t"<<result_reg<<", a0" << std::endl;
    
    // 清理函数使用的寄存器
    int temp = 0;
    for(auto i : regs_used_in_func[function_name].used){
        if(i){
            context.mark_register_free(temp);
        }
        temp++;
    }
    context.mark_register_free(result_reg);
    context.mark_register_used(result_reg, T_VAL_FUNC);
    
    if(arguments.size()>8){
        os << "  addi sp, sp, " << (arguments.size() - 8) * 4 << std::endl;
    }
}

// 错误处理标签和运行时支持
void generate_error_labels(std::ostream& os) {
    os << std::endl;
    os << ".L_div_error:" << std::endl;
    os << "  # Division by zero - terminate with error code" << std::endl;
    os << "  li a0, 1" << std::endl;
    os << "  li a7, 93" << std::endl; // exit system call
    os << "  ecall" << std::endl;
}

int main(int argc, char* argv[]) {
    // Parse the input
    if (yyparse() != 0) {
        std::cerr << "Parsing failed!" << std::endl;
        return 1;
    }

    // Generate code if parsing was successful
    if (parsed_program) {
        Context context;
        
        // 运行优化pass
        OptimizationManager opt_manager(context);
        opt_manager.add_pass(std::make_unique<AlgebraicPass>());
        opt_manager.add_pass(std::make_unique<StrengthReductionPass>());
        opt_manager.add_pass(std::make_unique<CommonSubexprPass>());
        opt_manager.add_pass(std::make_unique<LoopInvariantPass>());
        opt_manager.add_pass(std::make_unique<TailRecursionPass>());
        
        // 执行所有优化
        opt_manager.run_all(*parsed_program);
        
        // 生成优化后的代码
        parsed_program->generate_code(std::cout, context);
        generate_error_labels(std::cout);
    } else {
        std::cerr << "Error: No program parsed" << std::endl;
        return 1;
    }

    return 0;
}
