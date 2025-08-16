#ifndef TOYC_CONTEXT_HPP
#define TOYC_CONTEXT_HPP

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <stack>
#include <climits>

#define T_VAL_INTEGER "0int"
#define T_VAL_BOP_RES "1bop"
#define T_VAL_UNARY_RES "2unary"
#define T_VAL_ASSIGN "3assign"
#define T_VAL_DECL "4decl"
#define T_VAL_FUNC "5func"

// label count
static int label_count = 0;

// A unique label generator for assembly
class LabelFactory {
private:
    std::string prefix;
public:
    LabelFactory(std::string p) : prefix(std::move(p)) {}
    std::string create() {
        return prefix + std::to_string(label_count++);
    }
};

// Register allocation optimization (高级寄存器使用优化)
struct RegisterState {
    bool in_use[8] = {false, false, false, false, false, false, false, false};
    bool star[8] = {false, false, false, false, false, false, false, false};
    std::string t_value[8] = {"","","","","","","",""};
    int last_used_temp = 0;     // 最后使用的临时寄存器编号
};
static RegisterState global_reg_state = RegisterState();
static RegisterState reg_state = RegisterState();
struct UsedRegs{
    bool used[8] = {false, false, false, false, false, false, false, false};
};
static std::vector<RegisterState> reg_state_stack;
static std::unordered_map<std::string, UsedRegs> regs_used_in_func;

// Context holds all the state during code generation
class Context {
public:
    // Symbol table for variable offsets from fp (using unordered_map for O(1) lookup)
    std::unordered_map<std::string, int> variables;
    std::unordered_map<std::string, bool> is_const;
    std::unordered_map<std::string, int> const_val;
    std::unordered_set<std::string> used_variables;
    // std::unordered_map<std::string, int> variable_in_reg;
    std::vector<std::unordered_map<std::string, int>> variables_stack;
    std::vector<std::unordered_map<std::string, bool>> is_const_stack;
    std::vector<std::unordered_map<std::string, int>> const_val_stack;
    std::vector<std::unordered_set<std::string>> used_variables_stack;
    // std::vector<std::unordered_map<std::string, int>> variable_in_reg_stack;
    // Stack of loop labels for break/continue
    std::stack<std::pair<std::string, std::string>> loop_labels;
    // Current stack offset for new variables
    int stack_offset = -12; // Start after ra and old fp
    // Current function name for return statements
    std::string current_function = "main";

    LabelFactory label_factory{"L"};

    // Enhanced optimization flags
    bool optimize_variable_cache = true;

    void push_loop_labels(const std::string& start, const std::string& end) {
        loop_labels.push({start, end});
    }

    void pop_loop_labels() {
        if (!loop_labels.empty()) {
            loop_labels.pop();
        }
    }

    // Scope management methods
    void push_scope() {
        variables_stack.push_back(variables);
        variables = std::unordered_map<std::string, int>();
        is_const_stack.push_back(is_const);
        // is_const = std::unordered_map<std::string, bool>();
        const_val_stack.push_back(const_val);
        // const_val = std::unordered_map<std::string, int>();
        used_variables_stack.push_back(used_variables);
        used_variables = std::unordered_set<std::string>();
        reg_state_stack.push_back(reg_state);
        // reg_state = std::move(reg_state);
    }

    void pop_scope() {
        if(!variables_stack.empty()){
            variables = variables_stack.back();
            variables_stack.pop_back();
        }
        if(!is_const_stack.empty()){
            is_const = is_const_stack.back();
            is_const_stack.pop_back();
        }
        if(!const_val_stack.empty()){
            const_val = const_val_stack.back();
            const_val_stack.pop_back();
        }
        if(!used_variables_stack.empty()){
            used_variables = used_variables_stack.back();
            used_variables_stack.pop_back();
        }
        if(!reg_state_stack.empty()){
            reg_state = reg_state_stack.back();
            reg_state_stack.pop_back();
        }
    }

    bool is_variable_used(const std::string& var) const {
        return used_variables.count(var) > 0;
    }
    void add_used_variable(const std::string& var) {
        used_variables.insert(var);
    }

    bool is_variable_in_current_scope(const std::string& var) const {
        (void)variables_stack.size(); // Suppress unused variable warning
        if(variables.count(var) > 0){
            return true;
        }
        return false;
    }

