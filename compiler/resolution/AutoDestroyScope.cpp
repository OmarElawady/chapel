/*
 * Copyright 2004-2020 Hewlett Packard Enterprise Development LP
 * Other additional copyright holders may be indicated within.
 *
 * The entirety of this work is licensed under the Apache License,
 * Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "AutoDestroyScope.h"

#include "driver.h"
#include "expr.h"
#include "DeferStmt.h"
#include "resolution.h"
#include "stmt.h"
#include "symbol.h"

/************************************* | **************************************
*                                                                             *
* Track the state of lexical scopes during the execution of a function.       *
*                                                                             *
*   1) We only track variables that have an autoDestroy flag                  *
*                                                                             *
*   2) The compiler introduces "formal temps" to manage formals with out and  *
*      in-out concrete intents.  If a function has multiple returns then any  *
*      formal temps must be handled differently from other locals.            *
*                                                                             *
************************************** | *************************************/

static VarSymbol* variableToExclude(FnSymbol*  fn, Expr* refStmt);

static BlockStmt* findBlockForTarget(GotoStmt* stmt);

static void deinitializeOrCopyElide(Expr* before, Expr* after, VarSymbol* var);

AutoDestroyScope::AutoDestroyScope(AutoDestroyScope* parent,
                                   const BlockStmt* block) {
  mParent        = parent;
  mBlock         = block;

  mLocalsHandled = false;
}

// For a = or PRIM_ASSIGN setting an arg from a formal temp
// in the epilogue (for out or inout), return the formal temp handled.
// Otherwise, return NULL
static VarSymbol* findFormalTempAssignBack(const Expr* stmt) {
  if (const CallExpr* call = toConstCallExpr(stmt)) {
    SymExpr* lhsSe = NULL;
    SymExpr* rhsSe = NULL;
    if (FnSymbol* fn = call->resolvedFunction()) {
      if (fn->hasFlag(FLAG_ASSIGNOP) == true && call->numActuals() == 2) {
        lhsSe = toSymExpr(call->get(1));
        rhsSe = toSymExpr(call->get(2));
      }
    } else if (call->isPrimitive(PRIM_MOVE) ||
               call->isPrimitive(PRIM_ASSIGN)) {
      lhsSe = toSymExpr(call->get(1));
      rhsSe = toSymExpr(call->get(2));
    }

    Symbol* lhs = NULL;
    Symbol* rhs = NULL;
    if (lhsSe != NULL && rhsSe != NULL) {
      lhs = lhsSe->symbol();
      rhs = rhsSe->symbol();
    }

    if (lhs != NULL && rhs != NULL && isArgSymbol(lhs)) {
      if (rhs->hasFlag(FLAG_FORMAL_TEMP)) {
        VarSymbol* var = toVarSymbol(rhs);
        INT_ASSERT(var);
        return var;
      }
    }
  }
  return NULL;
}

void AutoDestroyScope::addFormalTemps() {
  FnSymbol* fn = const_cast<BlockStmt*>(mBlock)->getFunction();
  INT_ASSERT(mParent == NULL);
  INT_ASSERT(fn != NULL);

  if (fn->hasFlag(FLAG_EXTERN))
    return;

  bool anyOutInout = false;
  for_formals(formal, fn) {
    if (formal->intent == INTENT_OUT ||
        formal->originalIntent == INTENT_OUT ||
        formal->intent == INTENT_INOUT ||
        formal->originalIntent == INTENT_INOUT) {
      anyOutInout = true;
    }
  }
  if (anyOutInout) {
    // Go through the function epilogue looking for
    // write-backs to args from FORMAL_TEMP variables
    LabelSymbol* epilogue = fn->getEpilogueLabel();
    INT_ASSERT(epilogue != NULL);
    // should have been created in resolution
    Expr* next = NULL;
    for (Expr* cur = epilogue->defPoint; cur != NULL; cur = next) {
      next = cur->next;
      if (VarSymbol* var = findFormalTempAssignBack(cur)) {
        CallExpr* call = toCallExpr(cur);
        INT_ASSERT(var->hasFlag(FLAG_FORMAL_TEMP_INOUT) ||
                   var->hasFlag(FLAG_FORMAL_TEMP_OUT));
        INT_ASSERT(call);
        mFormalTempActions.push_back(call);
        call->remove(); // will be added back in just before destroying
      }
    }
  }
}

