#include "antlr.h"
#include "symbol.h"
#include "build.h"
#include "CatchStmt.h"
#include "DeferStmt.h"
#include "DoWhileStmt.h"
#include "driver.h"
#include "flex-chapel.h"
#include "ForallStmt.h"
#include "ForLoop.h"
#include "IfExpr.h"
#include "misc.h"
#include "parser.h"
#include "stmt.h"
#include "stringutil.h"
#include "TryStmt.h"
#include "vec.h"
#include "WhileDoStmt.h"

using namespace antlr4;

antlrcpp::Any ANTLRVisitor::visitProgram(ChapelParser::ProgramContext *ctx)
{

    yystartlineno = ctx->getStart()->getLine();
    return ctx->children[0]->accept(this);
}

antlrcpp::Any ANTLRVisitor::visitToplevel_stmt_ls(ChapelParser::Toplevel_stmt_lsContext *ctx)
{

    yystartlineno = ctx->getStart()->getLine();
    size_t n = ctx->children.size();
    BlockStmt *res;
    if (n == 0)
    {
        res = new BlockStmt();
        resetTempID();
    }
    else
    {
        BlockStmt *stmt_ls = ctx->children[0]->accept(this);
        BlockStmt *stmt = ctx->children[1]->accept(this);
        stmt_ls->appendChapelStmt(stmt);
        resetTempID();
        res = stmt_ls;
    }
    return res;
}

antlrcpp::Any ANTLRVisitor::visitToplevel_stmt(ChapelParser::Toplevel_stmtContext *ctx)
{

    yystartlineno = ctx->getStart()->getLine();
    return ctx->children[0]->accept(this);
}

antlrcpp::Any ANTLRVisitor::visitStmt(ChapelParser::StmtContext *ctx)
{

    yystartlineno = ctx->getStart()->getLine();
    Expr *stmt = ctx->children[0]->accept(this);
    return buildChapelStmt(stmt);
}

antlrcpp::Any ANTLRVisitor::visitStmt_level_expr(ChapelParser::Stmt_level_exprContext *ctx)
{

    yystartlineno = ctx->getStart()->getLine();
    Expr *stmt = ctx->children[0]->accept(this);
    return stmt;
}

antlrcpp::Any ANTLRVisitor::visitExpr(ChapelParser::ExprContext *ctx)
{

    yystartlineno = ctx->getStart()->getLine();
    Expr *expr = ctx->children[0]->accept(this);
    return expr;
}

antlrcpp::Any ANTLRVisitor::visitFun_expr(ChapelParser::Fun_exprContext *ctx)
{

    yystartlineno = ctx->getStart()->getLine();
    Expr *res = ctx->children[0]->accept(this);
    return res;
}

antlrcpp::Any ANTLRVisitor::visitLhs_expr(ChapelParser::Lhs_exprContext *ctx)
{

    yystartlineno = ctx->getStart()->getLine();
    Expr *res = ctx->children[0]->accept(this);
    return res;
}

antlrcpp::Any ANTLRVisitor::visitPrimary_expr(ChapelParser::Primary_exprContext *ctx)
{

    yystartlineno = ctx->getStart()->getLine();
    return ctx->children[0]->accept(this);
}

antlrcpp::Any ANTLRVisitor::visitLiteral(ChapelParser::LiteralContext *ctx)
{

    yystartlineno = ctx->getStart()->getLine();
    return buildIntLiteral(ctx->getText().c_str());
}

antlrcpp::Any ANTLRVisitor::visitCall_expr(ChapelParser::Call_exprContext *ctx)
{

    yystartlineno = ctx->getStart()->getLine();
    size_t n = ctx->children.size();
    Expr *res;
    if (n == 1)
    {
        res = ctx->children[0]->accept(this);
    }
    else
    {
        Expr *fun = ctx->children[0]->accept(this);
        CallExpr *actual_ls = ctx->children[2]->accept(this);
        res = new CallExpr(fun, actual_ls);
    }
    return res;
}

antlrcpp::Any ANTLRVisitor::visitIdent_expr(ChapelParser::Ident_exprContext *ctx)
{

    yystartlineno = ctx->getStart()->getLine();
    Expr *res = new UnresolvedSymExpr(ctx->getText().c_str());
    return res;
}

antlrcpp::Any ANTLRVisitor::visitOpt_actual_ls(ChapelParser::Opt_actual_lsContext *ctx)
{

    yystartlineno = ctx->getStart()->getLine();
    size_t n = ctx->children.size();
    CallExpr *res;
    if (n == 0)
        res = new CallExpr(PRIM_ACTUALS_LIST);
    else
        res = ctx->children[0]->accept(this);
    return res;
}

antlrcpp::Any ANTLRVisitor::visitActual_ls(ChapelParser::Actual_lsContext *ctx)
{

    yystartlineno = ctx->getStart()->getLine();
    size_t n = ctx->children.size();
    CallExpr *res;
    if (n == 1)
    {
        Expr *actual = ctx->children[0]->accept(this);
        res = new CallExpr(PRIM_ACTUALS_LIST, actual);
    }
    else
    {
        CallExpr *actual_ls = ctx->children[0]->accept(this);
        Expr *actual = ctx->children[2]->accept(this);
        actual_ls->insertAtTail(actual);
        res = actual_ls;
    }
    return res;
}

antlrcpp::Any ANTLRVisitor::visitActual(ChapelParser::ActualContext *ctx)
{

    yystartlineno = ctx->getStart()->getLine();
    Expr *res = ctx->children[0]->accept(this);
    return res;
}

antlrcpp::Any ANTLRVisitor::visitExponentiation_expr(ChapelParser::Exponentiation_exprContext *ctx)
{

    yystartlineno = ctx->getStart()->getLine();
    size_t n = ctx->children.size();
    Expr *res;
    if (n == 1)
        res = ctx->children[0]->accept(this);
    else
    {
        Expr *base = ctx->children[0]->accept(this);
        Expr *exponent = ctx->children[2]->accept(this);
        res = new CallExpr("**", base, exponent);
    }
    return res;
}

antlrcpp::Any ANTLRVisitor::visitMultiplication_expr(ChapelParser::Multiplication_exprContext *ctx)
{

    yystartlineno = ctx->getStart()->getLine();
    size_t n = ctx->children.size();
    Expr *res;
    if (n == 1)
        res = ctx->children[0]->accept(this);
    else
    {
        Expr *m1 = ctx->children[0]->accept(this);
        const char *op = ctx->children[1]->getText().c_str();
        Expr *m2 = ctx->children[2]->accept(this);
        res = new CallExpr(op, m1, m2);
    }
    return res;
}

antlrcpp::Any ANTLRVisitor::visitAddition_expr(ChapelParser::Addition_exprContext *ctx)
{

    yystartlineno = ctx->getStart()->getLine();
    size_t n = ctx->children.size();
    Expr *res;
    if (n == 1)
        res = ctx->children[0]->accept(this);
    else
    {
        Expr *m1 = ctx->children[0]->accept(this);
        const char *op = ctx->children[1]->getText().c_str();
        Expr *m2 = ctx->children[2]->accept(this);
        res = new CallExpr(op, m1, m2);
    }
    return res;
}
