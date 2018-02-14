#define ARMA_USE_SUPERLU 1
// [[Rcpp::depends(RcppArmadillo)]]
#include <RcppArmadillo.h>
#define ARMA_64BIT_WORD

//' @useDynLib clustRviz
arma::mat UnVec(arma::colvec x, int nrows, int ncols){
  arma::mat ret(nrows,ncols,arma::fill::zeros);
  arma::uword startidx;
  arma::uword stopidx;
  arma::ucolvec idx;
  for(arma::uword coliter = 0; coliter < ncols; coliter++){
    startidx = nrows*coliter;
    stopidx = startidx + nrows - 1;
    idx = arma::linspace<arma::ucolvec>(startidx,stopidx,nrows);
    ret.col(coliter) = x.elem(idx);
  }
  return(ret);
}

double TwoNorm(arma::colvec x){

  return(std::pow(sum(x % x),0.5));

}

arma::colvec DMatOpv2(arma::colvec u, int p, arma::umat IndMat, arma::umat EOneIndMat, arma::umat ETwoIndMat ){
  int cardE = EOneIndMat.n_rows;
  arma::ucolvec retIdx(p);
  arma::ucolvec EOneIdx(p);
  arma::ucolvec ETwoIdx(p);
  arma::colvec ret(cardE*p);
  for(int l = 0; l < cardE; l++){
    retIdx = IndMat.row(l).t();
    EOneIdx = EOneIndMat.row(l).t();
    ETwoIdx = ETwoIndMat.row(l).t();
    ret.elem(retIdx) = u.elem(EOneIdx) - u.elem(ETwoIdx);
  }
  return(ret);
}

arma::colvec DtMatOpv2(arma::colvec v, int n, int p, arma::umat IndMat,arma::umat EOneIndMat, arma::umat ETwoIndMat){
  arma::colvec out(n*p,arma::fill::zeros);
  arma::ucolvec vIdx(p);
  arma::ucolvec EOneIdx(p);
  arma::ucolvec ETwoIdx(p);
  int cardE = IndMat.n_rows;

  for(int l = 0; l < cardE; l++){
    vIdx = IndMat.row(l).t();
    EOneIdx = EOneIndMat.row(l).t();
    ETwoIdx = ETwoIndMat.row(l).t();
    out.elem(EOneIdx) = out.elem(EOneIdx) + v.elem(vIdx);
    out.elem(ETwoIdx) = out.elem(ETwoIdx) - v.elem(vIdx);
  }
  return(out);
}

int sgn(double x){
  int ret;
  if(x == 0){
    ret = 0;
  } else{
    ret = x/std::abs(x);
  }
  return(ret);
}

arma::colvec ProxL2(arma::colvec delta, int p, arma::colvec scalars, arma::umat IndMat){
  int cardE = IndMat.n_rows;
  arma::ucolvec retIdx(p);

  arma::colvec valvec(2);
  valvec(0) = 0;

  arma::colvec ret(p*cardE);

  for(int l = 0; l<cardE;l++){
    retIdx = IndMat.row(l).t();
    valvec(1) = 1 - scalars(l)/TwoNorm(delta.elem(retIdx));
    if(valvec.has_nan()){
      ret.elem(retIdx) = delta.elem(retIdx);
    } else{
      ret.elem(retIdx) = valvec.max()*delta.elem(retIdx);
    }
  }

  return(ret);
}


// [[Rcpp::export]]
arma::colvec ProxL1(arma::colvec delta, int p, double lambda, arma::colvec weights){
  int cardE = weights.n_rows;
  arma::colvec theta(cardE*p);
  arma::colvec ret(cardE*p);
  arma::colvec valvec(2);
  double ols;
  arma::uword widx;
  valvec(0) = 0;
  for(int l = 0; l < cardE*p; l++){
    widx = std::floor(l/p);
    ols = delta(l)*weights(widx);
    valvec(1) = std::abs(ols) - (lambda*std::pow(weights(widx),2));
    theta(l) = (sgn(ols))*valvec.max();
    ret(l) = theta(l)/weights(widx);
  }
  return(ret);
}