void AutoDestroyScope::variableAdd(VarSymbol* var) {
  mDeclaredVars.insert(var);
}

void AutoDestroyScope::deferAdd(DeferStmt* defer) {
  mLocalsAndDefers.push_back(defer);
}

void AutoDestroyScope::addInitialization(VarSymbol* var) {
  // Note: this will be called redundantly.
  for (AutoDestroyScope* cur = this; cur != NULL; cur = cur->mParent) {
    if (cur->mInitedVars.insert(var).second) {
      // An insertion occured, meaning this was the first
      // thing that looked like initialization for this variable.

      if (cur->mDeclaredVars.count(var) > 0) {
        // Add it to mDeclaredVars at the declaration scope.
        cur->mLocalsAndDefers.push_back(var);
        break;
      } else {
        // Or add it to mInitedOuterVars at inner scopes.
        cur->mInitedOuterVars.push_back(var);
      }
    }
  }
}

// Forget about initializations for outer variables initialized
// in this scope. The variables will no longer be considered initialized.
// This matters for split-init and conditionals.
void AutoDestroyScope::forgetOuterVariableInitializations() {

  // iterate through mInitedOuterVars in reverse
  size_t count = mInitedOuterVars.size();
  for (size_t i = 1; i <= count; i++) {
    VarSymbol* var = mInitedOuterVars[count - i];

    // Each outer variable should be stored in some parent scope's
    // mLocalsAndDefers.
    for (AutoDestroyScope* cur = this; cur != NULL; cur = cur->mParent) {
      if (cur->mInitedVars.erase(var) > 0 ) {
        if (cur->mDeclaredVars.count(var) > 0) {
          // expecting to find it at the last position in mLocalsAndDefers
          INT_ASSERT(cur->mLocalsAndDefers.back() == var);
          cur->mLocalsAndDefers.pop_back();
          break;
        } else {
          // expecting to find it at the last position in mInitedOuterVars
          INT_ASSERT(cur->mInitedOuterVars.back() == var);
          cur->mInitedOuterVars.pop_back();
        }
      }
    }
  }
}

std::vector<VarSymbol*> AutoDestroyScope::getInitedOuterVars() const {
  return mInitedOuterVars;
}

AutoDestroyScope* AutoDestroyScope::getParentScope() const {
  return mParent;
}


//
// Functions have an informal epilogue defined by code that
//
//   1) appears after a common "return label" (if present)
//   2) copies values to out/in-out formals
//
// We must      destroy the primaries before (1).
// We choose to destroy the primaries before (2).
//
// This code detects the start of (2)
//
bool AutoDestroyScope::handlingFormalTemps(const Expr* stmt) const {
  bool retval = false;

  if (mLocalsHandled == false) {

    if (findFormalTempAssignBack(stmt) != NULL) {
      retval = true;
    }
  }

  return retval;
}

// If the refStmt is a goto then we need to recurse
// to the block that contains the target of the goto
//
// adds autodestroys after refStmt
void AutoDestroyScope::insertAutoDestroys(FnSymbol* fn, Expr* refStmt,
                                          const std::set<VarSymbol*>& ignored) {
  GotoStmt*               gotoStmt   = toGotoStmt(refStmt);
  bool                    recurse    = (gotoStmt != NULL) ? true : false;
  BlockStmt*              forTarget  = findBlockForTarget(gotoStmt);
  VarSymbol*              excludeVar = variableToExclude(fn, refStmt);
  const AutoDestroyScope* scope      = this;
  bool                    gotoError  = false;
  std::set<VarSymbol*>    ignoredSet(ignored);

  if (gotoStmt != NULL && gotoStmt->gotoTag == GOTO_ERROR_HANDLING)
    gotoError = true;

  // Error handling gotos need to include auto-destroys
  // for any in-scope variables for the block containing
  // the error-handling label.
  // Compare with while/break, say, in which the
  // variables in the parent block are assumed to be destroyed by the
  // parent block.
  // Error handling gotos also need to include auto-destroys
  // for outer variables initialized in the scope (in the try block).

  while (scope != NULL) {
    // stop when block == forTarget for non-error-handling gotos
    if (scope->mBlock == forTarget && gotoError == false)
      break;

    // destroy outer variables initialized in the scope too
    if (gotoError) {
      scope->destroyOuterVariables(refStmt, ignoredSet);
    }

    scope->variablesDestroy(refStmt, excludeVar, ignoredSet, this);

    // stop if recurse == false or if block == forTarget
    if (recurse == false)
      break;
    if (scope->mBlock == forTarget)
      break;

    scope = scope->mParent;
  }

  mLocalsHandled = true;
}

