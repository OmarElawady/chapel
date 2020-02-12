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

#include "ImportStmt.h"

#include "AstVisitor.h"
#include "ResolveScope.h"

ImportStmt::ImportStmt(BaseAST* source) : Stmt(E_ImportStmt) {
  if (Symbol* b = toSymbol(source)) {
    src = new SymExpr(b);

  } else if (Expr* b = toExpr(source)) {
    src = b;

  } else {
    INT_FATAL(this, "Bad mod in ImportStmt constructor");
  }

  gImportStmts.add(this);
}

ImportStmt* ImportStmt::copyInner(SymbolMap* map) {
  ImportStmt* _this = new ImportStmt(COPY_INT(src));

  return _this;
}

Expr* ImportStmt::getFirstExpr() {
  return this;
}

void ImportStmt::replaceChild(Expr* oldAst, Expr* newAst) {
  if (oldAst == src) {
    src = newAst;

  } else {
    INT_FATAL(this, "Unexpected case in ImportStmt::replaceChild");
  }
}

void ImportStmt::accept(AstVisitor* visitor) {
  visitor->visitImportStmt(this);
}

void ImportStmt::verify() {
  Expr::verify();

  if (astTag != E_ImportStmt) {
    INT_FATAL(this, "Bad NamedExpr::astTag");
  }

  if (src == NULL) {
    INT_FATAL(this, "Bad ImportStmt::src");
  }

  verifyNotOnList(src);
}

//
// Resolve the module symbol referred to by the ImportStmt
//
void ImportStmt::scopeResolve(ResolveScope* scope) {
  /*
  // 2017-05-28: isValid() does not currently return on failure
  if (isValid(src) == true) { */
  // isValid should be re-enabled when we support general exprs in imports
  // As it is today, we won't encounter the situations it is preventing.
  // There is an else branch to turn on as well
    // 2017/05/28 The parser inserts a normalized UseStmt of ChapelBase
    if (SymExpr* se = toSymExpr(src)) {
      INT_FATAL("This should only happen for a UseStmt");

    } else if (Symbol* sym = scope->lookup(src, /*isUse=*/ true)) {
      SET_LINENO(this);

      if (ModuleSymbol* modSym = toModuleSymbol(sym)) {
        if (modSym->defPoint->parentSymbol != theProgram) {
          USR_FATAL_CONT(this, "unable to import nested modules");
        }
        scope->enclosingModule()->moduleUseAdd(modSym);

        updateEnclosingBlock(scope, sym);

      } else {
        if (sym->isImmediate() == true) {
          USR_FATAL(this,
                    "'import' statements must refer to module symbols "
                    "(e.g., 'import <module>;')");

        } else if (sym->name != NULL) {
          USR_FATAL_CONT(this,
                         "'import' of non-module symbol %s",
                         sym->name);
          USR_FATAL_CONT(sym,  "Definition of symbol %s", sym->name);
          USR_STOP();

        } else {
          USR_FATAL(this, "'import' of non-module symbol");
        }
      }
    } else {
      if (UnresolvedSymExpr* import = toUnresolvedSymExpr(src)) {
        USR_FATAL(this, "Cannot find module '%s'", import->unresolved);
      } else {
        USR_FATAL(this, "Cannot find module");
      }
    }
    /*
  } else {
    INT_ASSERT(false);
    }*/
}

//
// Go into the module referred to by the import statement
//
BaseAST* ImportStmt::getSearchScope() const {
  BaseAST* retval = NULL;

  if (SymExpr* se = toSymExpr(src)) {
    if (ModuleSymbol* module = toModuleSymbol(se->symbol())) {
      retval = module->block;

    } else {
      INT_FATAL(this, "Import invalid, not applied to module");
    }

  } else {
    INT_FATAL(this, "getSearchScope called before this import was processed");
  }

  return retval;
}

//
// Returns the module symbol if the name provided matches the module imported
//
Symbol* ImportStmt::checkIfModuleNameMatches(const char* name) {
  if (SymExpr* se = toSymExpr(src)) {
    if (ModuleSymbol* modSym = toModuleSymbol(se->symbol())) {
      if (strcmp(name, se->symbol()->name) == 0) {
        return modSym;
      }
    }
  } else {
    // Though we don't support it yet, things like `import M.N.O` probably
    // wouldn't reach here because we resolve such cases element-by-element
    // rather than wholesale.  Nothing else should fall under this category
  }
  return NULL;
}

//
// Extends the scope's block statement to store this import, after replacing the
// UnresolvedSymExpr we store with the found symbol
//
void ImportStmt::updateEnclosingBlock(ResolveScope* scope, Symbol* sym) {
  src->replace(new SymExpr(sym));

  remove();
  scope->asBlockStmt()->useListAdd(this);

  scope->extend(this);
}