// [[Rcpp::export]]
Rcpp::List CARPL2_VIS_FRAC(arma::colvec x, int n, int p, double lambda_init, arma::colvec weights,arma::colvec uinit, arma::colvec vinit,arma::sp_mat premat, arma::umat IndMat, arma::umat EOneIndMat, arma::umat ETwoIndMat, double rho = 1, int max_iter = 1e4,int burn_in = 50,bool verbose=false,double back = 0.5, double try_tol = 1e-3,int ti = 15, double t_switch = 1.01,int keep=10){

  double t = 1.1;
  int try_iter;

  int cardE = EOneIndMat.n_rows;
  arma::colvec unew(n*p);
  arma::colvec uold(n*p);
  unew = uinit;

  arma::colvec vnew(p*cardE);
  arma::colvec vold(p*cardE);
  vnew = vinit;

  arma::colvec lamnew(p*cardE);
  arma::colvec lamold(p*cardE);
  lamnew = vnew;

  arma::colvec proxin(p*cardE);
  double lambda = lambda_init;
  double lambda_old = lambda;
  double lambda_try_old;
  double lambda_lower;
  double lambda_upper;
  double try_eps;

  arma::uword path_iter = 0;
  arma::uword iter = 0;
  arma::mat UPath(n*p,1);
  UPath.col(iter)= uinit;
  arma::mat VPath(p*cardE,1);
  VPath.col(iter)= vinit;
  arma::colvec lambda_path(1);
  lambda_path(iter) = lambda_init;

  arma::colvec vZeroIndsnew(cardE,arma::fill::zeros);
  arma::colvec vZeroIndsold(cardE);
  arma::ucolvec vIdx(p);
  arma::mat vZeroInds_Path(cardE,1,arma::fill::zeros);

  bool rep_iter;
  int nzeros_old;
  int nzeros_new = 0;
  Rcpp::List ret;


  while( (iter < max_iter) & (nzeros_new < cardE) ){
    iter += 1;
    uold = unew;
    vold = vnew;
    lamold = lamnew;
    vZeroIndsold = vZeroIndsnew;
    nzeros_old = nzeros_new;

    rep_iter = true;
    try_iter = 0;
    lambda_upper = lambda;
    lambda_lower = lambda_old;
    lambda_try_old = lambda;
    while(rep_iter){
      try_iter = try_iter + 1;
      // Do update
      // u update
      unew = arma::spsolve(premat, (1/rho)*x + (1/rho)*DtMatOpv2(rho*vold - lamold,n,p,IndMat,EOneIndMat,ETwoIndMat));
      // v update
      proxin = DMatOpv2(unew,p,IndMat,EOneIndMat,ETwoIndMat) + (1/rho)*lamold;
      vnew = ProxL2(proxin,p,(1/rho)*weights*lambda, IndMat);

      // lambda update
      lamnew = lamold + rho*(DMatOpv2(unew,p,IndMat,EOneIndMat,ETwoIndMat)-vnew);

      // check number of sparsity events
      for(int l = 0; l<cardE; l++){
        vIdx = IndMat.row(l).t();
        if(sum(vnew.elem(vIdx)) == 0){
          vZeroIndsnew(l) = 1;
        }
      }
      nzeros_new = sum(vZeroIndsnew);
      if( (nzeros_new == nzeros_old) & (try_iter == 1)){
        // same sparsity on first try
        // go to next iteration
        rep_iter = false;
      } else if(nzeros_new > nzeros_old + 1){
        // too much sparsity
        // redo iteration
        vZeroIndsnew = vZeroIndsold;
        if(try_iter == 1){
          lambda = 0.5*(lambda_lower + lambda_upper);
        } else{
          lambda_upper = lambda;
          lambda = (0.5)*(lambda_lower + lambda_upper);
        }
      } else if(nzeros_new == nzeros_old){
        // not enough sparsity
        // redo iteration
        vZeroIndsnew = vZeroIndsold;
        lambda_lower = lambda;
        lambda = (0.5)*(lambda_lower + lambda_upper);
      } else{
        // just enough sparsity
        rep_iter = false;
      }

      try_eps = std::abs(lambda - lambda_try_old) / std::abs(lambda_try_old);
      if(try_iter > ti){
        rep_iter = false;
      }
    }

    if(nzeros_new > 0){
      t = t_switch;
    }
    // if sparsity event occurs, record it
    if( (nzeros_new!=nzeros_old) | (iter % keep == 0)){
      path_iter = path_iter + 1;
      UPath.insert_cols(path_iter,1);
      UPath.col(path_iter) = unew;
      VPath.insert_cols(path_iter,1);
      VPath.col(path_iter) = vnew;
      lambda_path.insert_rows(path_iter,1);
      lambda_path(path_iter) = lambda;
      lambda_old = lambda;

      vZeroInds_Path.insert_cols(path_iter,1);
      vZeroInds_Path.col(path_iter) = vZeroIndsnew;
    }

    lambda = lambda*t;








  }
  ret["u.path"] = UPath;
  ret["v.path"] = VPath;
  ret["v.zero.inds"]= vZeroInds_Path;
  ret["lambda.path"] = lambda_path;
  return(ret);
}

