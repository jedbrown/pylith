// -*- C++ -*-
//
// ----------------------------------------------------------------------
//
//                           Brad T. Aagaard
//                        U.S. Geological Survey
//
// {LicenseText}
//
// ----------------------------------------------------------------------
//

/** @file modulesrc/feassemble/ElasticityImplicitLgDeform.i
 *
 * @brief Python interface to C++ ElasticityImplicitLgDeform object.
 */

namespace pylith {
  namespace feassemble {

    class ElasticityImplicitLgDeform : public IntegratorElasticityLgDeform
    { // ElasticityImplicitLgDeform

      // PUBLIC MEMBERS /////////////////////////////////////////////////
    public:

      /// Constructor
      ElasticityImplicitLgDeform(void);

      /// Destructor
      ~ElasticityImplicitLgDeform(void);

      /// Deallocate PETSc and local data structures.
      void deallocate(void);
  
      /** Set time step for advancing from time t to time t+dt.
       *
       * @param dt Time step
       */
      void timeStep(const double dt);

      /** Get stable time step for advancing from time t to time t+dt.
       *
       * Default is current time step.
       *
       * @param mesh Finite-element mesh.
       * @returns Time step
       */
      double stableTimeStep(const pylith::topology::Mesh& mesh) const;

      /** Set flag for setting constraints for total field solution or
       *  incremental field solution.
       *
       * @param flag True if using incremental solution, false otherwise.
       */
      void useSolnIncr(const bool flag);

      /** Integrate residual part of RHS for 3-D finite elements.
       * Includes gravity and element internal force contribution.
       *
       * We assume that the effects of boundary conditions are already
       * included in the residual (tractions, concentrated nodal forces,
       * and contributions to internal force vector due to
       * displacement/velocity BC).  This routine computes the additional
       * external loads due to body forces plus the
       * element internal forces for the current stress state.
       *
       * @param residual Field containing values for residual
       * @param t Current time
       * @param fields Solution fields
       */
      void integrateResidual(const pylith::topology::Field<pylith::topology::Mesh>& residual,
			     const double t,
			     pylith::topology::SolutionFields* const fields);
      
      /** Integrate contributions to Jacobian matrix (A) associated with
       * operator.
       *
       * @param jacobian Sparse matrix for Jacobian of system.
       * @param t Current time
       * @param fields Solution fields
       */
      void integrateJacobian(pylith::topology::Jacobian* jacobian,
			     const double t,
			     pylith::topology::SolutionFields* const fields);
      
    }; // ElasticityImplicitLgDeform

  } // feassemble
} // pylith


// End of file 