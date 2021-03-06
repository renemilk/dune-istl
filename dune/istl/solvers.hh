// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:

#ifndef DUNE_SOLVERS_HH
#define DUNE_SOLVERS_HH

#include <cmath>
#include <complex>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>

#include "istlexception.hh"
#include "operators.hh"
#include "scalarproducts.hh"
#include "solver.hh"
#include "preconditioner.hh"
#include <dune/common/timer.hh>
#include <dune/common/ftraits.hh>
#include <dune/common/shared_ptr.hh>
#include <dune/common/static_assert.hh>

namespace Dune {
  /** @defgroup ISTL_Solvers Iterative Solvers
      @ingroup ISTL
   */
  /** @addtogroup ISTL_Solvers
      @{
   */


  /** \file

      \brief   Implementations of the inverse operator interface.

      This file provides various preconditioned Krylov methods.
   */



   //=====================================================================
  // Implementation of this interface
  //=====================================================================

  /*!
     \brief Preconditioned loop solver.

     Implements a preconditioned loop.
     Using this class every Preconditioner can be turned
     into a solver. The solver will apply one preconditioner
     step in each iteration loop.
   */
  template<class X>
  class LoopSolver : public InverseOperator<X,X> {
  public:
    //! \brief The domain type of the operator that we do the inverse for.
    typedef X domain_type;
    //! \brief The range type of the operator that we do the inverse for.
    typedef X range_type;
    //! \brief The field type of the operator that we do the inverse for.
    typedef typename X::field_type field_type;

    /*!
       \brief Set up Loop solver.

       \param op The operator we solve.
       \param prec The preconditioner to apply in each iteration of the loop.
       Has to inherit from Preconditioner.
       \param reduction The relative defect reduction to achieve when applying
       the operator.
       \param maxit The maximum number of iteration steps allowed when applying
       the operator.
       \param verbose The verbosity level.

       Verbose levels are:
       <ul>
       <li> 0 : print nothing </li>
       <li> 1 : print initial and final defect and statistics </li>
       <li> 2 : print line for each iteration </li>
       </ul>
     */
    template<class L, class P>
    LoopSolver (L& op, P& prec,
                double reduction, int maxit, int verbose) :
      ssp(), _op(op), _prec(prec), _sp(ssp), _reduction(reduction), _maxit(maxit), _verbose(verbose)
    {
      dune_static_assert(static_cast<int>(L::category) == static_cast<int>(P::category),
                         "L and P have to have the same category!");
      dune_static_assert(static_cast<int>(L::category) == static_cast<int>(SolverCategory::sequential),
                         "L has to be sequential!");
    }

    /**
        \brief Set up loop solver

        \param op The operator we solve.
        \param sp The scalar product to use, e. g. SeqScalarproduct.
        \param prec The preconditioner to apply in each iteration of the loop.
        Has to inherit from Preconditioner.
        \param reduction The relative defect reduction to achieve when applying
        the operator.
        \param maxit The maximum number of iteration steps allowed when applying
        the operator.
        \param verbose The verbosity level.

        Verbose levels are:
        <ul>
        <li> 0 : print nothing </li>
        <li> 1 : print initial and final defect and statistics </li>
        <li> 2 : print line for each iteration </li>
        </ul>
     */
    template<class L, class S, class P>
    LoopSolver (L& op, S& sp, P& prec,
                double reduction, int maxit, int verbose) :
      _op(op), _prec(prec), _sp(sp), _reduction(reduction), _maxit(maxit), _verbose(verbose)
    {
      dune_static_assert( static_cast<int>(L::category) == static_cast<int>(P::category),
                          "L and P must have the same category!");
      dune_static_assert( static_cast<int>(L::category) == static_cast<int>(S::category),
                          "L and S must have the same category!");
    }


    //! \copydoc InverseOperator::apply(X&,Y&,InverseOperatorResult&)
    virtual void apply (X& x, X& b, InverseOperatorResult& res)
    {
      // clear solver statistics
      res.clear();

      // start a timer
      Timer watch;

      // prepare preconditioner
      _prec.pre(x,b);

      // overwrite b with defect
      _op.applyscaleadd(-1,x,b);

      // compute norm, \todo parallelization
      double def0 = _sp.norm(b);

      // printing
      if (_verbose>0)
      {
        std::cout << "=== LoopSolver" << std::endl;
        if (_verbose>1)
        {
          this->printHeader(std::cout);
          this->printOutput(std::cout,0,def0);
        }
      }

      // allocate correction vector
      X v(x);

      // iteration loop
      int i=1; double def=def0;
      for ( ; i<=_maxit; i++ )
      {
        v = 0;                      // clear correction
        _prec.apply(v,b);           // apply preconditioner
        x += v;                     // update solution
        _op.applyscaleadd(-1,v,b);  // update defect
        double defnew=_sp.norm(b);  // comp defect norm
        if (_verbose>1)             // print
          this->printOutput(std::cout,i,defnew,def);
        //std::cout << i << " " << defnew << " " << defnew/def << std::endl;
        def = defnew;               // update norm
        if (def<def0*_reduction || def<1E-30)    // convergence check
        {
          res.converged  = true;
          break;
        }
      }

      //correct i which is wrong if convergence was not achieved.
      i=std::min(_maxit,i);

      // print
      if (_verbose==1)
        this->printOutput(std::cout,i,def);

      // postprocess preconditioner
      _prec.post(x);

      // fill statistics
      res.iterations = i;
      res.reduction = def/def0;
      res.conv_rate  = pow(res.reduction,1.0/i);
      res.elapsed = watch.elapsed();

      // final print
      if (_verbose>0)
      {
        std::cout << "=== rate=" << res.conv_rate
                  << ", T=" << res.elapsed
                  << ", TIT=" << res.elapsed/i
                  << ", IT=" << i << std::endl;
      }
    }

    //! \copydoc InverseOperator::apply(X&,Y&,double,InverseOperatorResult&)
    virtual void apply (X& x, X& b, double reduction, InverseOperatorResult& res)
    {
      std::swap(_reduction,reduction);
      (*this).apply(x,b,res);
      std::swap(_reduction,reduction);
    }

  private:
    SeqScalarProduct<X> ssp;
    LinearOperator<X,X>& _op;
    Preconditioner<X,X>& _prec;
    ScalarProduct<X>& _sp;
    double _reduction;
    int _maxit;
    int _verbose;
  };