static void findCopy(CallExpr* call, VarSymbol* var,
                     CallExpr** copyToElide, Symbol** copyToLhs)
{
  if (call->isNamedAstr(astrInitEquals)) {
    if (call->numActuals() >= 2) {
      if (SymExpr* lhsSe = toSymExpr(call->get(1))) {
        if (SymExpr* rhsSe = toSymExpr(call->get(2))) {
          if (lhsSe->getValType() == rhsSe->getValType()) {
            if (rhsSe->symbol() == var) {
              *copyToElide = call;
              *copyToLhs = lhsSe->symbol();
            }
          }
        }
      }
    }
  } else if (call->isPrimitive(PRIM_MOVE) || call->isPrimitive(PRIM_ASSIGN)) {
    if (SymExpr* lhsSe = toSymExpr(call->get(1))) {
      if (CallExpr* rhsCall = toCallExpr(call->get(2))) {
        if (rhsCall->isNamed("chpl__initCopy") ||
            rhsCall->isNamed("chpl__autoCopy")) {
          if (rhsCall->numActuals() >= 1) {
            if (SymExpr* rhsSe = toSymExpr(rhsCall->get(1))) {
              if (lhsSe->getValType() == rhsSe->getValType()) {
                if (rhsSe->symbol() == var) {
                  *copyToElide = call;
                  *copyToLhs = lhsSe->symbol();
                }
              }
            }
          }
        }
      }
    }
  } else if (call->isNamed("chpl__initCopy")||call->isNamed("chpl__autoCopy")) {
    if (call->numActuals() >= 2) {
      if (SymExpr* rhsSe = toSymExpr(call->get(1))) {
        if (SymExpr* lhsSe = toSymExpr(call->get(2))) {
          if (lhsSe->getValType() == rhsSe->getValType()) {
            if (rhsSe->symbol() == var) {
              *copyToElide = call;
              *copyToLhs = lhsSe->symbol();
            }
          }
        }
      }
    }
  }
}

static void deinitializeOrCopyElide(Expr* before, Expr* after, VarSymbol* var) {
  if (FnSymbol* autoDestroyFn = autoDestroyMap.get(var->type)) {
    INT_ASSERT(autoDestroyFn->hasFlag(FLAG_AUTO_DESTROY_FN));

    CallExpr* copyToElide = NULL;
    Symbol* copyToLhs = NULL;

    // Check to see if copy-elision is possible.
    if (fNoCopyElision == false) {
      // variable is dead at last mention.
      // is copy-initialization the last mention of this variable?
      // (Don't consider the end-of-statement marker for the copy-init itself)

      std::vector<SymExpr*> symExprs;

      // Scroll backwards finding uses of the variable.
      Expr* cur = NULL;
      if (after != NULL) cur = after;
      if (before != NULL) cur = before->prev;

      bool foundEndOfStatementMentioning = false;

      while (cur != NULL) {
        if (isCallExpr(cur) || isDefExpr(cur)) {
          CallExpr* call = toCallExpr(cur);

          symExprs.clear();
          collectSymExprsFor(cur, var, symExprs);
          if (symExprs.size() > 0) {
            // Found a mention
            if (call == NULL) {
              break; // found a mention not in a call
            } else {
              // in a call
              if (call->isPrimitive(PRIM_END_OF_STATEMENT)) {
                // in an end of statement marker
                if (foundEndOfStatementMentioning) {
                  break; // stop if already encountered a statement mentioning
                } else {
                  foundEndOfStatementMentioning = true;
                  // keep looking; ignore this end-of-statement
                }
              } else {
                // in another call - check if it is a copy
                findCopy(call, var, &copyToElide, &copyToLhs);

                // stop the search if we found a mention.
                break;
              }
            }
          }

        } else {
          // stop the search if it was a nested block
          break;
        }
        cur = cur->prev;
      }
    }

    if (copyToElide == NULL) {
      SET_LINENO(var);
      CallExpr* autoDestroy = new CallExpr(autoDestroyFn, var);
      if (before)
        before->insertBefore(autoDestroy);
      else
        after->insertAfter(autoDestroy);
    } else {
      SET_LINENO(copyToElide);
      // Change the copy into a move and don't destroy the variable.
      copyToElide->convertToNoop();
      copyToElide->insertBefore(new CallExpr(PRIM_ASSIGN, copyToLhs, var));
    }
  }
}

