# ToyC 编译器开发文档

## 1. 项目概述

本项目旨在为 ToyC 语言开发一个编译器。ToyC 是 C 语言的一个精心设计的子集，专注于核心的命令式编程特性。该编译器将使用 C++ 语言在 Linux 环境下开发，并利用 Flex 进行词法分析，Bison/Yacc 进行语法分析。最终目标是生成能在 32 位 RISC-V 架构上运行的汇编代码。

整个项目将遵循现代编译器设计的最佳实践，代码将力求清晰、模块化和可扩展，并从你提供的 `ExecEngine_C_Compiler` 仓库中汲取灵感。

## 2. ToyC 语言规范

由于无法直接读取你提供的 PDF 文档，本文档将基于你给出的语法片段、对 C 语言子集的通用理解以及补全后的规则，来定义 ToyC 的语言规范。

### 2.1. 数据类型

- `int`: 唯一的原生数据类型，表示 32 位有符号整数。

### 2.2. 关键字

- `int`
- `if`, `else`
- `while`
- `break`, `continue`
- `return`

### 2.3. 运算符

- **算术运算符**: `+`, `-`, `*`, `/`, `%`
- **关系运算符**: `==`, `!=`, `<`, `<=`, `>`, `>=`
- **赋值运算符**: `=`

### 2.4. 注释

- `// ...` (单行注释)
- `/* ... */` (多行注释)

### 2.5. 词法规范 (Tokens)

| Token 类型   | 正则表达式 / 描述                 | 示例                |
| ------------ | ----------------------------------- | ------------------- |
| `INT`        | "int"                               | `int`               |
| `IF`         | "if"                                | `if`                |
| `ELSE`       | "else"                              | `else`              |
| `WHILE`      | "while"                             | `while`             |
| `BREAK`      | "break"                             | `break`             |
| `CONTINUE`   | "continue"                          | `continue`          |
| `RETURN`     | "return"                            | `return`            |
| `ID`         | `[a-zA-Z_][a-zA-Z0-9_]*`             | `my_var`, `_temp`   |
| `INTEGER`    | `[0-9]+`                            | `123`, `0`          |
| `ASSIGN`     | `=`                                 | `=`                 |
| `EQ`, `NE`   | `==`, `!=`                          | `==`, `!=`          |
| `LT`, `GT`   | `<`, `>`                            | `<`, `>`            |
| `LE`, `GE`   | `<=`, `>=`                          | `<=`, `>=`          |
| `PLUS`, `MINUS` | `+`, `-`                           | `+`, `-`            |
| `STAR`, `SLASH` | `*`, `/`                           | `*`, `/`            |
| `PERCENT`    | `%`                                 | `%`                 |
| `LPAREN`, `RPAREN` | `(`, `)`                      | `(`, `)`            |
| `LBRACE`, `RBRACE` | `{`, `}`                      | `{`, `}`            |
| `SEMI`       | `;`                                 | `;`                 |

### 2.6. 语法规范 (BNF)

为了使语言功能更完整且符合 C 语言的习惯（例如，支持 `a = b = 5;`），我对 `Expr` 的定义进行了扩展，将赋值操作作为一种表达式。

```bnf
Program ::= StmtList

StmtList ::= Stmt
           | StmtList Stmt

Stmt ::= DeclStmt
       | ExprStmt
       | IfStmt
       | WhileStmt
       | BreakStmt
       | ContinueStmt
       | ReturnStmt
       | BlockStmt
       | EmptyStmt

DeclStmt ::= 'int' ID '=' Expr ';'

ExprStmt ::= Expr ';'

IfStmt ::= 'if' '(' Expr ')' Stmt
         | 'if' '(' Expr ')' Stmt 'else' Stmt

WhileStmt ::= 'while' '(' Expr ')' Stmt

BreakStmt ::= 'break' ';'

ContinueStmt ::= 'continue' ';'

ReturnStmt ::= 'return' Expr ';'
             | 'return' ';'

BlockStmt ::= '{' StmtList '}'
            | '{' '}'

EmptyStmt ::= ';'

Expr ::= AssignmentExpr

AssignmentExpr ::= EqualityExpr
                 | ID '=' Expr

EqualityExpr ::= RelationalExpr
               | EqualityExpr '==' RelationalExpr
               | EqualityExpr '!=' RelationalExpr

RelationalExpr ::= AdditiveExpr
                 | RelationalExpr '<' AdditiveExpr
                 | RelationalExpr '>' AdditiveExpr
                 | RelationalExpr '<=' AdditiveExpr
                 | RelationalExpr '>=' AdditiveExpr

AdditiveExpr ::= MultiplicativeExpr
               | AdditiveExpr '+' MultiplicativeExpr
               | AdditiveExpr '-' MultiplicativeExpr

MultiplicativeExpr ::= UnaryExpr
                     | MultiplicativeExpr '*' UnaryExpr
                     | MultiplicativeExpr '/' UnaryExpr
                     | MultiplicativeExpr '%' UnaryExpr

UnaryExpr ::= PrimaryExpr
            | '-' UnaryExpr

PrimaryExpr ::= ID
              | INTEGER
              | '(' Expr ')'
```

## 3. 编译器架构

编译器将分为前端和后端两个主要部分。

### 3.1. 前端 (Frontend)

前端负责将 ToyC 源代码转换为一种中间表示——抽象语法树 (AST)。

#### 3.1.1. 词法分析 (Lexer)
- **工具**: Flex
- **文件**: `src/lexer.flex`
- **任务**: 读取源代码，将其分解为一系列 token (见 2.5 节)，并传递给解析器。它将忽略空白和注释。