  // all these solvers are taken from the SUMO library
  //! gradient method
  template<class X>
  class GradientSolver : public InverseOperator<X,X> {
  public:
    //! \brief The domain type of the operator that we do the inverse for.
    typedef X domain_type;
    //! \brief The range type of the operator  that we do the inverse for.
    typedef X range_type;
    //! \brief The field type of the operator  that we do the inverse for.
    typedef typename X::field_type field_type;

    /*!
       \brief Set up solver.

       \copydoc LoopSolver::LoopSolver(L&,P&,double,int,int)
     */
    template<class L, class P>
    GradientSolver (L& op, P& prec,
                    double reduction, int maxit, int verbose) :
      ssp(), _op(op), _prec(prec), _sp(ssp), _reduction(reduction), _maxit(maxit), _verbose(verbose)
    {
      dune_static_assert(static_cast<int>(L::category) == static_cast<int>(P::category),
                         "L and P have to have the same category!");
      dune_static_assert(static_cast<int>(L::category) == static_cast<int>(SolverCategory::sequential),
                         "L has to be sequential!");
    }
    /*!
       \brief Set up solver.

       \copydoc LoopSolver::LoopSolver(L&,S&,P&,double,int,int)
     */
    template<class L, class S, class P>
    GradientSolver (L& op, S& sp, P& prec,
                    double reduction, int maxit, int verbose) :
      _op(op), _prec(prec), _sp(sp), _reduction(reduction), _maxit(maxit), _verbose(verbose)
    {
      dune_static_assert(static_cast<int>(L::category) == static_cast<int>(P::category),
                         "L and P have to have the same category!");
      dune_static_assert(static_cast<int>(L::category) == static_cast<int>(S::category),
                         "L and S have to have the same category!");
    }

    /*!
       \brief Apply inverse operator.

       \copydoc InverseOperator::apply(X&,Y&,InverseOperatorResult&)
     */
    virtual void apply (X& x, X& b, InverseOperatorResult& res)
    {
      res.clear();                  // clear solver statistics
      Timer watch;                // start a timer
      _prec.pre(x,b);             // prepare preconditioner
      _op.applyscaleadd(-1,x,b);  // overwrite b with defect

      X p(x);                     // create local vectors
      X q(b);

      double def0 = _sp.norm(b); // compute norm

      if (_verbose>0)             // printing
      {
        std::cout << "=== GradientSolver" << std::endl;
        if (_verbose>1)
        {
          this->printHeader(std::cout);
          this->printOutput(std::cout,0,def0);
        }
      }

      int i=1; double def=def0;   // loop variables
      field_type lambda;
      for ( ; i<=_maxit; i++ )
      {
        p = 0;                      // clear correction
        _prec.apply(p,b);           // apply preconditioner
        _op.apply(p,q);             // q=Ap
        lambda = _sp.dot(p,b)/_sp.dot(q,p); // minimization
        x.axpy(lambda,p);           // update solution
        b.axpy(-lambda,q);          // update defect

        double defnew=_sp.norm(b); // comp defect norm
        if (_verbose>1)             // print
          this->printOutput(std::cout,i,defnew,def);

        def = defnew;               // update norm
        if (def<def0*_reduction || def<1E-30)    // convergence check
        {
          res.converged  = true;
          break;
        }
      }

      //correct i which is wrong if convergence was not achieved.
      i=std::min(_maxit,i);

      if (_verbose==1)                // printing for non verbose
        this->printOutput(std::cout,i,def);

      _prec.post(x);                  // postprocess preconditioner
      res.iterations = i;               // fill statistics
      res.reduction = def/def0;
      res.conv_rate  = pow(res.reduction,1.0/i);
      res.elapsed = watch.elapsed();
      if (_verbose>0)                 // final print
        std::cout << "=== rate=" << res.conv_rate
                  << ", T=" << res.elapsed
                  << ", TIT=" << res.elapsed/i
                  << ", IT=" << i << std::endl;
    }

    /*!
       \brief Apply inverse operator with given reduction factor.

       \copydoc InverseOperator::apply(X&,Y&,double,InverseOperatorResult&)
     */
    virtual void apply (X& x, X& b, double reduction, InverseOperatorResult& res)
    {
      std::swap(_reduction,reduction);
      (*this).apply(x,b,res);
      std::swap(_reduction,reduction);
    }

  private:
    SeqScalarProduct<X> ssp;
    LinearOperator<X,X>& _op;
    Preconditioner<X,X>& _prec;
    ScalarProduct<X>& _sp;
    double _reduction;
    int _maxit;
    int _verbose;
  };



  //! \brief conjugate gradient method
  template<class X>
  class CGSolver : public InverseOperator<X,X> {
  public:
    //! \brief The domain type of the operator to be inverted.
    typedef X domain_type;
    //! \brief The range type of the operator to be inverted.
    typedef X range_type;
    //! \brief The field type of the operator to be inverted.
    typedef typename X::field_type field_type;

    /*!
       \brief Set up conjugate gradient solver.

       \copydoc LoopSolver::LoopSolver(L&,P&,double,int,int)
     */
    template<class L, class P>
    CGSolver (L& op, P& prec, double reduction, int maxit, int verbose) :
      ssp(), _op(op), _prec(prec), _sp(ssp), _reduction(reduction), _maxit(maxit), _verbose(verbose)
    {
      dune_static_assert( static_cast<int>(L::category) == static_cast<int>(P::category),
                          "L and P must have the same category!");
      dune_static_assert( static_cast<int>(L::category) == static_cast<int>(SolverCategory::sequential),
                          "L must be sequential!");
    }
    /*!
       \brief Set up conjugate gradient solver.

       \copydoc LoopSolver::LoopSolver(L&,S&,P&,double,int,int)
     */
    template<class L, class S, class P>
    CGSolver (L& op, S& sp, P& prec, double reduction, int maxit, int verbose) :
      _op(op), _prec(prec), _sp(sp), _reduction(reduction), _maxit(maxit), _verbose(verbose)
    {
      dune_static_assert( static_cast<int>(L::category) == static_cast<int>(P::category),
                          "L and P must have the same category!");
      dune_static_assert( static_cast<int>(L::category) == static_cast<int>(S::category),
                          "L and S must have the same category!");
    }