    void add_variable_to_current_scope(const std::string& var, int offset, int init_val = 0) {
        variables[var] = offset;
        // is_const[var] = true;
        const_val[var] = init_val;
    }

    bool is_variable_declared(const std::string& var){
        // in current scope?
        if (variables.find(var) != variables.end()) {
            return true;
        }

        // if not, find in previous scopes
        int vars_stack_size = variables_stack.size();
        for(int i = vars_stack_size - 1;i>=0;i--){
            auto prev_vars = variables_stack[i];
            if (prev_vars.find(var) != prev_vars.end()) {
                return true;
            }
        }

        // also no
        return false;
    }

    int find_variable_offset(const std::string& var){
        // in current scope?
        if (variables.find(var) != variables.end()) {
            return variables.at(var);
        }

        // if not, find in previous scopes
        int vars_stack_size = variables_stack.size();
        for(int i = vars_stack_size - 1;i>=0;i--){
            auto prev_vars = variables_stack[i];
            if (prev_vars.find(var) != prev_vars.end()) {
                return prev_vars.at(var);
            }
        }

        // also no, err
        std::cerr << "Error: Use of undeclared variable '" << var << "'" << std::endl;
        exit(1);

        return INT_MAX;
    }

    void mark_register_used_inner(const int& reg_id, const std::string& val, RegisterState& state) {
        state.in_use[reg_id] = true;
        state.star[reg_id] = true;
        state.t_value[reg_id] = val;
    }

    void mark_register_used(const int& reg_id, const std::string& val) {
        // std::cout<< "# - marked reg cache t"<<reg_id<<std::endl;
        mark_register_used_inner(reg_id, val, reg_state);
        mark_register_used_inner(reg_id, val, global_reg_state);
        if(regs_used_in_func.find(current_function) == regs_used_in_func.end()){
            regs_used_in_func[current_function] = UsedRegs();
        }
        regs_used_in_func[current_function].used[reg_id] = true;
        // for(int i=reg_state_stack.size() - 1;i>=0;i--){
        //     mark_register_used_inner(reg_id, val, reg_state_stack[i]);
        // }
    }

    int is_var_in_reg_inner(const std::string& var, const RegisterState& state){
        for(int i=0;i<7;i++){
            if(state.in_use[i] && state.t_value[i] == var){
                return i;
            }
        }
        return -1;
    }

    int is_var_in_reg(const std::string& var){
        // for(int i=reg_state_stack.size() - 1;i>=0;i--){
        //     int res = is_var_in_reg_inner(var, reg_state_stack[i]);
        //     if(res != -1){
        //         return res;
        //     }
        // }
        int res = is_var_in_reg_inner(var, reg_state);
        return res;
    }

    void mark_register_free_inner(const int& reg_id, RegisterState& state) {
        state.in_use[reg_id] = false;
        state.t_value[reg_id] = "";
        state.star[reg_id] = false;
    }

    void mark_register_free(const int& reg_id) {
        // std::cout<< "# - freed reg cache t"<<reg_id<<std::endl;
        mark_register_free_inner(reg_id, reg_state);
        for(int i=reg_state_stack.size() - 1;i>=0;i--){
            mark_register_free_inner(reg_id, reg_state_stack[i]);
        }
    }

    void clear_cache(){
        global_reg_state = RegisterState();
        reg_state = RegisterState();
    }

    // New: Get next available temp register with round-robin allocation
    int get_next_temp_register_id() {
        (void)0; // Suppress unused variable warnings
        // search for not in use
        for(int i=2;i<7;i++){
            if(!global_reg_state.in_use[i]){
                return i;
            }
        }
        // std::cout<< "# - all regs are in use"<<std::endl;
        // search for not stared
        for(int i=2;i<7;i++){
            if(!global_reg_state.star[i]){
                return i;
            }
        }
        // std::cout<< "# - all regs stared"<<std::endl;
        // remove stars
        for(int i=2;i<7;i++){
            global_reg_state.star[i] = false;
        }
        return 2;
    }
    std::string get_next_temp_register() {
        return "t" + std::to_string(get_next_temp_register_id());
    }
};

#endif // TOYC_CONTEXT_HPP 