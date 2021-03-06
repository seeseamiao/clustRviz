#include "clustRviz.h"

// [[Rcpp::export(rng = false)]]
Rcpp::List CARPcpp(const Eigen::MatrixXd& X,
                   const Eigen::ArrayXXd& M,
                   const Eigen::MatrixXd& D,
                   const Eigen::VectorXd& weights,
                   double epsilon,
                   double t,
                   double rho              = 1,
                   double thresh           = CLUSTRVIZ_DEFAULT_STOP_PRECISION,
                   int max_iter            = 100000,
                   int max_inner_iter      = 2500,
                   int burn_in             = 50,
                   double back             = 0.5,
                   int keep                = 10,
                   int viz_max_inner_iter  = 15,
                   double viz_initial_step = 1.1,
                   double viz_small_step   = 1.01,
                   bool l1                 = false,
                   bool show_progress      = true,
                   bool back_track         = false,
                   bool exact              = false){

  ConvexClustering problem(X, M, D, weights, rho, l1, show_progress);

  if(exact){
    if(back_track){
      ConvexClusteringADMM_VIZ admm_viz(problem,
                                        epsilon,
                                        thresh,
                                        max_iter,
                                        max_inner_iter,
                                        burn_in,
                                        back,
                                        viz_max_inner_iter,
                                        viz_initial_step,
                                        viz_small_step);

      return admm_viz.build_return_object();
    } else {
      ConvexClusteringADMM admm(problem, epsilon, t, thresh, max_iter, max_inner_iter);
      return admm.build_return_object();
    }
  } else {
    if(back_track){
      CARP_VIZ carp_viz(problem,
                        epsilon,
                        max_iter,
                        burn_in,
                        back,
                        keep,
                        viz_max_inner_iter,
                        viz_initial_step,
                        viz_small_step);

      return carp_viz.build_return_object();
    }

    CARP carp(problem, epsilon, t, max_iter, burn_in, keep);
    return carp.build_return_object();
  }
}

// [[Rcpp::export(rng = false)]]
Rcpp::List CBASScpp(const Eigen::MatrixXd& X,
                    const Eigen::ArrayXXd& M,
                    const Eigen::MatrixXd& D_row,
                    const Eigen::MatrixXd& D_col,
                    const Eigen::VectorXd& weights_row,
                    const Eigen::VectorXd& weights_col,
                    double epsilon,
                    double t,
                    double thresh           = CLUSTRVIZ_DEFAULT_STOP_PRECISION,
                    double rho              = 1,
                    int max_iter            = 100000,
                    int max_inner_iter      = 2500,
                    int burn_in             = 50,
                    double back             = 0.5,
                    int keep                = 10,
                    int viz_max_inner_iter  = 15,
                    double viz_initial_step = 1.1,
                    double viz_small_step   = 1.01,
                    bool l1                 = false,
                    bool show_progress      = true,
                    bool back_track         = false,
                    bool exact              = false){

  ConvexBiClustering problem(X, M, D_row, D_col, weights_row, weights_col, rho, l1, show_progress);

  if(exact){
    if(back_track){
      ConvexBiClusteringADMM_VIZ admm_viz(problem,
                                          epsilon,
                                          thresh,
                                          max_iter,
                                          max_inner_iter,
                                          burn_in,
                                          back,
                                          viz_max_inner_iter,
                                          viz_initial_step,
                                          viz_small_step);

      return admm_viz.build_return_object();
    } else {
      ConvexBiClusteringADMM admm(problem, epsilon, t, thresh, max_iter, max_inner_iter);
      return admm.build_return_object();
    }
  } else {
    if(back_track){
      CBASS_VIZ cbass_viz(problem,
                          epsilon,
                          max_iter,
                          burn_in,
                          back,
                          keep,
                          viz_max_inner_iter,
                          viz_initial_step,
                          viz_small_step);

      return cbass_viz.build_return_object();
    }

    CBASS cbass(problem, epsilon, t, max_iter, burn_in, keep);
    return cbass.build_return_object();
  }
}

// [[Rcpp::export(rng = false)]]
Rcpp::List ConvexClusteringCPP(const Eigen::MatrixXd& X,
                               const Eigen::ArrayXXd& M,
                               const Eigen::MatrixXd& D,
                               const Eigen::VectorXd& weights,
                               const std::vector<double> lambda_grid,
                               double rho         = 1,
                               double thresh      = CLUSTRVIZ_DEFAULT_STOP_PRECISION,
                               int max_iter       = 100000,
                               int max_inner_iter = 2500,
                               bool l1            = false,
                               bool show_progress = true){

  ConvexClustering problem(X, M, D, weights, rho, l1, show_progress);
  UserGridConvexClusteringADMM solver(problem, lambda_grid, thresh, max_iter, max_inner_iter);

  return solver.build_return_object();
}

// [[Rcpp::export(rng = false)]]
Rcpp::List ConvexBiClusteringCPP(const Eigen::MatrixXd& X,
                                 const Eigen::ArrayXXd& M,
                                 const Eigen::MatrixXd& D_row,
                                 const Eigen::MatrixXd& D_col,
                                 const Eigen::VectorXd& weights_row,
                                 const Eigen::VectorXd& weights_col,
                                 const std::vector<double> lambda_grid,
                                 double rho         = 1,
                                 double thresh      = CLUSTRVIZ_DEFAULT_STOP_PRECISION,
                                 int max_iter       = 100000,
                                 int max_inner_iter = 2500,
                                 bool l1            = false,
                                 bool show_progress = true){

  ConvexBiClustering problem(X, M, D_row, D_col, weights_row, weights_col, rho, l1, show_progress);
  UserGridConvexBiClusteringADMM solver(problem, lambda_grid, thresh, max_iter, max_inner_iter);

  return solver.build_return_object();
}