    /*!
       \brief Apply inverse operator.

       \copydoc InverseOperator::apply(X&,Y&,InverseOperatorResult&)
     */
    virtual void apply (X& x, X& b, InverseOperatorResult& res)
    {
      res.clear();                  // clear solver statistics
      Timer watch;                // start a timer
      _prec.pre(x,b);             // prepare preconditioner
      _op.applyscaleadd(-1,x,b);  // overwrite b with defect

      X p(x);              // the search direction
      X q(x);              // a temporary vector

      double def0 = _sp.norm(b); // compute norm
      if (def0<1E-30)    // convergence check
      {
        res.converged  = true;
        res.iterations = 0;               // fill statistics
        res.reduction = 0;
        res.conv_rate  = 0;
        res.elapsed=0;
        if (_verbose>0)                 // final print
          std::cout << "=== rate=" << res.conv_rate
                    << ", T=" << res.elapsed << ", TIT=" << res.elapsed
                    << ", IT=0" << std::endl;
        return;
      }

      if (_verbose>0)             // printing
      {
        std::cout << "=== CGSolver" << std::endl;
        if (_verbose>1) {
          this->printHeader(std::cout);
          this->printOutput(std::cout,0,def0);
        }
      }

      // some local variables
      double def=def0;   // loop variables
      field_type rho,rholast,lambda,alpha,beta;

      // determine initial search direction
      p = 0;                          // clear correction
      _prec.apply(p,b);               // apply preconditioner
      rholast = _sp.dot(p,b);         // orthogonalization

      // the loop
      int i=1;
      for ( ; i<=_maxit; i++ )
      {
        // minimize in given search direction p
        _op.apply(p,q);             // q=Ap
        alpha = _sp.dot(p,q);       // scalar product
        lambda = rholast/alpha;     // minimization
        x.axpy(lambda,p);           // update solution
        b.axpy(-lambda,q);          // update defect

        // convergence test
        double defnew=_sp.norm(b); // comp defect norm

        if (_verbose>1)             // print
          this->printOutput(std::cout,i,defnew,def);

        def = defnew;               // update norm
        if (def<def0*_reduction || def<1E-30)    // convergence check
        {
          res.converged  = true;
          break;
        }

        // determine new search direction
        q = 0;                      // clear correction
        _prec.apply(q,b);           // apply preconditioner
        rho = _sp.dot(q,b);         // orthogonalization
        beta = rho/rholast;         // scaling factor
        p *= beta;                  // scale old search direction
        p += q;                     // orthogonalization with correction
        rholast = rho;              // remember rho for recurrence
      }

      //correct i which is wrong if convergence was not achieved.
      i=std::min(_maxit,i);

      if (_verbose==1)                // printing for non verbose
        this->printOutput(std::cout,i,def);

      _prec.post(x);                  // postprocess preconditioner
      res.iterations = i;               // fill statistics
      res.reduction = def/def0;
      res.conv_rate  = pow(res.reduction,1.0/i);
      res.elapsed = watch.elapsed();

      if (_verbose>0)                 // final print
      {
        std::cout << "=== rate=" << res.conv_rate
                  << ", T=" << res.elapsed
                  << ", TIT=" << res.elapsed/i
                  << ", IT=" << i << std::endl;
      }
    }

    /*!
       \brief Apply inverse operator with given reduction factor.

       \copydoc InverseOperator::apply(X&,Y&,double,InverseOperatorResult&)
     */
    virtual void apply (X& x, X& b, double reduction,
                        InverseOperatorResult& res)
    {
      std::swap(_reduction,reduction);
      (*this).apply(x,b,res);
      std::swap(_reduction,reduction);
    }

  private:
    SeqScalarProduct<X> ssp;
    LinearOperator<X,X>& _op;
    Preconditioner<X,X>& _prec;
    ScalarProduct<X>& _sp;
    double _reduction;
    int _maxit;
    int _verbose;
  };


  // Ronald Kriemanns BiCG-STAB implementation from Sumo
  //! \brief Bi-conjugate Gradient Stabilized (BiCG-STAB)
  template<class X>
  class BiCGSTABSolver : public InverseOperator<X,X> {
  public:
    //! \brief The domain type of the operator to be inverted.
    typedef X domain_type;
    //! \brief The range type of the operator to be inverted.
    typedef X range_type;
    //! \brief The field type of the operator to be inverted
    typedef typename X::field_type field_type;
    //! \brief The real type of the field type (is the same of using real numbers, but differs for std::complex)
    typedef typename FieldTraits<field_type>::real_type real_type;

    /*!
       \brief Set up solver.

       \copydoc LoopSolver::LoopSolver(L&,P&,double,int,int)
     */
    template<class L, class P>
    BiCGSTABSolver (L& op, P& prec,
                    double reduction, int maxit, int verbose) :
      ssp(), _op(op), _prec(prec), _sp(ssp), _reduction(reduction), _maxit(maxit), _verbose(verbose)
    {
      dune_static_assert(static_cast<int>(L::category) == static_cast<int>(P::category), "L and P must be of the same category!");
      dune_static_assert(static_cast<int>(L::category) == static_cast<int>(SolverCategory::sequential), "L must be sequential!");
    }
    /*!
       \brief Set up solver.

       \copydoc LoopSolver::LoopSolver(L&,S&,P&,double,int,int)
     */
    template<class L, class S, class P>
    BiCGSTABSolver (L& op, S& sp, P& prec,
                    double reduction, int maxit, int verbose) :
      _op(op), _prec(prec), _sp(sp), _reduction(reduction), _maxit(maxit), _verbose(verbose)
    {
      dune_static_assert( static_cast<int>(L::category) == static_cast<int>(P::category),
                          "L and P must have the same category!");
      dune_static_assert( static_cast<int>(L::category) == static_cast<int>(S::category),
                          "L and S must have the same category!");
    }