void AutoDestroyScope::destroyVariable(Expr* after, VarSymbol* var,
                                       const std::set<VarSymbol*>& ignored) {
  INT_ASSERT(!var->hasFlag(FLAG_FORMAL_TEMP));

  if (ignored.count(var) == 0) {
    deinitializeOrCopyElide(NULL, after, var);
  }
}

// Destroy outer variables and add them to the ignored set
// This is used for error handling cases
void AutoDestroyScope::destroyOuterVariables(Expr* before,
                                             std::set<VarSymbol*>& ignored) const
{
  size_t count = mInitedOuterVars.size();
  for (size_t i = 1; i <= count; i++) {
    VarSymbol* var = mInitedOuterVars[count - i];
    INT_ASSERT(!var->hasFlag(FLAG_FORMAL_TEMP));

    if (ignored.count(var) == 0) {
      deinitializeOrCopyElide(before, NULL, var);
      ignored.insert(var);
    }
  }
}

// If 'refStmt' is in a shadow variable's initBlock(),
// return that svar's deinitBlock(). Otherwise return NULL.
static BlockStmt* shadowVarsDeinitBlock(Expr* refStmt) {
  if (ShadowVarSymbol* svar = toShadowVarSymbol(refStmt->parentSymbol))
    if (refStmt->parentExpr == svar->initBlock())
      return svar->deinitBlock();

  return NULL;
}

// add autoDestroys after refStmt
void AutoDestroyScope::variablesDestroy(Expr*      refStmt,
                                        VarSymbol* excludeVar,
                                        const std::set<VarSymbol*>& ignored,
                                        AutoDestroyScope* startingScope) const {
  // Handle the primary locals
  if (mLocalsHandled == false) {
    Expr*  insertBeforeStmt = refStmt;
    Expr*  noop             = NULL;
    size_t count            = mLocalsAndDefers.size();

    // If this is a simple nested block, insert after the final stmt
    // But always insert the destruction calls in reverse declaration order.
    // Do not get tricked by sequences of unreachable code
    if (count == 0) {
      // Don't bother adding a noop if there is nothing to insert
      insertBeforeStmt = NULL;
    } else if (refStmt->next == NULL) {
      if (mParent != NULL && !isGotoStmt(refStmt)) {
        SET_LINENO(refStmt);
        // Add a PRIM_NOOP to insert before
        noop = new CallExpr(PRIM_NOOP);
        if (BlockStmt* deinitBlock = shadowVarsDeinitBlock(refStmt)) {
          // 'deinitBlock' may already have deinit() of the shadow var.
          // The shadow var was probably the last thing that was initialized
          // in its initBlock(). So it should be the first to be DEinitialized
          // in the deinitBlock. Everything else, then, should go after that.
          deinitBlock->insertAtTail(noop);
        } else {
          refStmt->insertAfter(noop);
        }
        insertBeforeStmt = noop;
      }
    }

    // Add formal temp writebacks for non-error returns
    // Do the writebacks in formal declaration order
    GotoStmt* gotoStmt = toGotoStmt(refStmt);
    bool forErrorReturn = gotoStmt != NULL &&
                          gotoStmt->gotoTag == GOTO_ERROR_HANDLING_RETURN;

    if (forErrorReturn == false) {
      size_t nActions = mFormalTempActions.size();
      for (size_t i = 0; i < nActions; i++) {
        CallExpr* action = mFormalTempActions[i];
        SET_LINENO(action);
        refStmt->insertBefore(action->copy());
      }
    }

    for (size_t i = 1; i <= count; i++) {
      BaseAST*  localOrDefer = mLocalsAndDefers[count - i];
      VarSymbol* var = toVarSymbol(localOrDefer);
      DeferStmt* defer = toDeferStmt(localOrDefer);
      // This code only handles VarSymbols and DeferStmts.
      // It handles both in one vector because the order
      // of interleaving matters.
      INT_ASSERT(var || defer);

      if (var != NULL && var != excludeVar && ignored.count(var) == 0) {
        if (startingScope->isVariableInitialized(var)) {
          bool outIntentFormalReturn = forErrorReturn == false &&
                                       var->hasFlag(FLAG_FORMAL_TEMP_OUT);
          // No deinit for out formal returns - deinited at call site
          if (outIntentFormalReturn == false)
            deinitializeOrCopyElide(insertBeforeStmt, NULL, var);
        }
      }

      if (defer != NULL) {
        SET_LINENO(defer);
        BlockStmt* deferBlockCopy = defer->body()->copy();
        insertBeforeStmt->insertBefore(deferBlockCopy);
        deferBlockCopy->flattenAndRemove();
      }
    }

    // remove the PRIM_NOOP if we added one.
    if (noop != NULL)
      noop->remove();
  }

}

