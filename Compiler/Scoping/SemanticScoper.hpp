//
//  CallableScoper.hpp
//  Emojicode
//
//  Created by Theo Weidmann on 07/05/16.
//  Copyright © 2016 Theo Weidmann. All rights reserved.
//

#ifndef CallableScoper_hpp
#define CallableScoper_hpp

#include "Functions/Function.hpp"
#include "SemanticScopeStats.hpp"
#include "Scope.hpp"
#include <list>
#include <vector>

namespace EmojicodeCompiler {

struct ResolvedVariable {
    ResolvedVariable(Variable &variable, bool inInstanceScope) : variable(variable), inInstanceScope(inInstanceScope) {}
    Variable &variable;
    bool inInstanceScope;
};

struct FunctionObjectVariableInformation;

/// Scoper used during Semantic Analysis. Assigns ID's to variables that are used with IDScoper later.
class SemanticScoper {
public:
    SemanticScoper() = default;
    explicit SemanticScoper(Scope *instanceScope) : instanceScope_(instanceScope) {};

    static SemanticScoper scoperForFunction(Function *function);

    /// Retrieves a variable or throws a @c VariableNotFoundError if the variable is not found.
    virtual ResolvedVariable getVariable(const std::u32string &name, const SourcePosition &errorPosition);

    /// Returns the current subscope.
    Scope& currentScope() {
        assert(!scopes_.empty());
        return scopes_.front();
    }

    /// Pushes a new subscope.
    void pushScope();

    /// Pushes a new subscope and sets the argument variables in it.
    virtual Scope& pushArgumentsScope(const std::vector<Parameter> &arguments, const SourcePosition &p);

    /// Issues an error if the variable `name` is found.
    /// This method is called before declaring variables to warn against variable shadowing.
    void checkForShadowing(const std::u32string &name, const SourcePosition &p, Compiler *compiler) const;

    /// Pops the current scope and calls @c recommendFrozenVariables on it.
    /// @returns The number of variables that were in this scope.
    SemanticScopeStats popScope(Compiler *compiler);

    /// Returns the instance scope or @c nullptr
    Scope* instanceScope() const { return instanceScope_; }

    /// The number of variable ids that were assigned.
    size_t variableIdCount() const { return maxVariableId_; }

    virtual ~SemanticScoper() = default;

protected:
    int maxInitializationLevel() const { return maxInitializationLevel_; }

    /// Returns the topmost local scope, i.e. the one in which all other locals scopes are subscopes.
    Scope& topmostLocalScope() {
        return scopes_.back();
    }

private:
    void updateMaxVariableIdForPopping() {
        auto &scope = currentScope();
        if (scope.maxVariableId() > maxVariableId_) {
            maxVariableId_ = scope.maxVariableId();
        }
    }

    void pushScopeInternal() {
        scopes_.emplace_front(Scope(scopes_.empty() ? maxVariableId_ : scopes_.front().maxVariableId()));
    }

    std::list<Scope> scopes_;
    Scope *instanceScope_ = nullptr;
    int maxInitializationLevel_ = 1;
    size_t maxVariableId_ = 0;
};

}  // namespace EmojicodeCompiler

#endif /* CallableScoper_hpp */