// [[Rcpp::export]]
Rcpp::List CARPL2_NF_FRAC(arma::colvec x, int n, int p, double lambda_init,double t, arma::colvec weights,arma::colvec uinit, arma::colvec vinit,arma::sp_mat premat, arma::umat IndMat, arma::umat EOneIndMat, arma::umat ETwoIndMat, double rho = 1, int max_iter = 1e4,int burn_in = 50,bool verbose=false,int keep=10){


  int cardE = EOneIndMat.n_rows;
  arma::colvec unew(n*p);
  arma::colvec uold(n*p);
  unew = uinit;

  arma::colvec vnew(p*cardE);
  arma::colvec vold(p*cardE);
  vnew = vinit;

  arma::colvec lamnew(p*cardE);
  arma::colvec lamold(p*cardE);
  lamnew = vnew;

  arma::colvec proxin(p*cardE);
  double lambda = lambda_init;

  arma::uword path_iter = 0;
  arma::uword iter = 0;
  arma::mat UPath(n*p,1);
  UPath.col(iter)= uinit;
  arma::mat VPath(p*cardE,1);
  VPath.col(iter)= vinit;
  arma::colvec lambda_path(1);
  lambda_path(iter) = lambda_init;

  arma::colvec vZeroIndsnew(cardE,arma::fill::zeros);
  arma::colvec vZeroIndsold(cardE);
  arma::ucolvec vIdx(p);
  arma::mat vZeroInds_Path(cardE,1,arma::fill::zeros);

  int nzeros_old = 0;
  int nzeros_new = 0;
  Rcpp::List ret;


  while( (iter < max_iter) & (nzeros_new < cardE) ){
    iter += 1;
    uold = unew;
    vold = vnew;
    lamold = lamnew;
    vZeroIndsold = vZeroIndsnew;
    nzeros_old = nzeros_new;

    // u update
    unew = arma::spsolve(premat, (1/rho)*x + (1/rho)*DtMatOpv2(rho*vold - lamold,n,p,IndMat,EOneIndMat,ETwoIndMat));
    // v update
    proxin = DMatOpv2(unew,p,IndMat,EOneIndMat,ETwoIndMat) + (1/rho)*lamold;
    vnew = ProxL2(proxin,p,(1/rho)*weights*lambda, IndMat);

    // // lambda update
    lamnew = lamold + rho*(DMatOpv2(unew,p,IndMat,EOneIndMat,ETwoIndMat)-vnew);

    for(int l = 0; l<cardE; l++){
      vIdx = IndMat.row(l).t();
      if(sum(vnew.elem(vIdx)) == 0){
        vZeroIndsnew(l) = 1;
      }
    }
    nzeros_new = sum(vZeroIndsnew);
    if( (nzeros_new!=nzeros_old) | (iter % keep == 0)) {
      path_iter = path_iter + 1;
      UPath.insert_cols(path_iter,1);
      UPath.col(path_iter) = unew;
      VPath.insert_cols(path_iter,1);
      VPath.col(path_iter) = vnew;
      lambda_path.insert_rows(path_iter,1);
      lambda_path(path_iter) = lambda;

      vZeroInds_Path.insert_cols(path_iter,1);
      vZeroInds_Path.col(path_iter) = vZeroIndsnew;
    }




    if(iter >= burn_in){
      lambda = lambda*t;
    }


  }
  ret["u.path"] = UPath;
  ret["v.path"] = VPath;
  ret["v.zero.inds"]= vZeroInds_Path;
  ret["lambda.path"] = lambda_path;
  return(ret);
}