    /*!
       \brief Apply inverse operator.

       \copydoc InverseOperator::apply(X&,Y&,InverseOperatorResult&)
     */
    virtual void apply (X& x, X& b, InverseOperatorResult& res)
    {
      const double EPSILON=1e-80;
      double it;
      field_type rho, rho_new, alpha, beta, h, omega;
      real_type norm, norm_old, norm_0;

      //
      // get vectors and matrix
      //
      X& r=b;
      X p(x);
      X v(x);
      X t(x);
      X y(x);
      X rt(x);

      //
      // begin iteration
      //

      // r = r - Ax; rt = r
      res.clear();                // clear solver statistics
      Timer watch;                // start a timer
      _prec.pre(x,r);             // prepare preconditioner
      _op.applyscaleadd(-1,x,r);  // overwrite b with defect

      rt=r;

      norm = norm_old = norm_0 = _sp.norm(r);

      p=0;
      v=0;

      rho   = 1;
      alpha = 1;
      omega = 1;

      if (_verbose>0)             // printing
      {
        std::cout << "=== BiCGSTABSolver" << std::endl;
        if (_verbose>1)
        {
          this->printHeader(std::cout);
          this->printOutput(std::cout,0,norm_0);
          //std::cout << " Iter       Defect         Rate" << std::endl;
          //std::cout << "    0" << std::setw(14) << norm_0 << std::endl;
        }
      }

      if ( norm < (_reduction * norm_0)  || norm<1E-30)
      {
        res.converged = 1;
        _prec.post(x);                  // postprocess preconditioner
        res.iterations = 0;             // fill statistics
        res.reduction = 0;
        res.conv_rate  = 0;
        res.elapsed = watch.elapsed();
        return;
      }

      //
      // iteration
      //

      for (it = 0.5; it < _maxit; it+=.5)
      {
        //
        // preprocess, set vecsizes etc.
        //

        // rho_new = < rt , r >
        rho_new = _sp.dot(rt,r);

        // look if breakdown occured
        if (std::abs(rho) <= EPSILON)
          DUNE_THROW(ISTLError,"breakdown in BiCGSTAB - rho "
                     << rho << " <= EPSILON " << EPSILON
                     << " after " << it << " iterations");
        if (std::abs(omega) <= EPSILON)
          DUNE_THROW(ISTLError,"breakdown in BiCGSTAB - omega "
                     << omega << " <= EPSILON " << EPSILON
                     << " after " << it << " iterations");


        if (it<1)
          p = r;
        else
        {
          beta = ( rho_new / rho ) * ( alpha / omega );
          p.axpy(-omega,v); // p = r + beta (p - omega*v)
          p *= beta;
          p += r;
        }

        // y = W^-1 * p
        y = 0;
        _prec.apply(y,p);           // apply preconditioner

        // v = A * y
        _op.apply(y,v);

        // alpha = rho_new / < rt, v >
        h = _sp.dot(rt,v);

        if ( std::abs(h) < EPSILON )
          DUNE_THROW(ISTLError,"h=0 in BiCGSTAB");

        alpha = rho_new / h;

        // apply first correction to x
        // x <- x + alpha y
        x.axpy(alpha,y);

        // r = r - alpha*v
        r.axpy(-alpha,v);

        //
        // test stop criteria
        //

        norm = _sp.norm(r);

        if (_verbose>1) // print
        {
          this->printOutput(std::cout,it,norm,norm_old);
        }

        if ( norm < (_reduction * norm_0) )
        {
          res.converged = 1;
          break;
        }
        it+=.5;

        norm_old = norm;

        // y = W^-1 * r
        y = 0;
        _prec.apply(y,r);

        // t = A * y
        _op.apply(y,t);

        // omega = < t, r > / < t, t >
        omega = _sp.dot(t,r)/_sp.dot(t,t);

        // apply second correction to x
        // x <- x + omega y
        x.axpy(omega,y);

        // r = s - omega*t (remember : r = s)
        r.axpy(-omega,t);

        rho = rho_new;

        //
        // test stop criteria
        //

        norm = _sp.norm(r);

        if (_verbose > 1)             // print
        {
          this->printOutput(std::cout,it,norm,norm_old);
        }

        if ( norm < (_reduction * norm_0)  || norm<1E-30)
        {
          res.converged = 1;
          break;
        }

        norm_old = norm;
      } // end for

      //correct i which is wrong if convergence was not achieved.
      it=std::min((double)_maxit,it);

      if (_verbose==1)                // printing for non verbose
        this->printOutput(std::cout,it,norm);

      _prec.post(x);                  // postprocess preconditioner
      res.iterations = static_cast<int>(std::ceil(it));              // fill statistics
      res.reduction = norm/norm_0;
      res.conv_rate  = pow(res.reduction,1.0/it);
      res.elapsed = watch.elapsed();
      if (_verbose>0)                 // final print
        std::cout << "=== rate=" << res.conv_rate
                  << ", T=" << res.elapsed
                  << ", TIT=" << res.elapsed/it
                  << ", IT=" << it << std::endl;
    }

    /*!
       \brief Apply inverse operator with given reduction factor.

       \copydoc InverseOperator::apply(X&,Y&,double,InverseOperatorResult&)
     */
    virtual void apply (X& x, X& b, double reduction, InverseOperatorResult& res)
    {
      std::swap(_reduction,reduction);
      (*this).apply(x,b,res);
      std::swap(_reduction,reduction);
    }

  private:
    SeqScalarProduct<X> ssp;
    LinearOperator<X,X>& _op;
    Preconditioner<X,X>& _prec;
    ScalarProduct<X>& _sp;
    double _reduction;
    int _maxit;
    int _verbose;
  };

  /*! \brief Minimal Residual Method (MINRES)

     Symmetrically Preconditioned MINRES as in A. Greenbaum, 'Iterative Methods for Solving Linear Systems', pp. 121
     Iterative solver for symmetric indefinite operators.
     Note that in order to ensure the (symmetrically) preconditioned system to remain symmetric, the preconditioner has to be spd.
   */
  template<class X>
  class MINRESSolver : public InverseOperator<X,X> {
  public:
    //! \brief The domain type of the operator to be inverted.
    typedef X domain_type;
    //! \brief The range type of the operator to be inverted.
    typedef X range_type;
    //! \brief The field type of the operator to be inverted.
    typedef typename X::field_type field_type;
    //! \brief The real type of the field type (is the same of using real numbers, but differs for std::complex)
    typedef typename FieldTraits<field_type>::real_type real_type;

