#pragma once

#include <memory>
#include <vector>
#include <cassert>
#include <Eigen/Eigen>
#include <ifopt/variable_set.h>
#include <ifopt/constraint_set.h>
#include <ifopt/cost_term.h>
#include <ifopt/problem.h>
#include <ifopt/ipopt_solver.h>

#include "problem.hpp"


using SparseJacobian = Eigen::SparseMatrix<double, Eigen::RowMajor>;


class VarSet : public ifopt::VariableSet {
    // a friendlier wrapper for variable sets
public:
    VarSet(const std::string& name, int numVars);
    VarSet(const std::string& name, int numVars, Eigen::VectorXd initVals);
    VarSet(const std::string& name, int numVars, Eigen::VectorXd initVals, std::vector<ifopt::Bounds> bounds);

    void SetVariables(const Eigen::VectorXd& x) override {
        assert(x.size() == GetRows());
        vars = x;
    }

    Eigen::VectorXd GetValues() const override {
        return vars;
    }

    std::vector<ifopt::Bounds> GetBounds() const override {
        return bounds;
    };

private:
    Eigen::VectorXd vars;
    std::vector<ifopt::Bounds> bounds;
};


class IfoptObjective : public ifopt::CostTerm {
    // wrapper for cost terms.
    // note that this actually is framed in terms of a function we want to _maximize_, not minimize
    // since in economics that's typically what we're trying to do
public:
    IfoptObjective(const std::string& name, const std::string& varName, Objective& objectiveFunc);

    double GetCost() const override;

    void FillJacobianBlock(std::string var_set, SparseJacobian& jac) const override;

    void FillJacobianBlock(SparseJacobian& jac) const;

private:
    std::string varName;
    Objective& objectiveFunc;
};


void configure_to_default_solver(std::shared_ptr<ifopt::IpoptSolver> solver);


class IfoptProblem {
    // contains a variable set, constraint set, and objective
    // include functions for solving and changing solver options
public:
    IfoptProblem(
        std::shared_ptr<VarSet> varSet,
        std::shared_ptr<IfoptObjective> objective
    );

    Eigen::ArrayXd solve();

    void changeSolver(std::shared_ptr<ifopt::IpoptSolver> newSolver);

private:
    ifopt::Problem problem;
    std::shared_ptr<ifopt::IpoptSolver> solver = std::make_shared<ifopt::IpoptSolver>();
};