// [[Rcpp::export]]
Rcpp::List CARPL1_NF_FRAC(arma::colvec x, int n, int p, double lambda_init,double t, arma::colvec weights,arma::colvec uinit, arma::colvec vinit,arma::sp_mat premat, arma::umat IndMat, arma::umat EOneIndMat, arma::umat ETwoIndMat, double rho = 1, int max_iter = 1e4,int burn_in = 50,bool verbose=false,int keep=10){


  int cardE = EOneIndMat.n_rows;
  arma::colvec unew(n*p);
  arma::colvec uold(n*p);
  unew = uinit;

  arma::colvec vnew(p*cardE);
  arma::colvec vold(p*cardE);
  vnew = vinit;

  arma::colvec lamnew(p*cardE);
  arma::colvec lamold(p*cardE);
  lamnew = vnew;

  arma::colvec proxin(p*cardE);
  double lambda = lambda_init;

  arma::uword path_iter = 0;
  arma::uword iter = 0;
  arma::mat UPath(n*p,1);
  UPath.col(iter)= uinit;
  arma::mat VPath(p*cardE,1);
  VPath.col(iter)= vinit;
  arma::colvec lambda_path(1);
  lambda_path(iter) = lambda_init;

  arma::colvec vZeroIndsnew(cardE,arma::fill::zeros);
  arma::colvec vZeroIndsold(cardE);
  arma::ucolvec vIdx(p);
  arma::mat vZeroInds_Path(cardE,1,arma::fill::zeros);

  int nzeros_old = 0;
  int nzeros_new = 0;
  Rcpp::List ret;


  while( (iter < max_iter) & (nzeros_new < cardE) ){
    iter += 1;
    uold = unew;
    vold = vnew;
    lamold = lamnew;
    vZeroIndsold = vZeroIndsnew;
    nzeros_old = nzeros_new;

    // u update
    unew = arma::spsolve(premat, (1/rho)*x + (1/rho)*DtMatOpv2(rho*vold - lamold,n,p,IndMat,EOneIndMat,ETwoIndMat));
    // v update
    proxin = DMatOpv2(unew,p,IndMat,EOneIndMat,ETwoIndMat) + (1/rho)*lamold;
    vnew = ProxL1(proxin,p,(1/rho)*lambda,weights);

    // lambda update
    lamnew = lamold + rho*(DMatOpv2(unew,p,IndMat,EOneIndMat,ETwoIndMat)-vnew);

    for(int l = 0; l<cardE; l++){
      vIdx = IndMat.row(l).t();
      if(sum(vnew.elem(vIdx)) == 0){
        vZeroIndsnew(l) = 1;
      }
    }
    nzeros_new = sum(vZeroIndsnew);
    if( (nzeros_new!=nzeros_old) | (iter % keep == 0)) {
      path_iter = path_iter + 1;
      UPath.insert_cols(path_iter,1);
      UPath.col(path_iter) = unew;
      VPath.insert_cols(path_iter,1);
      VPath.col(path_iter) = vnew;
      lambda_path.insert_rows(path_iter,1);
      lambda_path(path_iter) = lambda;

      vZeroInds_Path.insert_cols(path_iter,1);
      vZeroInds_Path.col(path_iter) = vZeroIndsnew;
    }




    if(iter >= burn_in){
      lambda = lambda*t;
    }


  }
  ret["u.path"] = UPath;
  ret["v.path"] = VPath;
  ret["v.zero.inds"]= vZeroInds_Path;
  ret["lambda.path"] = lambda_path;
  return(ret);
}