    /*!
       \brief Set up MINRES solver.

       \copydoc LoopSolver::LoopSolver(L&,P&,double,int,int)
     */
    template<class L, class P>
    MINRESSolver (L& op, P& prec, double reduction, int maxit, int verbose) :
      ssp(), _op(op), _prec(prec), _sp(ssp), _reduction(reduction), _maxit(maxit), _verbose(verbose)
    {
      dune_static_assert( static_cast<int>(L::category) == static_cast<int>(P::category),
                          "L and P must have the same category!");
      dune_static_assert( static_cast<int>(L::category) == static_cast<int>(SolverCategory::sequential),
                          "L must be sequential!");
    }
    /*!
       \brief Set up MINRES solver.

       \copydoc LoopSolver::LoopSolver(L&,S&,P&,double,int,int)
     */
    template<class L, class S, class P>
    MINRESSolver (L& op, S& sp, P& prec, double reduction, int maxit, int verbose) :
      _op(op), _prec(prec), _sp(sp), _reduction(reduction), _maxit(maxit), _verbose(verbose)
    {
      dune_static_assert( static_cast<int>(L::category) == static_cast<int>(P::category),
                          "L and P must have the same category!");
      dune_static_assert( static_cast<int>(L::category) == static_cast<int>(S::category),
                          "L and S must have the same category!");
    }

    /*!
       \brief Apply inverse operator.

       \copydoc InverseOperator::apply(X&,Y&,InverseOperatorResult&)
     */
    virtual void apply (X& x, X& b, InverseOperatorResult& res)
    {
      res.clear();                // clear solver statistics
      Timer watch;                // start a timer
      _prec.pre(x,b);             // prepare preconditioner
      _op.applyscaleadd(-1,x,b);  // overwrite b with defect/residual

      real_type def0 = _sp.norm(b);   // compute residual norm

      if (def0<1E-30)    // convergence check
      {
        res.converged  = true;
        res.iterations = 0;               // fill statistics
        res.reduction = 0;
        res.conv_rate  = 0;
        res.elapsed=0;
        if (_verbose>0)                 // final print
          std::cout << "=== rate=" << res.conv_rate << ", T=" << res.elapsed << ", TIT=" << res.elapsed << ", IT=0" << std::endl;
        return;
      }

      if (_verbose>0)             // printing
      {
        std::cout << "=== MINRESSolver" << std::endl;
        if (_verbose>1) {
          this->printHeader(std::cout);
          this->printOutput(std::cout,0,def0);
        }
      }

      // some local variables
      real_type def=def0;                 // the defect/residual norm
      field_type alpha,                   // recurrence coefficients as computed in the Lanczos alg making up the matrix T
                 c[2]={0.0, 0.0}, // diagonal entry of Givens rotation
                 s[2]={0.0, 0.0}; // off-diagonal entries of Givens rotation
      real_type beta;

      field_type T[3]={0.0, 0.0, 0.0};      // recurrence coefficients (column k of Matrix T)

      X z(b.size()),        // some temporary vectors
      dummy(b.size());

      field_type xi[2]={1.0, 0.0};

      // initialize
      z = 0.0;                  // clear correction

      _prec.apply(z,b);         // apply preconditioner z=M^-1*b

      beta = std::sqrt(std::abs(_sp.dot(z,b)));
      real_type beta0 = beta;

      X p[3];       // the search directions
      X q[3];       // Orthonormal basis vectors (in unpreconditioned case)

      q[0].resize(b.size());
      q[1].resize(b.size());
      q[2].resize(b.size());
      q[0] = 0.0;
      q[1] = b;
      q[1] /= beta;
      q[2] = 0.0;

      p[0].resize(b.size());
      p[1].resize(b.size());
      p[2].resize(b.size());
      p[0] = 0.0;
      p[1] = 0.0;
      p[2] = 0.0;


      z /= beta;        // this is w_current

      // the loop
      int i=1;
      for ( ; i<=_maxit; i++)
      {
        dummy = z;   // remember z_old for the computation of the search direction p in the next iteration

        int i1 = i%3,
            i0 = (i1+2)%3,
            i2 = (i1+1)%3;

        // Symmetrically Preconditioned Lanczos (Greenbaum p.121)
        _op.apply(z,q[i2]);               // q[i2] = Az
        q[i2].axpy(-beta, q[i0]);
        alpha = _sp.dot(q[i2],z);
        q[i2].axpy(-alpha, q[i1]);

        z=0.0;
        _prec.apply(z,q[i2]);

        beta = std::sqrt(std::abs(_sp.dot(q[i2],z)));

        q[i2] /= beta;
        z /= beta;

        // QR Factorization of recurrence coefficient matrix
        // apply previous Givens rotations to last column of T
        T[1] = T[2];
        if (i>2)
        {
          T[0] = s[i%2]*T[1];
          T[1] = c[i%2]*T[1];
        }
        if (i>1)
        {
          T[2] = c[(i+1)%2]*alpha - s[(i+1)%2]*T[1];
          T[1] = c[(i+1)%2]*T[1] + s[(i+1)%2]*alpha;
        }
        else
          T[2] = alpha;

        // recompute c, s -> current Givens rotation \TODO use BLAS-routine drotg instead for greater robustness
        //          cblas_drotg (a, b, c, s);
        c[i%2] = 1.0/std::sqrt(T[2]*T[2] + beta*beta);
        s[i%2] = beta*c[i%2];
        c[i%2] *= T[2];

        // apply current Givens rotation to T eliminating the last entry...
        T[2] = c[i%2]*T[2] + s[i%2]*beta;

        // ...and to xi, the right hand side of the least squares problem min_y||beta*xi-T*y||
        xi[i%2] = -s[i%2]*xi[(i+1)%2];
        xi[(i+1)%2] *= c[i%2];

        // compute correction direction
        p[i2] = dummy;
        p[i2].axpy(-T[1],p[i1]);
        p[i2].axpy(-T[0],p[i0]);
        p[i2] /= T[2];

        // apply correction/update solution
        x.axpy(beta0*xi[(i+1)%2], p[i2]);

        // remember beta_old
        T[2] = beta;

        // update residual - not necessary if in the preconditioned case we are content with the residual norm of the
        // preconditioned system as convergence test
        //          _op.apply(p[i2],dummy);
        //          b.axpy(-beta0*xi[(i+1)%2],dummy);

        //          convergence test
        real_type defnew = std::abs(beta0*xi[i%2]);   // the last entry the QR-transformed least squares RHS is the new residual norm

        if (_verbose>1)               // print
          this->printOutput(std::cout,i,defnew,def);

        def = defnew;                 // update norm
        if (def<def0*_reduction || def<1E-30 || i==_maxit)      // convergence check
        {
          res.converged  = true;
          break;
        }
      }

      //correct i which is wrong if convergence was not achieved.
      i=std::min(_maxit,i);

      if (_verbose==1)                  // printing for non verbose
        this->printOutput(std::cout,i,def);

      _prec.post(x);                    // postprocess preconditioner
      res.iterations = i;                 // fill statistics
      res.reduction = def/def0;
      res.conv_rate  = pow(res.reduction,1.0/i);
      res.elapsed = watch.elapsed();

      if (_verbose>0)                   // final print
      {
        std::cout << "=== rate=" << res.conv_rate
                  << ", T=" << res.elapsed
                  << ", TIT=" << res.elapsed/i
                  << ", IT=" << i << std::endl;
      }

    }