#### 3.1.2. 语法分析 (Parser)
- **工具**: Bison/Yacc
- **文件**: `src/parser.y`
- **任务**: 接收 Lexer 提供的 token 序列，根据 2.6 节定义的语法规则进行匹配。在匹配成功时，构建 AST。

#### 3.1.3. 抽象语法树 (AST)
- **文件**: `include/ast.hpp`
- **设计**:
    - 定义一个基类 `Node`，所有 AST 节点的根。它将提供一个接口（例如，一个虚方法 `GenerateCode`) 来启动代码生成。
    - 为每种语法结构（如 `Expr`, `Stmt`, `BinaryOpExpr`, `IfStmt` 等）创建派生类。
    - AST 将是连接前端和后端的关键数据结构。

### 3.2. 后端 (Backend)

后端接收 AST，并生成目标 RISC-V 汇编代码。

#### 3.2.1. 上下文与符号表 (Context & Symbol Table)
- **实现**:
    - 创建一个 `Context` 类来贯穿整个代码生成过程。
    - `Context` 内部维护一个符号表 `std::map<std::string, int>`，用于存储变量名和它们在当前栈帧中的偏移量。
    - 为支持作用域（如 `if` 和 `while` 内部的块），可以设计一个栈式的符号表结构。进入新作用域时推入新 map，退出时弹出。
    - `Context` 还将负责管理 `while` 循环的开始和结束标签（用于 `break`/`continue`）以及栈帧大小。

#### 3.2.2. 代码生成 (Code Generation)
- **核心逻辑**: 通过访问者模式或在每个 AST 节点内定义一个 `GenerateCode` 方法，递归地遍历 AST。
- **实现**: 每个 AST 节点都负责生成其对应的汇编代码片段。例如，`BinaryOpExpr` 的代码生成逻辑会先生成计算左右子表达式的代码，然后生成执行相应操作的指令。

## 4. RISC-V 代码生成策略

### 4.1. 寄存器约定

我们将遵循 RISC-V 的标准调用约定：
- **临时寄存器 (`t0`-`t6`)**: 用于表达式求值，不需要保存。
- **保存寄存器 (`s0`-`s11`)**: `s0` 将用作帧指针(FP)。如果使用其他 `s` 寄存器，必须在函数序言中保存，在函数尾声中恢复。
- **参数寄存器 (`a0`-`a7`)**: 用于函数调用（在ToyC中，主要用于 `return`）。

### 4.2. 栈帧布局

每个函数（在本项目中，主要是隐式的 `main` 函数）都有自己的栈帧。

```
+------------------+  <- high address
| ...              |
| argument N       |
| ...              |
| argument 0       |
+------------------+
| return address(ra)|
+------------------+
| old fp (s0)      |  <- current fp (s0)
+------------------+
| local var 1      |
+------------------+
| local var 2      |
+------------------+
| ...              |
+------------------+  <- current sp
|                  |
+------------------+  <- low address
```

- **帧指针 (`fp`)**: 使用 `s0` 寄存器。它在函数执行期间保持不变，指向一个固定的位置。
- **栈指针 (`sp`)**: 指向栈顶。它会在分配/释放栈空间时移动。

### 4.3. 函数序言 (Prologue)

在 `main` 函数（或任何未来添加的函数）的开始处：
1.  **分配栈帧**: `addi sp, sp, -FRAME_SIZE`
2.  **保存返回地址**: `sw ra, FRAME_SIZE-4(sp)`
3.  **保存旧的 fp**: `sw s0, FRAME_SIZE-8(sp)`
4.  **设置新的 fp**: `addi s0, sp, FRAME_SIZE`

### 4.4. 函数尾声 (Epilogue)

在函数 `return` 之前：
1.  **恢复返回地址**: `lw ra, -4(s0)`
2.  **恢复旧的 fp**: `lw s0, -8(s0)`
3.  **释放栈帧**: `addi sp, sp, FRAME_SIZE`
4.  **返回**: `jr ra`

### 4.5. 变量访问

- 局部变量通过 `fp` (即 `s0`) 加上一个负偏移量来访问。例如，第一个局部变量的地址可能是 ` -12(s0)`。

## 5. 开发步骤

1.  **环境搭建**: 配置好 `g++`, `flex`, `bison`, `make` 和 RISC-V 工具链（用于测试）。
2.  **AST 节点定义 (`include/ast.hpp`)**: 定义所有需要的 AST 节点类。
3.  **词法分析器 (`src/lexer.flex`)**: 实现对 ToyC 所有 token 的识别。
4.  **语法分析器 (`src/parser.y`)**: 实现 ToyC 的语法规则，并构建 AST。
5.  **主程序 (`src/main.cpp`)**: 编写 `main` 函数，串联词法分析和语法分析，并调用代码生成。
6.  **代码生成器 (集成在 AST 节点中)**: 逐个实现每个 AST 节点的代码生成逻辑。从最简单的 `INTEGER` 和 `ReturnStmt` 开始。
7.  **Makefile 编写**: 创建一个 `Makefile` 来自动化编译过程。
8.  **测试**: 编写简单的 ToyC 程序，编译成 RISC-V 汇编，并在模拟器（如 `Spike`）或实际硬件上验证其正确性。

---
这份开发文档为我们接下来的工作奠定了坚实的基础。接下来，我将根据这份文档，开始逐步编写代码。我们先从创建 `Makefile` 和定义 `ast.hpp` 开始。