// [[Rcpp::export]]
Rcpp::List CARPL1_VIS_FRAC(arma::colvec x, int n, int p, double lambda_init, arma::colvec weights,arma::colvec uinit, arma::colvec vinit,arma::sp_mat premat, arma::umat IndMat, arma::umat EOneIndMat, arma::umat ETwoIndMat, double rho = 1, int max_iter = 1e4,int burn_in = 50,bool verbose=false,double back = 0.5, double try_tol = 1e-3,int ti = 15, double t_switch = 1.01,int keep=10){

  double t = 1.1;
  int try_iter;

  int cardE = EOneIndMat.n_rows;
  arma::colvec unew(n*p);
  arma::colvec uold(n*p);
  unew = uinit;

  arma::colvec vnew(p*cardE);
  arma::colvec vold(p*cardE);
  vnew = vinit;

  arma::colvec lamnew(p*cardE);
  arma::colvec lamold(p*cardE);
  lamnew = vnew;

  arma::colvec proxin(p*cardE);
  double lambda = lambda_init;
  double lambda_old = lambda;
  double lambda_try_old;
  double lambda_lower;
  double lambda_upper;
  double try_eps;

  arma::uword path_iter = 0;
  arma::uword iter = 0;
  arma::mat UPath(n*p,1);
  UPath.col(iter)= uinit;
  arma::mat VPath(p*cardE,1);
  VPath.col(iter)= vinit;
  arma::colvec lambda_path(1);
  lambda_path(iter) = lambda_init;

  arma::colvec vZeroIndsnew(cardE,arma::fill::zeros);
  arma::colvec vZeroIndsold(cardE);
  arma::ucolvec vIdx(p);
  arma::mat vZeroInds_Path(cardE,1,arma::fill::zeros);

  bool rep_iter;
  int nzeros_old;
  int nzeros_new = 0;
  Rcpp::List ret;


  while( (iter < max_iter) & (nzeros_new < cardE) ){
    iter += 1;
    uold = unew;
    vold = vnew;
    lamold = lamnew;
    vZeroIndsold = vZeroIndsnew;
    nzeros_old = nzeros_new;

    rep_iter = true;
    try_iter = 0;
    lambda_upper = lambda;
    lambda_lower = lambda_old;
    lambda_try_old = lambda;
    while(rep_iter){
      try_iter = try_iter + 1;
      // Do update
      // u update
      unew = arma::spsolve(premat, (1/rho)*x + (1/rho)*DtMatOpv2(rho*vold - lamold,n,p,IndMat,EOneIndMat,ETwoIndMat));
      // v update
      proxin = DMatOpv2(unew,p,IndMat,EOneIndMat,ETwoIndMat) + (1/rho)*lamold;
      vnew = ProxL1(proxin,p,(1/rho)*lambda,weights);

      // lambda update
      lamnew = lamold + rho*(DMatOpv2(unew,p,IndMat,EOneIndMat,ETwoIndMat)-vnew);

      // check number of sparsity events
      for(int l = 0; l<cardE; l++){
        vIdx = IndMat.row(l).t();
        if(sum(vnew.elem(vIdx)) == 0){
          vZeroIndsnew(l) = 1;
        }
      }
      nzeros_new = sum(vZeroIndsnew);
      if( (nzeros_new == nzeros_old) & (try_iter == 1)){
        // same sparsity on first try
        // go to next iteration
        rep_iter = false;
      } else if(nzeros_new > nzeros_old + 1){
        // too much sparsity
        // redo iteration
        vZeroIndsnew = vZeroIndsold;
        if(try_iter == 1){
          lambda = 0.5*(lambda_lower + lambda_upper);
        } else{
          lambda_upper = lambda;
          lambda = (0.5)*(lambda_lower + lambda_upper);
        }
      } else if(nzeros_new == nzeros_old){
        // not enough sparsity
        // redo iteration
        vZeroIndsnew = vZeroIndsold;
        lambda_lower = lambda;
        lambda = (0.5)*(lambda_lower + lambda_upper);
      } else{
        // just enough sparsity
        rep_iter = false;
      }

      try_eps = std::abs(lambda - lambda_try_old) / std::abs(lambda_try_old);
      if(try_iter > ti){
        rep_iter = false;
      }
    }

    if(nzeros_new > 0){
      t = t_switch;
    }
    // if sparsity event occurs, record it
    if( (nzeros_new!=nzeros_old) | (iter % keep == 0)){
      path_iter = path_iter + 1;
      UPath.insert_cols(path_iter,1);
      UPath.col(path_iter) = unew;
      VPath.insert_cols(path_iter,1);
      VPath.col(path_iter) = vnew;
      lambda_path.insert_rows(path_iter,1);
      lambda_path(path_iter) = lambda;
      lambda_old = lambda;

      vZeroInds_Path.insert_cols(path_iter,1);
      vZeroInds_Path.col(path_iter) = vZeroIndsnew;
    }

    lambda = lambda*t;








  }
  ret["u.path"] = UPath;
  ret["v.path"] = VPath;
  ret["v.zero.inds"]= vZeroInds_Path;
  ret["lambda.path"] = lambda_path;
  return(ret);
}

