#include <iostream>
#include "solve.hpp"


VarSet::VarSet(
    const std::string& name,
    int numVars
) : ifopt::VariableSet(numVars, name),
    vars(Eigen::VectorXd::Constant(numVars, 1.0)),
    bounds(std::vector<ifopt::Bounds>(numVars, ifopt::BoundGreaterZero))
{}

VarSet::VarSet(
    const std::string& name,
    int numVars,
    Eigen::VectorXd initVals
) : ifopt::VariableSet(numVars, name),
    vars(initVals),
    bounds(std::vector<ifopt::Bounds>(numVars, ifopt::BoundGreaterZero))
{}

VarSet::VarSet(
    const std::string& name,
    int numVars,
    Eigen::VectorXd initVals,
    std::vector<ifopt::Bounds> bounds
) : ifopt::VariableSet(numVars, name),
    vars(initVals),
    bounds(bounds)
{}

IfoptObjective::IfoptObjective(
    const std::string& name,
    const std::string& varName,
    Objective& objectiveFunc
) : ifopt::CostTerm(name),
    varName(varName),
    objectiveFunc(objectiveFunc)
{}

double IfoptObjective::GetCost() const {
    Eigen::ArrayXd x = GetVariables()->GetComponent(varName)->GetValues().array();
    // notice we return the negative objectiveFunc since we're trying to maximize objective
    return objectiveFunc.f(x);
}

void IfoptObjective::FillJacobianBlock(
    std::string var_set,
    SparseJacobian& jac
) const {
    if (var_set == varName) {
        Eigen::ArrayXd x = GetVariables()->GetComponent(varName)->GetValues().array();
        Eigen::Array2d jac_ = objectiveFunc.jac(x);
        jac.coeffRef(0, 0) = jac_(0);
        jac.coeffRef(0, 1) = jac_(1);
    }
}

void IfoptObjective::FillJacobianBlock(SparseJacobian& jac) const {
    return FillJacobianBlock(varName, jac);
}


void configure_solver(
    std::shared_ptr<ifopt::IpoptSolver> solver,
    int max_iter,
    double tol
) {
    // use MUMPS as linear solver
    // if you have the HSL solvers, you should use those instead
    solver->SetOption("linear_solver", "mumps");
    // require jacobians to be pre-provided
    solver->SetOption("jacobian_approximation", "exact");
    // basically only prints status if something goes wrong
    solver->SetOption("print_level", IPOPT_PRINT_LEVEL);
    // suppresses ipopt info message
    solver->SetOption("sb", "yes");
    // set maximum iterations
    solver->SetOption("max_iter", max_iter);
    // set solver tolerance
    solver->SetOption("tol", tol);
}


IfoptProblem::IfoptProblem(
    std::shared_ptr<VarSet> varSet,
    std::shared_ptr<IfoptObjective> objective,
    int max_iter,
    double tol
) {
    problem.AddVariableSet(varSet);
    problem.AddCostSet(objective);
    configure_solver(solver, max_iter, tol);
}

Eigen::ArrayXd IfoptProblem::solve() {
    solver->Solve(problem);
    return problem.GetOptVariables()->GetValues().array();
}

void IfoptProblem::changeSolver(std::shared_ptr<ifopt::IpoptSolver> newSolver) {
    solver = newSolver;
}


// MultiSolver::MultiSolver(
//     const std::vector<Problem>& problems,
//     int max_iters,
//     double exit_tol,
//     const Eigen::ArrayX2d& start_guess
// ) : problems(problems),
//     max_iters(max_iters),
//     exit_tol(exit_tol),
//     start_guess(start_guess),
//     n_problems(problems.size())
// {}

// // need to pass entire vector of results, i is from current_problems
// void MultiSolver::thread_helper(std::vector<Eigen::ArrayX2d>* results, int* current_problems) {
//     {
//         std::lock_guard<std::mutex> lock(my_mutex);
//         if (*current_problems >= n_problems) {
//             return;
//         }
//         else {
//             (*current_problems)++;
//         }
//     }
//     int i = *current_problems - 1;
//     Eigen::ArrayX2d solution = solve(problems[i], max_iters, exit_tol, start_guess);
//     {
//         std::lock_guard<std::mutex> lock(my_mutex);
//         (*results)[i] = solution;
//     }
//     // keep going until *current_problems == n_problems
//     thread_helper(results, current_problems);
// }

// std::vector<Eigen::ArrayX2d> MultiSolver::run() {
//     int n_threads = (n_problems > PROCESSOR_COUNT) ? PROCESSOR_COUNT : n_problems;
//     std::vector<std::thread> threads;
//     threads.reserve(n_threads);
//     int current_problems = 0;
//     std::vector<Eigen::ArrayX2d> results;
//     results.reserve(n_problems);
//     for (int i = 0; i < n_threads; i++) {
//         threads.push_back(
//             std::thread(
//                 &MultiSolver::thread_helper,
//                 this,
//                 &results,
//                 &current_problems
//             )
//         );
//     }
//     for (int i = 0; i < n_threads; i++) {
//         threads[i].join();
//     }
//     return results;
// }

// std::vector<Eigen::ArrayX2d> MultiSolver::run(
//     const std::vector<Problem>& problems,
//     int max_iters,
//     double exit_tol,
//     const Eigen::ArrayX2d& start_guess
// ) {
//     return MultiSolver(problems, max_iters, exit_tol, start_guess).run();
// }

Eigen::ArrayX2d solve_single(
    const Problem& problem,
    const Eigen::ArrayX2d& current_guess,
    int ipopt_max_iter,
    double ipopt_tol
) {
    Eigen::ArrayX2d new_strat(problem.n_players, 2);
    for (int i = 0; i < problem.n_players; i++) {
        Objective objective(problem, i, current_guess);
        auto varSet = std::make_shared<VarSet>("vars", 2, current_guess.row(i));
        auto ifoptObjective = std::make_shared<IfoptObjective>("obj", "vars", objective);
        IfoptProblem ifoptProblem(varSet, ifoptObjective, ipopt_max_iter, ipopt_tol);
        new_strat.row(i) = ifoptProblem.solve();
    }
    return new_strat;
}

Eigen::ArrayX2d solve(
    const Problem& problem,
    const Eigen::ArrayX2d& start_guess,
    int max_iters,
    double exit_tol,
    int ipopt_max_iter,
    double ipopt_tol
) {
    Eigen::ArrayX2d current_guess = start_guess;
    for (int i = 0; i < max_iters; i++) {
        Eigen::ArrayX2d new_guess = solve_single(problem, current_guess, ipopt_max_iter, ipopt_tol);
        if (((new_guess - current_guess) / current_guess).abs().maxCoeff() < exit_tol) {
            std::cout << "Exited on iteration " << i << '\n';
            return new_guess;
        }
        current_guess = new_guess;
    }
    std::cout << "Reached max iterations\n";
    return current_guess;
}

Eigen::ArrayX2d solve(
    const Problem& problem,
    int max_iters,
    double exit_tol,
    int ipopt_max_iter,
    double ipopt_tol
) {
    return solve(
        problem,
        // default starting guess is just 1.0 for everything
        Eigen::ArrayX2d::Constant(problem.n_players, 2, 1.0),
        max_iters,
        exit_tol,
        ipopt_max_iter,
        ipopt_tol
    );
}
