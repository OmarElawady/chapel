#ifndef _CHPL_ANTLR_H
#define _CHOL_ANTLR_H

#include "antlr4-runtime.h"
#include "ChapelLexer.h"
#include "ChapelVisitor.h"
#include "ChapelBaseVisitor.h"

class ANTLRVisitor : public ChapelBaseVisitor
{
public:
    virtual antlrcpp::Any visitProgram(ChapelParser::ProgramContext *ctx);

    virtual antlrcpp::Any visitToplevel_stmt_ls(ChapelParser::Toplevel_stmt_lsContext *ctx);

    virtual antlrcpp::Any visitToplevel_stmt(ChapelParser::Toplevel_stmtContext *ctx);

    virtual antlrcpp::Any visitStmt(ChapelParser::StmtContext *ctx);

    virtual antlrcpp::Any visitStmt_level_expr(ChapelParser::Stmt_level_exprContext *ctx);

    virtual antlrcpp::Any visitExpr(ChapelParser::ExprContext *ctx);

    virtual antlrcpp::Any visitFun_expr(ChapelParser::Fun_exprContext *ctx);

    virtual antlrcpp::Any visitLhs_expr(ChapelParser::Lhs_exprContext *ctx);

    virtual antlrcpp::Any visitPrimary_expr(ChapelParser::Primary_exprContext *ctx);

    virtual antlrcpp::Any visitLiteral(ChapelParser::LiteralContext *ctx);

    virtual antlrcpp::Any visitCall_expr(ChapelParser::Call_exprContext *ctx);

    virtual antlrcpp::Any visitIdent_expr(ChapelParser::Ident_exprContext *ctx);

    virtual antlrcpp::Any visitOpt_actual_ls(ChapelParser::Opt_actual_lsContext *ctx);

    virtual antlrcpp::Any visitActual_ls(ChapelParser::Actual_lsContext *ctx);

    virtual antlrcpp::Any visitActual(ChapelParser::ActualContext *ctx);

    virtual antlrcpp::Any visitExponentiation_expr(ChapelParser::Exponentiation_exprContext *ctx);

    virtual antlrcpp::Any visitMultiplication_expr(ChapelParser::Multiplication_exprContext *ctx);

    virtual antlrcpp::Any visitAddition_expr(ChapelParser::Addition_exprContext *ctx);
};
#endif