// [[Rcpp::export]]
Rcpp::List BICARPL2_VIS(arma::colvec x, int n, int p, double lambda_init,arma::colvec weights_col, arma::colvec weights_row, arma::colvec uinit_row, arma::colvec uinit_col,arma::colvec vinit_row, arma::colvec vinit_col,arma::sp_mat premat_row,arma::sp_mat premat_col, arma::umat IndMat_row, arma::umat IndMat_col,arma::umat EOneIndMat_row, arma::umat EOneIndMat_col, arma::umat ETwoIndMat_row, arma::umat ETwoIndMat_col,double rho = 1, int max_iter = 1e4, int burn_in =50, bool verbose=false,bool verbose_inner=false, double try_tol = 1e-3,int ti = 15, double t_switch = 1.01){

  double t = 1.1;
  int try_iter;

  int cardE_row = EOneIndMat_row.n_rows;
  int cardE_col = EOneIndMat_col.n_rows;


  arma::colvec unew = x;
  arma::colvec uold(n*p);
  arma::colvec ut(n*p);

  arma::colvec vnew_col = vinit_col;
  arma::colvec vold_col(p*cardE_col);
  arma::colvec vnew_row = vinit_row;
  arma::colvec vold_row(n*cardE_row);

  arma::colvec lamnew_col = vnew_col;
  arma::colvec lamold_col(p*cardE_col);
  arma::colvec lamnew_row = vnew_row;
  arma::colvec lamold_row(n*cardE_row);

  arma::colvec proxin_row(n*cardE_row);
  arma::colvec proxin_col(p*cardE_col);

  arma::colvec yt(n*p);
  arma::colvec y(n*p);
  arma::colvec pnew(n*p,arma::fill::zeros);
  arma::colvec pold(n*p);
  arma::colvec pt(n*p);
  arma::colvec qnew(n*p,arma::fill::zeros);
  arma::colvec qold(n*p);

  double lambda = lambda_init;
  double lambda_old = lambda;
  double lambda_try_old;
  double lambda_lower;
  double lambda_upper;
  double try_eps;

  int nzerosold_row = 0;
  int nzerosnew_row = 0;
  int nzerosold_col = 0;
  int nzerosnew_col = 0;



  arma::uword path_iter = 0;
  arma::uword iter = 0;
  arma::mat UPath(n*p,1);
  UPath.col(path_iter)= uinit_col;
  arma::mat VPath_row(n*cardE_row,1);
  VPath_row.col(path_iter)= vinit_row;
  arma::mat VPath_col(p*cardE_col,1);
  VPath_col.col(path_iter)= vinit_col;
  arma::colvec lambda_path(1);
  lambda_path(path_iter) = lambda_init;


  arma::colvec vZeroIndsnew_row(cardE_row,arma::fill::zeros);
  arma::colvec vZeroIndsold_row(cardE_row);
  arma::ucolvec vIdx_row(n);
  arma::colvec vZeroIndsnew_col(cardE_col,arma::fill::zeros);
  arma::colvec vZeroIndsold_col(cardE_col,arma::fill::zeros);
  arma::ucolvec vIdx_col(p);

  arma::mat vZeroIndsPath_row(cardE_row,1,arma::fill::zeros);
  arma::mat vZeroIndsPath_col(cardE_col,1,arma::fill::zeros);


  Rcpp::List ret;
  bool rep_iter;
  while( ((nzerosnew_row < cardE_row) | (nzerosnew_col < cardE_col)) & (iter < max_iter)){
    iter = iter + 1;
    uold = unew;
    vold_row = vnew_row;
    vold_col = vnew_col;
    lamold_row = lamnew_row;
    lamold_col = lamnew_col;

    pold = pnew;
    qold = qnew;

    vZeroIndsold_row = vZeroIndsnew_row;
    vZeroIndsold_col = vZeroIndsnew_col;
    nzerosold_row = nzerosnew_row;
    nzerosold_col = nzerosnew_col;

    pt = arma::vectorise(UnVec(pold,p,n).t());
    ut = arma::vectorise(UnVec(uold,p,n).t());
    rep_iter = true;
    try_iter = 0;
    lambda_upper = lambda;
    lambda_lower = lambda_old;
    lambda_try_old = lambda;
    while(rep_iter){
      try_iter = try_iter + 1;
      ////////////// solve row problem
        // u update
        yt = arma::spsolve(premat_row, (1/rho)*(ut+pt) + (1/rho)*DtMatOpv2(rho*vold_row - lamold_row,p,n,IndMat_row,EOneIndMat_row,ETwoIndMat_row));
        // v update
        proxin_row = DMatOpv2(yt,n,IndMat_row,EOneIndMat_row,ETwoIndMat_row) + (1/rho)*lamold_row;
        vnew_row = ProxL2(proxin_row,n,(1/rho)*weights_row*lambda, IndMat_row);
        // lambda update
        lamnew_row = lamold_row + rho*(DMatOpv2(yt,n,IndMat_row,EOneIndMat_row,ETwoIndMat_row)-vnew_row);
      ////////// end solve row problem

      y = arma::vectorise(UnVec(yt,n,p).t());
      pnew = uold + pold - y;
      ////////////// Solve col problem
        // u update
        unew = arma::spsolve(premat_col, (1/rho)*(y + qold) + (1/rho)*DtMatOpv2(rho*vold_col - lamold_col,n,p,IndMat_col,EOneIndMat_col,ETwoIndMat_col));
        // v update
        proxin_col = DMatOpv2(unew,p,IndMat_col,EOneIndMat_col,ETwoIndMat_col) + (1/rho)*lamold_col;
        vnew_col = ProxL2(proxin_col,p,(1/rho)*weights_col*lambda, IndMat_col);
        // lambda update
        lamnew_col = lamold_col + rho*(DMatOpv2(unew,p,IndMat_col,EOneIndMat_col,ETwoIndMat_col)-vnew_col);
      ////////////// End Solve col problem
      qnew = y + qold - unew;

      for(int l = 0; l<cardE_row; l++){
        vIdx_row = IndMat_row.row(l).t();
        if(sum(vnew_row.elem(vIdx_row)) == 0){
          vZeroIndsnew_row(l) = 1;
        }
      }
      for(int l = 0; l<cardE_col; l++){
        vIdx_col = IndMat_col.row(l).t();
        if(sum(vnew_col.elem(vIdx_col)) == 0){
          vZeroIndsnew_col(l) = 1;
        }
      }
      nzerosnew_row = sum(vZeroIndsnew_row);
      nzerosnew_col = sum(vZeroIndsnew_col);
      if( (nzerosnew_row == nzerosold_row) & (nzerosnew_col == nzerosold_col) & (try_iter == 1)){
        // same sparsity on first try
        // go to next iteration
        rep_iter = false;
      } else if((nzerosnew_row > nzerosold_row + 1) | (nzerosnew_col > nzerosold_col + 1) ){
        // too much sparsity
        // redo iteration
        vZeroIndsnew_col = vZeroIndsold_col;
        vZeroIndsnew_row = vZeroIndsold_row;
        if(try_iter == 1){
          lambda = 0.5*(lambda_lower + lambda_upper);
        } else{
          lambda_upper = lambda;
          lambda = (0.5)*(lambda_lower + lambda_upper);
        }
      } else if( (nzerosnew_row == nzerosold_row) & (nzerosnew_col == nzerosold_col) ){
        // not enough sparsity
        // redo iteration
        vZeroIndsnew_col = vZeroIndsold_col;
        vZeroIndsnew_row = vZeroIndsold_row;
        lambda_lower = lambda;
        lambda = (0.5)*(lambda_lower + lambda_upper);
      } else{
        // just enough sparsity
        rep_iter = false;
      }
      try_eps = std::abs(lambda - lambda_try_old) / std::abs(lambda_try_old);
      if(try_iter > ti){
        rep_iter = false;
      }


    }
    if( (nzerosnew_col > 0) | (nzerosnew_row >0) ){
      t = t_switch;
    }



    if((nzerosnew_row!=nzerosold_row) | (nzerosnew_col!=nzerosold_col)){
      path_iter = path_iter + 1;
      UPath.insert_cols(path_iter,1);
      UPath.col(path_iter) = unew;
      VPath_row.insert_cols(path_iter,1);
      VPath_row.col(path_iter) = vnew_row;
      VPath_col.insert_cols(path_iter,1);
      VPath_col.col(path_iter) = vnew_col;
      lambda_path.insert_rows(path_iter,1);
      lambda_path(path_iter) = lambda;
      lambda_old = lambda;

      vZeroIndsPath_row.insert_cols(path_iter,1);
      vZeroIndsPath_row.col(path_iter) = vZeroIndsnew_row;
      vZeroIndsPath_col.insert_cols(path_iter,1);
      vZeroIndsPath_col.col(path_iter) = vZeroIndsnew_col;
    }
    lambda = lambda*t;

  }
  ret["u.path"] = UPath;
  ret["v.row.path"] = VPath_row;
  ret["v.col.path"] = VPath_col;
  ret["v.row.zero.inds"] = vZeroIndsPath_row;
  ret["v.col.zero.inds"] = vZeroIndsPath_col;
  ret["lambda.path"] = lambda_path;
  return(ret);
}