bool AutoDestroyScope::isVariableInitialized(VarSymbol* var) const {
  for (const AutoDestroyScope* scope = this;
       scope != NULL;
       scope = scope->mParent) {
    if (scope->mInitedVars.count(var) > 0)
      return true;
  }

  return false;
}

// Walk backwards from the current statement to determine if a sequence of
// moves have copied a variable that is marked for auto destruction in to
// the dedicated return-temp within the current scope.
//
// Note that the value we are concerned about may be copied in to one or
// more temporary variables between being copied to the return temp.
static VarSymbol* variableToExclude(FnSymbol* fn, Expr* refStmt) {
  Symbol* retVar = NULL;
  VarSymbol* exclude = NULL;

  if (fn->hasFlag(FLAG_FN_RETARG)) {
    // FnSymbol::getReturnSymbol does not work on functions
    // transformed in this way, so work around that issue.

    // Starting from the end of the function, find a PRIM_ASSIGN to the RETARG
    for (Expr* cur = fn->body->body.last(); cur != NULL; cur = cur->prev) {
      if (CallExpr* call = toCallExpr(cur)) {
        if (call->isPrimitive(PRIM_ASSIGN)) {
          if (SymExpr* lhsSe = toSymExpr(call->get(1))) {
            if (ArgSymbol* lhs = toArgSymbol(lhsSe->symbol())) {
              if (lhs->hasFlag(FLAG_RETARG)) {
                SymExpr* rhsSe = toSymExpr(call->get(2));
                VarSymbol* rhs = toVarSymbol(rhsSe->symbol());
                // rhs commonly has FLAG_RVV but not necessarily
                retVar = rhs;
                break;
              }
            }
          }
        }
      }
    }
  } else {
    retVar = toVarSymbol(fn->getReturnSymbol());
  }

  // TODO: migrate variableToExclude to addAutoDestroys
  // and the excluded set.

  if (retVar != NULL) {
    if (typeNeedsCopyInitDeinit(retVar->type) ||
        fn->hasFlag(FLAG_INIT_COPY_FN)) {
      Symbol* needle = retVar;
      Expr*   expr   = refStmt;

      // Walk backwards looking for the variable that is being returned
      while (exclude == NULL && expr != NULL && needle != NULL) {
        if (CallExpr* move = toCallExpr(expr)) {
          if (move->isPrimitive(PRIM_MOVE) || move->isPrimitive(PRIM_ASSIGN)) {
            SymExpr*   lhs    = toSymExpr(move->get(1));

            if (needle == lhs->symbol()) {
              if (SymExpr* rhs = toSymExpr(move->get(2))) {
                VarSymbol* rhsVar = toVarSymbol(rhs->symbol());

                if (isAutoDestroyedVariable(rhsVar) == true) {
                  exclude = rhsVar;
                } else {
                  needle = rhsVar;
                }
              } else {
                needle = NULL;
              }
            }
          }
        }

        expr = expr->prev;
      }
    }
  }

  return exclude;
}

// Find the block stmt that encloses the target of this gotoStmt
static BlockStmt* findBlockForTarget(GotoStmt* stmt) {
  BlockStmt* retval = NULL;

  if (stmt != NULL && stmt->isGotoReturn() == false) {
    SymExpr* labelSymExpr = toSymExpr(stmt->label);
    Expr*    ptr          = labelSymExpr->symbol()->defPoint;

    while (ptr != NULL && isBlockStmt(ptr) == false) {
      ptr = ptr->parentExpr;
    }

    retval = toBlockStmt(ptr);

    INT_ASSERT(retval);
  }

  return retval;
}