    /*!
       \brief Apply inverse operator with given reduction factor.

       \copydoc InverseOperator::apply(X&,Y&,double,InverseOperatorResult&)
     */
    virtual void apply (X& x, X& b, double reduction, InverseOperatorResult& res)
    {
      std::swap(_reduction,reduction);
      (*this).apply(x,b,res);
      std::swap(_reduction,reduction);
    }

  private:
    SeqScalarProduct<X> ssp;
    LinearOperator<X,X>& _op;
    Preconditioner<X,X>& _prec;
    ScalarProduct<X>& _sp;
    double _reduction;
    int _maxit;
    int _verbose;
  };

  /**
     \brief implements the Generalized Minimal Residual (GMRes) method

     GMRes solves the unsymmetric linear system Ax = b using the
     Generalized Minimal Residual method as described the SIAM Templates
     book (http://www.netlib.org/templates/templates.pdf).

     \todo construct F via rebind and an appropriate field_type

   */

  template<class X, class Y=X, class F = Y>
  class RestartedGMResSolver : public InverseOperator<X,Y>
  {
  public:
    //! \brief The domain type of the operator to be inverted.
    typedef X domain_type;
    //! \brief The range type of the operator to be inverted.
    typedef Y range_type;
    //! \brief The field type of the operator to be inverted
    typedef typename X::field_type field_type;
    //! \brief The real type of the field type (is the same of using real numbers, but differs for std::complex)
    typedef typename FieldTraits<field_type>::real_type real_type;
    //! \brief The field type of the basis vectors
    typedef F basis_type;

    /*!
       \brief Set up solver.

       \copydoc LoopSolver::LoopSolver(L&,P&,double,int,int)
       \param restart number of GMRes cycles before restart
       \param recalc_defect recalculate the defect after everey restart or not [default=false]
     */
    template<class L, class P>
    RestartedGMResSolver (L& op, P& prec, double reduction, int restart, int maxit, int verbose, bool recalc_defect = false) :
      _A_(op), _M(prec),
      ssp(), _sp(ssp), _restart(restart),
      _reduction(reduction), _maxit(maxit), _verbose(verbose),
      _recalc_defect(recalc_defect)
    {
      dune_static_assert(static_cast<int>(P::category) == static_cast<int>(L::category),
                         "P and L must be the same category!");
      dune_static_assert( static_cast<int>(L::category) == static_cast<int>(SolverCategory::sequential),
                          "L must be sequential!");
    }

    /*!
       \brief Set up solver.

       \copydoc LoopSolver::LoopSolver(L&,S&,P&,double,int,int)
       \param restart number of GMRes cycles before restart
       \param recalc_defect recalculate the defect after everey restart or not [default=false]
     */
    template<class L, class S, class P>
    RestartedGMResSolver (L& op, S& sp, P& prec, double reduction, int restart, int maxit, int verbose, bool recalc_defect = false) :
      _A_(op), _M(prec),
      _sp(sp), _restart(restart),
      _reduction(reduction), _maxit(maxit), _verbose(verbose),
      _recalc_defect(recalc_defect)
    {
      dune_static_assert(static_cast<int>(P::category) == static_cast<int>(L::category),
                         "P and L must have the same category!");
      dune_static_assert(static_cast<int>(P::category) == static_cast<int>(S::category),
                         "P and S must have the same category!");
    }

    //! \copydoc InverseOperator::apply(X&,Y&,InverseOperatorResult&)
    virtual void apply (X& x, X& b, InverseOperatorResult& res)
    {
      apply(x,b,_reduction,res);
    }

    /*!
       \brief Apply inverse operator.

       \copydoc InverseOperator::apply(X&,Y&,double,InverseOperatorResult&)
     */
    virtual void apply (X& x, Y& b, double reduction, InverseOperatorResult& res)
    {
      int m = _restart;
      real_type norm;
      real_type norm_old = 0.0;
      real_type norm_0;
      real_type beta;
      int i, j = 1, k;
      std::vector<field_type> s(m+1), cs(m), sn(m);
      // helper vector
      X w(b);
      std::vector< std::vector<field_type> > H(m+1,s);
      std::vector<F> v(m+1,b);

      // start timer
      Timer watch;                // start a timer

      // clear solver statistics
      res.clear();
      _M.pre(x,b);
      if (_recalc_defect)
      {
        // norm_0 = norm(M^-1 b)
        w = 0.0; _M.apply(w,b); // w = M^-1 b
        norm_0 = _sp.norm(w); // use defect of preconditioned residual
        // r = _M.solve(b - A * x);
        w = b;
        _A_.applyscaleadd(-1,x, /* => */ w); // w = b - Ax;
        v[0] = 0.0; _M.apply(v[0],w); // r = M^-1 w
        beta = _sp.norm(v[0]);
      }
      else
      {
        // norm_0 = norm(b-Ax)
        _A_.applyscaleadd(-1,x, /* => */ b); // b = b - Ax;
        v[0] = 0.0; _M.apply(v[0],b); // r = M^-1 b
        beta = _sp.norm(v[0]);
        norm_0 = beta; // use defect of preconditioned residual
      }

      // avoid division by zero
      if (norm_0 == 0.0)
        norm_0 = 1.0;
      norm = norm_old = beta;

      // print header
      if (_verbose > 0)
      {
        std::cout << "=== RestartedGMResSolver" << std::endl;
        if (_verbose > 1)
        {
          this->printHeader(std::cout);
          this->printOutput(std::cout,0,norm_0);
          this->printOutput(std::cout,0,norm, norm_0);
        }
      }

      // check convergence
      if (norm <= reduction * norm_0) {
        _M.post(x);                  // postprocess preconditioner
        res.converged  = true;
        if (_verbose > 0)                 // final print
          print_result(res);
        return;
      }

      while (j <= _maxit && res.converged != true) {
        v[0] *= (1.0 / beta);
        for (i=1; i<=m; i++) s[i] = 0.0;
        s[0] = beta;

        for (i = 0; i < m && j <= _maxit && res.converged != true; i++, j++) {
          w = 0.0;
          v[i+1] = 0.0; // use v[i+1] as temporary vector
          _A_.apply(v[i], /* => */ v[i+1]);
          _M.apply(w, v[i+1]);
          for (k = 0; k <= i; k++) {
            H[k][i] = _sp.dot(w, v[k]);
            // w -= H[k][i] * v[k];
            w.axpy(-H[k][i], v[k]);
          }
          H[i+1][i] = _sp.norm(w);
          if (H[i+1][i] == 0.0)
            DUNE_THROW(ISTLError,"breakdown in GMRes - |w| "
                       << " == 0.0 after " << j << " iterations");
          // v[i+1] = w * (1.0 / H[i+1][i]);
          v[i+1] = w; v[i+1] *= (1.0 / H[i+1][i]);

          for (k = 0; k < i; k++)
            applyPlaneRotation(H[k][i], H[k+1][i], cs[k], sn[k]);

          generatePlaneRotation(H[i][i], H[i+1][i], cs[i], sn[i]);
          applyPlaneRotation(H[i][i], H[i+1][i], cs[i], sn[i]);
          applyPlaneRotation(s[i], s[i+1], cs[i], sn[i]);

          norm = std::abs(s[i+1]);

          if (_verbose > 1)             // print
          {
            this->printOutput(std::cout,j,norm,norm_old);
          }

          norm_old = norm;

          if (norm < reduction * norm_0) {
            res.converged = true;
          }
        }

        if (_recalc_defect)
        {
          // update x
          update(x, i - 1, H, s, v);

          // update residuum
          // r = M^-1 (b - A * x);
          w = b; _A_.applyscaleadd(-1,x, /* => */ w);
          _M.apply(v[0], w);
          beta = _sp.norm(v[0]);
          norm = beta;
        }
        else
        {
          // calc update vector
          w = 0;
          update(w, i - 1, H, s, v);

          // update x
          x += w;

          // r = M^-1 (b - A * x);
          // update defect
          _A_.applyscaleadd(-1,w, /* => */ b);
          // r = M^-1 (b - A * x);
          v[0] = 0.0; _M.apply(v[0],b); // r = M^-1 b
          beta = _sp.norm(v[0]);
          norm = beta;

          res.converged = false;
        }

        //correct i which is wrong if convergence was not achieved.
        j=std::min(_maxit,j);

        if (_verbose > 1)             // print
        {
          this->printOutput(std::cout,j,norm,norm_old);
        }

        norm_old = norm;

        if (norm < reduction * norm_0) {
          // fill statistics
          res.converged = true;
        }

        if (res.converged != true && _verbose > 0)
          std::cout << "=== GMRes::restart\n";
      }

      _M.post(x);                  // postprocess preconditioner

      res.iterations = j;
      res.reduction = norm / norm_0;
      res.conv_rate  = pow(res.reduction,1.0/j);
      res.elapsed = watch.elapsed();

      if (_verbose>0)
        print_result(res);
    }
  private:

    void
    print_result (const InverseOperatorResult & res) const
    {
      int j = res.iterations>0 ? res.iterations : 1;
      std::cout << "=== rate=" << res.conv_rate
                << ", T=" << res.elapsed
                << ", TIT=" << res.elapsed/j
                << ", IT=" << res.iterations
                << std::endl;
    }

    static void
    update(X &x, int k,
           std::vector< std::vector<field_type> > & h,
           std::vector<field_type> & s, std::vector<F> v)
    {
      std::vector<field_type> y(s);

      // Backsolve:
      for (int i = k; i >= 0; i--) {
        y[i] /= h[i][i];
        for (int j = i - 1; j >= 0; j--)
          y[j] -= h[j][i] * y[i];
      }

      for (int j = 0; j <= k; j++)
        // x += v[j] * y[j];
        x.axpy(y[j],v[j]);
    }

    void
    generatePlaneRotation(field_type &dx, field_type &dy, field_type &cs, field_type &sn)
    {
      if (dy == 0.0) {
        cs = 1.0;
        sn = 0.0;
      } else if (std::abs(dy) > std::abs(dx)) {
        field_type temp = dx / dy;
        sn = 1.0 / std::sqrt( 1.0 + temp*temp );
        cs = temp * sn;
      } else {
        field_type temp = dy / dx;
        cs = 1.0 / std::sqrt( 1.0 + temp*temp );
        sn = temp * cs;
      }
    }


    void
    applyPlaneRotation(field_type &dx, field_type &dy, field_type &cs, field_type &sn)
    {
      field_type temp  =  cs * dx + sn * dy;
      dy = -sn * dx + cs * dy;
      dx = temp;
    }

    LinearOperator<X,X>& _A_;
    Preconditioner<X,X>& _M;
    SeqScalarProduct<X> ssp;
    ScalarProduct<X>& _sp;
    int _restart;
    double _reduction;
    int _maxit;
    int _verbose;
    bool _recalc_defect;
  };


  /**
   * @brief Generalized preconditioned conjugate gradient solver.
   *
   * A preconditioned conjugate gradient that allows
   * the preconditioner to change between iterations.
   *
   * One example for such preconditioner is AMG when used without
   * a direct coarse solver. In this case the number of iterations
   * performed on the coarsest level might change between applications.
   *
   * In contrast to CGSolver the search directions are stored and
   * the orthogonalization is done explicitly.
   */
  template<class X>
  class GeneralizedPCGSolver : public InverseOperator<X,X>
  {
  public:
    //! \brief The domain type of the operator to be inverted.
    typedef X domain_type;
    //! \brief The range type of the operator to be inverted.
    typedef X range_type;
    //! \brief The field type of the operator to be inverted.
    typedef typename X::field_type field_type;

    /*!
       \brief Set up nonlinear preconditioned conjugate gradient solver.

       \copydoc LoopSolver::LoopSolver(L&,P&,double,int,int)
       \param restart When to restart the construction of
       the Krylov search space.
     */
    template<class L, class P>
    GeneralizedPCGSolver (L& op, P& prec, double reduction, int maxit, int verbose,
                          int restart=10) :
      ssp(), _op(op), _prec(prec), _sp(ssp), _reduction(reduction), _maxit(maxit),
      _verbose(verbose), _restart(std::min(maxit,restart))
    {
      dune_static_assert(static_cast<int>(L::category) == static_cast<int>(P::category),
                         "L and P have to have the same category!");
      dune_static_assert(static_cast<int>(L::category) ==
                         static_cast<int>(SolverCategory::sequential),
                         "L has to be sequential!");
    }
    /*!
       \brief Set up nonlinear preconditioned conjugate gradient solver.

       \copydoc LoopSolver::LoopSolver(L&,S&,P&,double,int,int)
       \param restart When to restart the construction of
       the Krylov search space.
     */
    template<class L, class P, class S>
    GeneralizedPCGSolver (L& op, S& sp, P& prec,
                          double reduction, int maxit, int verbose, int restart=10) :
      _op(op), _prec(prec), _sp(sp), _reduction(reduction), _maxit(maxit), _verbose(verbose),
      _restart(std::min(maxit,restart))
    {
      dune_static_assert( static_cast<int>(L::category) == static_cast<int>(P::category),
                          "L and P must have the same category!");
      dune_static_assert( static_cast<int>(L::category) == static_cast<int>(S::category),
                          "L and S must have the same category!");
    }
    /*!
       \brief Apply inverse operator.

       \copydoc InverseOperator::apply(X&,Y&,InverseOperatorResult&)
     */
    virtual void apply (X& x, X& b, InverseOperatorResult& res)
    {
      res.clear();                      // clear solver statistics
      Timer watch;                    // start a timer
      _prec.pre(x,b);                 // prepare preconditioner
      _op.applyscaleadd(-1,x,b);      // overwrite b with defect

      std::vector<shared_ptr<X> > p(_restart);
      std::vector<typename X::field_type> pp(_restart);
      X q(x);                  // a temporary vector
      X prec_res(x);           // a temporary vector for preconditioner output

      p[0].reset(new X(x));

      double def0 = _sp.norm(b);    // compute norm
      if (def0<1E-30)        // convergence check
      {
        res.converged  = true;
        res.iterations = 0;                     // fill statistics
        res.reduction = 0;
        res.conv_rate  = 0;
        res.elapsed=0;
        if (_verbose>0)                       // final print
          std::cout << "=== rate=" << res.conv_rate
                    << ", T=" << res.elapsed << ", TIT=" << res.elapsed
                    << ", IT=0" << std::endl;
        return;
      }

      if (_verbose>0)                 // printing
      {
        std::cout << "=== GeneralizedPCGSolver" << std::endl;
        if (_verbose>1) {
          this->printHeader(std::cout);
          this->printOutput(std::cout,0,def0);
        }
      }
      // some local variables
      double def=def0;       // loop variables
      field_type rho, lambda;

      int i=0;
      int ii=0;
      // determine initial search direction
      *(p[0]) = 0;                              // clear correction
      _prec.apply(*(p[0]),b);                   // apply preconditioner
      rho = _sp.dot(*(p[0]),b);             // orthogonalization
      _op.apply(*(p[0]),q);                 // q=Ap
      pp[0] = _sp.dot(*(p[0]),q);           // scalar product
      lambda = rho/pp[0];         // minimization
      x.axpy(lambda,*(p[0]));               // update solution
      b.axpy(-lambda,q);              // update defect

      // convergence test
      double defnew=_sp.norm(b);    // comp defect norm
      if (_verbose>1)                 // print
        this->printOutput(std::cout,++i,defnew,def);
      def = defnew;                   // update norm
      if (def<def0*_reduction || def<1E-30)        // convergence check
      {
        res.converged  = true;
        if (_verbose>0)                       // final print
        {
          std::cout << "=== rate=" << res.conv_rate
                    << ", T=" << res.elapsed
                    << ", TIT=" << res.elapsed
                    << ", IT=" << 1 << std::endl;
        }
        return;
      }

      while(i<_maxit) {
        // the loop
        int end=std::min(_restart, _maxit-i+1);
        for (ii=1; ii<end; ++ii )
        {
          //std::cout<<" ii="<<ii<<" i="<<i<<std::endl;
          // compute next conjugate direction
          prec_res = 0;                                  // clear correction
          _prec.apply(prec_res,b);                       // apply preconditioner

          p[ii].reset(new X(prec_res));
          _op.apply(prec_res, q);

          for(int j=0; j<ii; ++j) {
            rho =_sp.dot(q,*(p[j]))/pp[j];
            p[ii]->axpy(-rho, *(p[j]));
          }

          // minimize in given search direction
          _op.apply(*(p[ii]),q);                     // q=Ap
          pp[ii] = _sp.dot(*(p[ii]),q);               // scalar product
          rho = _sp.dot(*(p[ii]),b);                 // orthogonalization
          lambda = rho/pp[ii];             // minimization
          x.axpy(lambda,*(p[ii]));                   // update solution
          b.axpy(-lambda,q);                  // update defect

          // convergence test
          double defnew=_sp.norm(b);        // comp defect norm

          if (_verbose>1)                     // print
            this->printOutput(std::cout,++i,defnew,def);

          def = defnew;                       // update norm
          if (def<def0*_reduction || def<1E-30)            // convergence check
          {
            res.converged  = true;
            break;
          }
        }
        if(res.converged)
          break;
        if(end==_restart) {
          *(p[0])=*(p[_restart-1]);
          pp[0]=pp[_restart-1];
        }
      }

      // postprocess preconditioner
      _prec.post(x);

      // fill statistics
      res.iterations = i;
      res.reduction = def/def0;
      res.conv_rate  = pow(res.reduction,1.0/i);
      res.elapsed = watch.elapsed();

      if (_verbose>0)                     // final print
      {
        std::cout << "=== rate=" << res.conv_rate
                  << ", T=" << res.elapsed
                  << ", TIT=" << res.elapsed/i
                  << ", IT=" << i+1 << std::endl;
      }
    }

    /*!
       \brief Apply inverse operator with given reduction factor.

       \copydoc InverseOperator::apply(X&,Y&,double,InverseOperatorResult&)
     */
    virtual void apply (X& x, X& b, double reduction,
                        InverseOperatorResult& res)
    {
      std::swap(_reduction,reduction);
      (*this).apply(x,b,res);
      std::swap(_reduction,reduction);
    }
  private:
    SeqScalarProduct<X> ssp;
    LinearOperator<X,X>& _op;
    Preconditioner<X,X>& _prec;
    ScalarProduct<X>& _sp;
    double _reduction;
    int _maxit;
    int _verbose;
    int _restart;
  };

  /** @} end documentation */

} // end namespace

#endif
