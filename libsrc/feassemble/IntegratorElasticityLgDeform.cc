// -*- C++ -*-
//
// ======================================================================
//
//                           Brad T. Aagaard
//                        U.S. Geological Survey
//
// {LicenseText}
//
// ======================================================================
//

#include <portinfo>

#include "IntegratorElasticityLgDeform.hh" // implementation of class methods

#include "Quadrature.hh" // USES Quadrature
#include "CellGeometry.hh" // USES CellGeometry

#include "pylith/materials/ElasticMaterial.hh" // USES ElasticMaterial
#include "pylith/topology/Field.hh" // USES Field
#include "pylith/topology/Fields.hh" // USES Fields
#include "pylith/topology/SolutionFields.hh" // USES SolutionFields
#include "pylith/utils/EventLogger.hh" // USES EventLogger

#include "spatialdata/units/Nondimensional.hh" // USES Nondimensional

#include "pylith/utils/array.hh" // USES double_array

#include <cstring> // USES memcpy()
#include <strings.h> // USES strcasecmp()
#include <cassert> // USES assert()
#include <stdexcept> // USES std::runtime_error

//#define PRECOMPUTE_GEOMETRY

// ----------------------------------------------------------------------
typedef pylith::topology::Mesh::SieveMesh SieveMesh;
typedef pylith::topology::Mesh::RealSection RealSection;

// ----------------------------------------------------------------------
// Constructor
pylith::feassemble::IntegratorElasticityLgDeform::IntegratorElasticityLgDeform(void)
{ // constructor
} // constructor

// ----------------------------------------------------------------------
// Destructor
pylith::feassemble::IntegratorElasticityLgDeform::~IntegratorElasticityLgDeform(void)
{ // destructor
} // destructor
  
// ----------------------------------------------------------------------
// Determine whether we need to recompute the Jacobian.
bool
pylith::feassemble::IntegratorElasticityLgDeform::needNewJacobian(void)
{ // needNewJacobian
  _needNewJacobian = IntegratorElasticity::needNewJacobian();
  return _needNewJacobian;
} // needNewJacobian

// ----------------------------------------------------------------------
// Update state variables as needed.
void
pylith::feassemble::IntegratorElasticityLgDeform::updateStateVars(
				      const double t,
				      topology::SolutionFields* const fields)
{ // updateStateVars
  assert(0 != _quadrature);
  assert(0 != _material);
  assert(0 != fields);

  // No need to update state vars if material doesn't have any.
  if (!_material->hasStateVars())
    return;

  // Get cell information that doesn't depend on particular cell
  const int cellDim = _quadrature->cellDim();
  const int numQuadPts = _quadrature->numQuadPts();
  const int numBasis = _quadrature->numBasis();
  const int spaceDim = _quadrature->spaceDim();
  const int tensorSize = _material->tensorSize();
  totalStrain_fn_type calcTotalStrainFn;
  if (1 == cellDim) {
    calcTotalStrainFn = 
      &pylith::feassemble::IntegratorElasticityLgDeform::_calcTotalStrain1D;
  } else if (2 == cellDim) {
    calcTotalStrainFn = 
      &pylith::feassemble::IntegratorElasticityLgDeform::_calcTotalStrain2D;
  } else if (3 == cellDim) {
    calcTotalStrainFn = 
      &pylith::feassemble::IntegratorElasticityLgDeform::_calcTotalStrain3D;
  } else {
      std::cerr << "Bad cell dimension '" << cellDim << "'." << std::endl;
      assert(0);
      throw std::logic_error("Bad cell dimension in IntegratorElasticityLgDeform.");
  } // else

  // Allocate arrays for cell data.
  double_array dispCell(numBasis*spaceDim);
  double_array strainCell(numQuadPts*tensorSize);
  double_array deformCell(numQuadPts*spaceDim*spaceDim);
  deformCell = 0.0;
  strainCell = 0.0;

  // Get cell information
  const ALE::Obj<SieveMesh>& sieveMesh = fields->mesh().sieveMesh();
  assert(!sieveMesh.isNull());
  const int materialId = _material->id();
  const ALE::Obj<SieveMesh::label_sequence>& cells = 
    sieveMesh->getLabelStratum("material-id", materialId);
  assert(!cells.isNull());
  const SieveMesh::label_sequence::iterator cellsBegin = cells->begin();
  const SieveMesh::label_sequence::iterator cellsEnd = cells->end();

  // Get fields
  const topology::Field<topology::Mesh>& disp = fields->get("disp(t)");
  const ALE::Obj<RealSection>& dispSection = disp.section();
  assert(!dispSection.isNull());
  topology::Mesh::RestrictVisitor dispVisitor(*dispSection, 
					      dispCell.size(), &dispCell[0]);

  double_array coordinatesCell(numBasis*spaceDim);
  const ALE::Obj<RealSection>& coordinates = 
    sieveMesh->getRealSection("coordinates");
  assert(!coordinates.isNull());
  topology::Mesh::RestrictVisitor coordsVisitor(*coordinates, 
						coordinatesCell.size(),
						&coordinatesCell[0]);

  // Loop over cells
  for (SieveMesh::label_sequence::iterator c_iter=cellsBegin;
       c_iter != cellsEnd;
       ++c_iter) {
    // Retrieve geometry information for current cell
#if defined(PRECOMPUTE_GEOMETRY)
    _quadrature->retrieveGeometry(*c_iter);
#else
    coordsVisitor.clear();
    sieveMesh->restrictClosure(*c_iter, coordsVisitor);
    _quadrature->computeGeometry(coordinatesCell, *c_iter);
#endif

    // Restrict input fields to cell
    dispVisitor.clear();
    sieveMesh->restrictClosure(*c_iter, dispVisitor);

    // Get cell geometry information that depends on cell
    const double_array& basisDeriv = _quadrature->basisDeriv();
  
    // Compute deformation tensor.
    _calcDeformation(&deformCell, basisDeriv, coordinatesCell, dispCell,
		     numBasis, numQuadPts, spaceDim);

    // Compute strains
    calcTotalStrainFn(&strainCell, deformCell, numQuadPts);

    // Update material state
    _material->updateStateVars(strainCell, *c_iter);
  } // for
} // updateStateVars

// ----------------------------------------------------------------------
void
pylith::feassemble::IntegratorElasticityLgDeform::_calcStrainStressField(
				 topology::Field<topology::Mesh>* field,
				 const char* name,
				 topology::SolutionFields* const fields)
{ // _calcStrainStressField
  assert(0 != field);
  assert(0 != _quadrature);
  assert(0 != _material);

  const bool calcStress = (0 == strcasecmp(name, "stress")) ? true : false;
    
  // Get cell information that doesn't depend on particular cell
  const int cellDim = _quadrature->cellDim();
  const int numQuadPts = _quadrature->numQuadPts();
  const int numBasis = _quadrature->numBasis();
  const int spaceDim = _quadrature->spaceDim();
  const int tensorSize = _material->tensorSize();
  totalStrain_fn_type calcTotalStrainFn;
  if (1 == cellDim) {
    calcTotalStrainFn = 
      &pylith::feassemble::IntegratorElasticityLgDeform::_calcTotalStrain1D;
  } else if (2 == cellDim) {
    calcTotalStrainFn = 
      &pylith::feassemble::IntegratorElasticityLgDeform::_calcTotalStrain2D;
  } else if (3 == cellDim) {
    calcTotalStrainFn = 
      &pylith::feassemble::IntegratorElasticityLgDeform::_calcTotalStrain3D;
  } else {
      std::cerr << "Bad cell dimension '" << cellDim << "'." << std::endl;
      assert(0);
      throw std::logic_error("Bad cell dimension in IntegratorElasticityLgDeform.");
  } // else
  
  // Allocate arrays for cell data.
  double_array dispCell(numBasis*spaceDim);
  double_array deformCell(numQuadPts*spaceDim*spaceDim);
  double_array strainCell(numQuadPts*tensorSize);
  double_array stressCell(numQuadPts*tensorSize);

  // Get cell information
  const ALE::Obj<SieveMesh>& sieveMesh = field->mesh().sieveMesh();
  assert(!sieveMesh.isNull());
  const int materialId = _material->id();
  const ALE::Obj<SieveMesh::label_sequence>& cells = 
    sieveMesh->getLabelStratum("material-id", materialId);
  assert(!cells.isNull());
  const SieveMesh::label_sequence::iterator cellsBegin = cells->begin();
  const SieveMesh::label_sequence::iterator cellsEnd = cells->end();

  // Get field
  const topology::Field<topology::Mesh>& disp = fields->get("disp(t)");
  const ALE::Obj<RealSection>& dispSection = disp.section();
  assert(!dispSection.isNull());
  topology::Mesh::RestrictVisitor dispVisitor(*dispSection, 
					      dispCell.size(), &dispCell[0]);
    
  double_array coordinatesCell(numBasis*spaceDim);
  const ALE::Obj<RealSection>& coordinates = 
    sieveMesh->getRealSection("coordinates");
  assert(!coordinates.isNull());
  topology::Mesh::RestrictVisitor coordsVisitor(*coordinates, 
						coordinatesCell.size(),
						&coordinatesCell[0]);

  const ALE::Obj<RealSection>& fieldSection = field->section();
  assert(!fieldSection.isNull());

  // Loop over cells
  for (SieveMesh::label_sequence::iterator c_iter=cellsBegin;
       c_iter != cellsEnd;
       ++c_iter) {
    // Retrieve geometry information for current cell
#if defined(PRECOMPUTE_GEOMETRY)
    _quadrature->retrieveGeometry(*c_iter);
#else
    coordsVisitor.clear();
    sieveMesh->restrictClosure(*c_iter, coordsVisitor);
    _quadrature->computeGeometry(coordinatesCell, *c_iter);
#endif

    // Restrict input fields to cell
    dispVisitor.clear();
    sieveMesh->restrictClosure(*c_iter, dispVisitor);

    // Get cell geometry information that depends on cell
    const double_array& basisDeriv = _quadrature->basisDeriv();
    
    // Compute deformation tensor.
    _calcDeformation(&deformCell, basisDeriv, coordinatesCell, dispCell,
		     numBasis, numQuadPts, spaceDim);

    // Compute strains
    calcTotalStrainFn(&strainCell, deformCell, numQuadPts);

    if (!calcStress) 
      fieldSection->updatePoint(*c_iter, &strainCell[0]);
    else {
      _material->retrievePropsAndVars(*c_iter);
      stressCell = _material->calcStress(strainCell);
      fieldSection->updatePoint(*c_iter, &stressCell[0]);
    } // else
  } // for
} // _calcStrainStressField

// ----------------------------------------------------------------------
void
pylith::feassemble::IntegratorElasticityLgDeform::_calcStressFromStrain(
				   topology::Field<topology::Mesh>* field)
{ // _calcStressFromStrain
  assert(0 != field);
  assert(0 != _quadrature);
  assert(0 != _material);

  const int cellDim = _quadrature->cellDim();
  const int numQuadPts = _quadrature->numQuadPts();
  const int numBasis = _quadrature->numBasis();
  const int spaceDim = _quadrature->spaceDim();
  const int tensorSize = _material->tensorSize();
  
  // Allocate arrays for cell data.
  double_array strainCell(numQuadPts*tensorSize);
  strainCell = 0.0;
  double_array stressCell(numQuadPts*tensorSize);
  stressCell = 0.0;

  // Get cell information
  const ALE::Obj<SieveMesh>& sieveMesh = field->mesh().sieveMesh();
  assert(!sieveMesh.isNull());
  const int materialId = _material->id();
  const ALE::Obj<SieveMesh::label_sequence>& cells = 
    sieveMesh->getLabelStratum("material-id", materialId);
  assert(!cells.isNull());
  const SieveMesh::label_sequence::iterator cellsBegin = cells->begin();
  const SieveMesh::label_sequence::iterator cellsEnd = cells->end();

  // Get field
  const ALE::Obj<RealSection>& fieldSection = field->section();
  assert(!fieldSection.isNull());

  // Loop over cells
  for (SieveMesh::label_sequence::iterator c_iter=cellsBegin;
       c_iter != cellsEnd;
       ++c_iter) {
    fieldSection->restrictPoint(*c_iter, &strainCell[0], strainCell.size());
    _material->retrievePropsAndVars(*c_iter);
    stressCell = _material->calcStress(strainCell);
    fieldSection->updatePoint(*c_iter, &stressCell[0]);
  } // for
} // _calcStressFromStrain

// ----------------------------------------------------------------------
// Integrate elasticity term in residual for 1-D cells.
void
pylith::feassemble::IntegratorElasticityLgDeform::_elasticityResidual1D(
				     const double_array& stress,
				     const double_array& disp)
{ // _elasticityResidual1D
  const int numQuadPts = _quadrature->numQuadPts();
  const int numBasis = _quadrature->numBasis();
  const int spaceDim = _quadrature->spaceDim();
  const int cellDim = _quadrature->cellDim();
  const double_array& quadWts = _quadrature->quadWts();
  const double_array& jacobianDet = _quadrature->jacobianDet();
  const double_array& basisDeriv = _quadrature->basisDeriv();

  assert(1 == cellDim);
  assert(quadWts.size() == numQuadPts);

  for (int iQuad=0; iQuad < numQuadPts; ++iQuad) {
    const double wt = quadWts[iQuad] * jacobianDet[iQuad];
    const double s11 = stress[iQuad];
    double l11 = 0.0;
    for (int kBasis=0; kBasis < numBasis; ++kBasis)
      l11 += basisDeriv[iQuad*numBasis+kBasis  ] * disp[kBasis  ]; 
    for (int iBasis=0; iBasis < numBasis; ++iBasis) {
      const double N1 = wt * (1.0 + l11) * basisDeriv[iQuad*numBasis+iBasis  ];
      _cellVector[iBasis*spaceDim  ] -= N1*s11;
    } // for
  } // for
  PetscLogFlops(numQuadPts*(1+numBasis*5+numBasis*2));
} // _elasticityResidual1D

// ----------------------------------------------------------------------
// Integrate elasticity term in residual for 2-D cells.
void
pylith::feassemble::IntegratorElasticityLgDeform::_elasticityResidual2D(
				     const double_array& stress,
				     const double_array& disp)
{ // _elasticityResidual2D
  const int numQuadPts = _quadrature->numQuadPts();
  const int numBasis = _quadrature->numBasis();
  const int spaceDim = _quadrature->spaceDim();
  const int cellDim = _quadrature->cellDim();
  const double_array& quadWts = _quadrature->quadWts();
  const double_array& jacobianDet = _quadrature->jacobianDet();
  const double_array& basisDeriv = _quadrature->basisDeriv();
  
  assert(2 == cellDim);
  assert(quadWts.size() == numQuadPts);
  const int stressSize = 3;

  for (int iQuad=0; iQuad < numQuadPts; ++iQuad) {
    const int iQ = iQuad*numBasis*spaceDim;
    const double wt = quadWts[iQuad] * jacobianDet[iQuad];
    const double s11 = stress[iQuad*stressSize+0];
    const double s22 = stress[iQuad*stressSize+1];
    const double s12 = stress[iQuad*stressSize+2];

    double l11 = 0.0;
    double l12 = 0.0;
    double l21 = 0.0;
    double l22 = 0.0;
    for (int kBasis=0; kBasis < numBasis; ++kBasis) {
      const int kB = kBasis*spaceDim;
      l11 += basisDeriv[iQ+kB  ] * disp[kB  ];
      l12 += basisDeriv[iQ+kB+1] * disp[kB  ];
      l21 += basisDeriv[iQ+kB  ] * disp[kB+1];
      l22 += basisDeriv[iQ+kB+1] * disp[kB+1];
    } // for

    for (int iBasis=0, iQ=iQuad*numBasis*spaceDim;
	 iBasis < numBasis;
	 ++iBasis) {
      const int iB = iBasis*spaceDim;
      const double N1 = basisDeriv[iQ+iB  ];
      const double N2 = basisDeriv[iQ+iB+1];
      _cellVector[iB  ] -= 
	wt*((1.0+l11)*N1*s11 + N2*l12*s22 + ((1.0+l11)*N2 + l12*N1)*s12);
      _cellVector[iB+1] -=
	wt*(l21*N1*s11 + (1.0+l22)*N2*s22 + ((1.0+l22)*N1 + l21*N2)*s12);
    } // for
  } // for
  PetscLogFlops(numQuadPts*(1+numBasis*(numBasis*8+14)));
} // _elasticityResidual2D

// ----------------------------------------------------------------------
// Integrate elasticity term in residual for 3-D cells.
void
pylith::feassemble::IntegratorElasticityLgDeform::_elasticityResidual3D(
				     const double_array& stress,
				     const double_array& disp)
{ // _elasticityResidual3D
  const int numQuadPts = _quadrature->numQuadPts();
  const int numBasis = _quadrature->numBasis();
  const int spaceDim = _quadrature->spaceDim();
  const int cellDim = _quadrature->cellDim();
  const double_array& quadWts = _quadrature->quadWts();
  const double_array& jacobianDet = _quadrature->jacobianDet();
  const double_array& basisDeriv = _quadrature->basisDeriv();
  
  assert(3 == cellDim);
  assert(quadWts.size() == numQuadPts);
  const int stressSize = 6;
  
  for (int iQuad=0; iQuad < numQuadPts; ++iQuad) {
    const int iQ = iQuad*numBasis*spaceDim;
    const double wt = quadWts[iQuad] * jacobianDet[iQuad];
    const double s11 = stress[iQuad*stressSize+0];
    const double s22 = stress[iQuad*stressSize+1];
    const double s33 = stress[iQuad*stressSize+2];
    const double s12 = stress[iQuad*stressSize+3];
    const double s23 = stress[iQuad*stressSize+4];
    const double s13 = stress[iQuad*stressSize+5];
    
    double l11 = 0.0;
    double l12 = 0.0;
    double l13 = 0.0;
    double l21 = 0.0;
    double l22 = 0.0;
    double l23 = 0.0;
    double l31 = 0.0;
    double l32 = 0.0;
    double l33 = 0.0;
    for (int kBasis=0; kBasis < numBasis; ++kBasis) {
      const int kB = kBasis*spaceDim;
      l11 += basisDeriv[iQ+kB  ] * disp[kB  ];
      l12 += basisDeriv[iQ+kB+1] * disp[kB  ];
      l13 += basisDeriv[iQ+kB+2] * disp[kB  ];
      l21 += basisDeriv[iQ+kB  ] * disp[kB+1];
      l22 += basisDeriv[iQ+kB+1] * disp[kB+1];
      l23 += basisDeriv[iQ+kB+2] * disp[kB+1];
      l31 += basisDeriv[iQ+kB  ] * disp[kB+2];
      l32 += basisDeriv[iQ+kB+1] * disp[kB+2];
      l33 += basisDeriv[iQ+kB+2] * disp[kB+2];
    } // for
    
    for (int iBasis=0, iQ=iQuad*numBasis*spaceDim;
	 iBasis < numBasis;
	 ++iBasis) {
      const int iB = iBasis*spaceDim;
      const double N1 = basisDeriv[iQ+iB  ];
      const double N2 = basisDeriv[iQ+iB+1];
      const double N3 = basisDeriv[iQ+iB+2];

      _cellVector[iB  ] -= 
	wt*((1.0+l11)*N1*s11 + l12*N2*s22 + l13*N3*s33 +
	    ((1.0+l11)*N2 + l12*N1)*s12 +
	    (l12*N3 + l13*N2)*s23 +
	    ((1.0+l11)*N3 + l13*N1)*s13);
      _cellVector[iB+1] -=
	wt*(l21*N1*s11 + (1.0+l22)*N2*s22 + l23*N3*s33 +
	    ((1.0+l22)*N1 + l21*N2)*s12 +
	    ((1.0+l22)*N3 + l23*N2)*s23 +
	    (l21*N3 + l23*N1)*s13);
      _cellVector[iB+2] -=
	wt*(l31*N1*s11 + l32*N2*s22 + (1.0+l33)*N3*s33 +
	    (l31*N2 + l32*N1)*s12 +
	    ((1.0+l33)*N2 + l32*N3)*s23 +
	    ((1.0+l33)*N1 + l31*N3)*s13);
    } // for
  } // for
  PetscLogFlops(numQuadPts*(1+numBasis*(numBasis*18+3*27)));
} // _elasticityResidual3D

// ----------------------------------------------------------------------
// Integrate elasticity term in Jacobian for 1-D cells.
void
pylith::feassemble::IntegratorElasticityLgDeform::_elasticityJacobian1D(
				       const double_array& elasticConsts,
				       const double_array& stress,
				       const double_array& disp)
{ // _elasticityJacobian1D
  const int numQuadPts = _quadrature->numQuadPts();
  const int numBasis = _quadrature->numBasis();
  const int spaceDim = _quadrature->spaceDim();
  const int cellDim = _quadrature->cellDim();
  const double_array& quadWts = _quadrature->quadWts();
  const double_array& jacobianDet = _quadrature->jacobianDet();
  const double_array& basisDeriv = _quadrature->basisDeriv();
  
  assert(1 == cellDim);
  assert(quadWts.size() == numQuadPts);
  
  for (int iQuad=0; iQuad < numQuadPts; ++iQuad) {
    const double wt = quadWts[iQuad] * jacobianDet[iQuad];
    const double C1111 = elasticConsts[iQuad];
    const double s11 = stress[iQuad];

    double l11 = 0.0;
    for (int kBasis=0; kBasis < numBasis; ++kBasis)
      l11 += basisDeriv[iQuad*numBasis+kBasis  ] * disp[kBasis  ]; 

    // KLij = valI * valJ * C1111 + valInl * valJnl * s11
    // valI = (1+l11) * Ni,1
    // valJ = (1+l11) * Nj,1
    // valInl = Ni,1
    // valJnl = Nj,1
    for (int iBasis=0, iQ=iQuad*numBasis; iBasis < numBasis; ++iBasis) {
      const double valI = wt*basisDeriv[iQ+iBasis]*(1.0+l11)*(1.0+l11)*C1111;
      const double valInl = wt*s11*basisDeriv[iQ+iBasis];
      for (int jBasis=0; jBasis < numBasis; ++jBasis) {
	const double valIJ = valI * basisDeriv[iQ+jBasis];
	const double valIJnl = valInl * basisDeriv[iQ+jBasis];
	const int iBlock = iBasis*spaceDim * (numBasis*spaceDim);
	const int jBlock = jBasis*spaceDim;
	_cellMatrix[iBlock+jBlock] += valIJ + valIJnl;
      } // for
    } // for
  } // for
  PetscLogFlops(numQuadPts*(1+numBasis*(6+numBasis*4)));
} // _elasticityJacobian1D

// ----------------------------------------------------------------------
// Integrate elasticity term in Jacobian for 2-D cells.
void
pylith::feassemble::IntegratorElasticityLgDeform::_elasticityJacobian2D(
				       const double_array& elasticConsts,
				       const double_array& stress,
				       const double_array& disp)
{ // _elasticityJacobian2D
  const int numQuadPts = _quadrature->numQuadPts();
  const int numBasis = _quadrature->numBasis();
  const int spaceDim = _quadrature->spaceDim();
  const int cellDim = _quadrature->cellDim();
  const double_array& quadWts = _quadrature->quadWts();
  const double_array& jacobianDet = _quadrature->jacobianDet();
  const double_array& basisDeriv = _quadrature->basisDeriv();
  const int tensorSize = _material->tensorSize();
  
  assert(2 == cellDim);
  assert(quadWts.size() == numQuadPts);
  const int numConsts = 6;

  for (int iQuad=0; iQuad < numQuadPts; ++iQuad) {
    const int iQ = iQuad*numBasis*spaceDim;
    const double wt = quadWts[iQuad] * jacobianDet[iQuad];
    // tau_ij = C_ijkl * e_kl
    //        = C_ijlk * 0.5 (u_k,l + u_l,k)
    //        = 0.5 * C_ijkl * (u_k,l + u_l,k)
    // divide C_ijkl by 2 if k != l
    const int iC = iQuad*numConsts;
    const double C1111 = elasticConsts[iC+0];
    const double C1122 = elasticConsts[iC+1];
    const double C1112 = elasticConsts[iC+2] / 2.0; // 2*mu -> mu
    const double C2222 = elasticConsts[iC+3];
    const double C2212 = elasticConsts[iC+4] / 2.0;
    const double C1212 = elasticConsts[iC+5] / 2.0;

    const int iS = iQuad*tensorSize;
    const double s11 = stress[iS+0];
    const double s22 = stress[iS+1];
    const double s12 = stress[iS+2];

    double l11 = 0.0;
    double l12 = 0.0;
    double l21 = 0.0;
    double l22 = 0.0;
    for (int kBasis=0; kBasis < numBasis; ++kBasis) {
      const int kB = kBasis*spaceDim;
      l11 += basisDeriv[iQ+kB  ] * disp[kB  ];
      l12 += basisDeriv[iQ+kB+1] * disp[kB  ];
      l21 += basisDeriv[iQ+kB  ] * disp[kB+1];
      l22 += basisDeriv[iQ+kB+1] * disp[kB+1];
    } // for

    for (int iBasis=0, iQ=iQuad*numBasis*spaceDim;
	 iBasis < numBasis;
	 ++iBasis) {
      const int iB = iBasis*spaceDim;
      const double Nip = wt*basisDeriv[iQ+iB  ];
      const double Niq = wt*basisDeriv[iQ+iB+1];
      const int iBlock = (iB) * (numBasis*spaceDim);
      const int iBlock1 = (iB+1) * (numBasis*spaceDim);

      const double valInl0 = Nip*s11 + Niq*s12;
      const double valInl1 = Nip*s12 + Niq*s22;
      for (int jBasis=0; jBasis < numBasis; ++jBasis) {
	const int jB = jBasis*spaceDim;
	const double Njp = basisDeriv[iQ+jB  ];
	const double Njq = basisDeriv[iQ+jB+1];

	// Generated using Maxima (see jacobian2d_lgdeform.wxm)
	const double Ki0j0 = 
	  l12*Niq*(l12*Njq*C2222 + 
		   ((l11+1)*Njq+l12*Njp)*C2212 + 
		   (l11+1)*Njp*C1122) + 
	  ((l11+1)*Niq+l12*Nip)*(l12*Njq*C2212 + 
				 ((l11+1)*Njq+l12*Njp)*C1212 + 
				 (l11+1)*Njp*C1112) + 
	  (l11+1)*Nip*(l12*Njq*C1122 + 
		       ((l11+1)*Njq+l12*Njp)*C1112 + 
		       (l11+1)*Njp*C1111);
	const double Ki0j1 =
	  l12*Niq*((l22+1.0)*Njq*C2222 + 
		   (l21*Njq+(l22+1.0)*Njp)*C2212 + 
		   l21*Njp*C1122) + 
	  ((l11+1.0)*Niq+l12*Nip)*((l22+1.0)*Njq*C2212 + 
				 (l21*Njq+(l22+1.0)*Njp)*C1212 + 
				 l21*Njp*C1112) + 
	  (l11+1.0)*Nip*((l22+1.0)*Njq*C1122 + 
		       (l21*Njq+(l22+1.0)*Njp)*C1112 + 
		       l21*Njp*C1111);
	const double Ki1j0 =
	  (l22+1.0)*Niq*(l12*Njq*C2222 + 
		       ((l11+1.0)*Njq+l12*Njp)*C2212 + 
		       (l11+1.0)*Njp*C1122) + 
	  (l21*Niq+(l22+1.0)*Nip)*(l12*Njq*C2212 + 
				 ((l11+1.0)*Njq+l12*Njp)*C1212 + 
				 (l11+1.0)*Njp*C1112) + 
	  l21*Nip*(l12*Njq*C1122 + 
		   ((l11+1.0)*Njq+l12*Njp)*C1112 + 
		   (l11+1.0)*Njp*C1111);
	const double Ki1j1 =
	  (l22+1.0)*Niq*((l22+1.0)*Njq*C2222 + 
		       (l21*Njq+(l22+1.0)*Njp)*C2212 + 
		       l21*Njp*C1122) + 
	  (l21*Niq+(l22+1.0)*Nip)*((l22+1.0)*Njq*C2212 + 
				 (l21*Njq+(l22+1.0)*Njp)*C1212 + 
				 l21*Njp*C1112) + 
	  l21*Nip*((l22+1.0)*Njq*C1122 + 
		   (l21*Njq+(l22+1.0)*Njp)*C1112 + 
		   l21*Njp*C1111);
	const double Knl = 
	  (Nip*s11 + Niq*s12)*Njp + (Nip*s12 + Niq*s22)*Njq;

	const int jBlock = (jB);
	const int jBlock1 = (jB+1);
	_cellMatrix[iBlock +jBlock ] += Ki0j0 + Knl;
	_cellMatrix[iBlock +jBlock1] += Ki0j1;
	_cellMatrix[iBlock1+jBlock ] += Ki1j0;
	_cellMatrix[iBlock1+jBlock1] += Ki1j1 + Knl;
      } // for
    } // for
  } // for
  PetscLogFlops(numQuadPts*(1+numBasis*(2+numBasis*(3*11+4))));
} // _elasticityJacobian2D

// ----------------------------------------------------------------------
// Integrate elasticity term in Jacobian for 3-D cells.
void
pylith::feassemble::IntegratorElasticityLgDeform::_elasticityJacobian3D(
				       const double_array& elasticConsts,
				       const double_array& stress,
				       const double_array& disp)
{ // _elasticityJacobian3D
  const int numQuadPts = _quadrature->numQuadPts();
  const int numBasis = _quadrature->numBasis();
  const int spaceDim = _quadrature->spaceDim();
  const int cellDim = _quadrature->cellDim();
  const double_array& quadWts = _quadrature->quadWts();
  const double_array& jacobianDet = _quadrature->jacobianDet();
  const double_array& basisDeriv = _quadrature->basisDeriv();
  const int tensorSize = _material->tensorSize();

  assert(3 == cellDim);
  assert(quadWts.size() == numQuadPts);
  const int numConsts = 21;

  // Compute Jacobian for consistent tangent matrix
  for (int iQuad=0; iQuad < numQuadPts; ++iQuad) {
    const int iQ = iQuad*numBasis*spaceDim;
    const double wt = quadWts[iQuad] * jacobianDet[iQuad];
    // tau_ij = C_ijkl * e_kl
    //        = C_ijlk * 0.5 (u_k,l + u_l,k)
    //        = 0.5 * C_ijkl * (u_k,l + u_l,k)
    // divide C_ijkl by 2 if k != l
    const int iC = iQuad*numConsts;
    const double C1111 = elasticConsts[iC+ 0];
    const double C1122 = elasticConsts[iC+ 1];
    const double C1133 = elasticConsts[iC+ 2];
    const double C1112 = elasticConsts[iC+ 3] / 2.0;
    const double C1123 = elasticConsts[iC+ 4] / 2.0;
    const double C1113 = elasticConsts[iC+ 5] / 2.0;
    const double C2222 = elasticConsts[iC+ 6];
    const double C2233 = elasticConsts[iC+ 7];
    const double C2212 = elasticConsts[iC+ 8] / 2.0;
    const double C2223 = elasticConsts[iC+ 9] / 2.0;
    const double C2213 = elasticConsts[iC+10] / 2.0;
    const double C3333 = elasticConsts[iC+11];
    const double C3312 = elasticConsts[iC+12] / 2.0;
    const double C3323 = elasticConsts[iC+13] / 2.0;
    const double C3313 = elasticConsts[iC+14] / 2.0;
    const double C1212 = elasticConsts[iC+15] / 2.0;
    const double C1223 = elasticConsts[iC+16] / 2.0;
    const double C1213 = elasticConsts[iC+17] / 2.0;
    const double C2323 = elasticConsts[iC+18] / 2.0;
    const double C2313 = elasticConsts[iC+19] / 2.0;
    const double C1313 = elasticConsts[iC+20] / 2.0;

    const int iS = iQuad*tensorSize;
    const double s11 = stress[iS+0];
    const double s22 = stress[iS+1];
    const double s33 = stress[iS+2];
    const double s12 = stress[iS+3];
    const double s23 = stress[iS+4];
    const double s13 = stress[iS+5];

    double l11 = 0.0;
    double l12 = 0.0;
    double l13 = 0.0;
    double l21 = 0.0;
    double l22 = 0.0;
    double l23 = 0.0;
    double l31 = 0.0;
    double l32 = 0.0;
    double l33 = 0.0;
    for (int kBasis=0; kBasis < numBasis; ++kBasis) {
      const int kB = kBasis*spaceDim;
      l11 += basisDeriv[iQ+kB  ] * disp[kB  ];
      l12 += basisDeriv[iQ+kB+1] * disp[kB  ];
      l13 += basisDeriv[iQ+kB+2] * disp[kB  ];
      l21 += basisDeriv[iQ+kB  ] * disp[kB+1];
      l22 += basisDeriv[iQ+kB+1] * disp[kB+1];
      l23 += basisDeriv[iQ+kB+2] * disp[kB+1];
      l31 += basisDeriv[iQ+kB  ] * disp[kB+2];
      l32 += basisDeriv[iQ+kB+1] * disp[kB+2];
      l33 += basisDeriv[iQ+kB+2] * disp[kB+2];
    } // for
    
    for (int iBasis=0, iQ=iQuad*numBasis*spaceDim;
	 iBasis < numBasis;
	 ++iBasis) {
      const int iB = iBasis*spaceDim;
      const double Nip = wt*basisDeriv[iQ+iB+0];
      const double Niq = wt*basisDeriv[iQ+iB+1];
      const double Nir = wt*basisDeriv[iQ+iB+2];
      for (int jBasis=0; jBasis < numBasis; ++jBasis) {
	const int jB = jBasis*spaceDim;
	const double Njp = basisDeriv[iQ+jB+0];
	const double Njq = basisDeriv[iQ+jB+1];
	const double Njr = basisDeriv[iQ+jB+2];

	// Generated using Maxima (see jacobian3d_lgdeform.wxm)
	const double Ki0j0 = 
	  l13*Nir*(l13*Njr*C3333 + 
		   (l12*Njr+l13*Njq)*C3323 + 
		   ((l11+1)*Njr+l13*Njp)*C3313 + 
		   ((l11+1)*Njq+l12*Njp)*C3312 + 
		   l12*Njq*C2233 + 
		   (l11+1)*Njp*C1133) + 
	  (l12*Nir+l13*Niq)*(l13*Njr*C3323 + 
			     (l12*Njr+l13*Njq)*C2323 + 
			     ((l11+1)*Njr+l13*Njp)*C2313 + 
			     l12*Njq*C2223 + 
			     ((l11+1)*Njq+l12*Njp)*C1223 + 
			     (l11+1)*Njp*C1123) + 
	  ((l11+1)*Nir+l13*Nip)*(l13*Njr*C3313 + 
				 (l12*Njr+l13*Njq)*C2313 + 
				 l12*Njq*C2213 + 
				 ((l11+1)*Njr+l13*Njp)*C1313 + 
				 ((l11+1)*Njq+l12*Njp)*C1213 + 
				 (l11+1)*Njp*C1113) + 
	  ((l11+1)*Niq+l12*Nip)*(l13*Njr*C3312 + 
				 l12*Njq*C2212 + 
				 (l12*Njr+l13*Njq)*C1223 + 
				 ((l11+1)*Njr+l13*Njp)*C1213 + 
				 ((l11+1)*Njq+l12*Njp)*C1212 + 
				 (l11+1)*Njp*C1112) + 
	  l12*Niq*(l13*Njr*C2233 + 
		   (l12*Njr+l13*Njq)*C2223 + 
		   l12*Njq*C2222 + 
		   ((l11+1)*Njr+l13*Njp)*C2213 + 
		   ((l11+1)*Njq+l12*Njp)*C2212 + 
		   (l11+1)*Njp*C1122) + 
	  (l11+1)*Nip*(l13*Njr*C1133 + 
		       (l12*Njr+l13*Njq)*C1123 + 
		       l12*Njq*C1122 + 
		       ((l11+1)*Njr+l13*Njp)*C1113 + 
		       ((l11+1)*Njq+l12*Njp)*C1112 + 
		       (l11+1)*Njp*C1111);

	const double Ki0j1 =
	  l13*Nir*(l23*Njr*C3333 + 
		   ((l22+1)*Njr+l23*Njq)*C3323 + 
		   (l21*Njr+l23*Njp)*C3313 + 
		   (l21*Njq+(l22+1)*Njp)*C3312 + 
		   (l22+1)*Njq*C2233 + 
		   l21*Njp*C1133) + 
	  (l12*Nir+l13*Niq)*(l23*Njr*C3323 + 
			     ((l22+1)*Njr+l23*Njq)*C2323 + 
			     (l21*Njr+l23*Njp)*C2313 + 
			     (l22+1)*Njq*C2223 + 
			     (l21*Njq+(l22+1)*Njp)*C1223 + 
			     l21*Njp*C1123) + 
	  ((l11+1)*Nir+l13*Nip)*(l23*Njr*C3313 + 
				 ((l22+1)*Njr+l23*Njq)*C2313 + 
				 (l22+1)*Njq*C2213 + 
				 (l21*Njr+l23*Njp)*C1313 + 
				 (l21*Njq+(l22+1)*Njp)*C1213 + 
				 l21*Njp*C1113) + 
	  ((l11+1)*Niq+l12*Nip)*(l23*Njr*C3312 + 
				 (l22+1)*Njq*C2212 + 
				 ((l22+1)*Njr+l23*Njq)*C1223 + 
				 (l21*Njr+l23*Njp)*C1213 + 
				 (l21*Njq+(l22+1)*Njp)*C1212 + 
				 l21*Njp*C1112) + 
	  l12*Niq*(l23*Njr*C2233 + 
		   ((l22+1)*Njr+l23*Njq)*C2223 + 
		   (l22+1)*Njq*C2222 + 
		   (l21*Njr+l23*Njp)*C2213 + 
		   (l21*Njq+(l22+1)*Njp)*C2212 + 
		   l21*Njp*C1122) + 
	  (l11+1)*Nip*(l23*Njr*C1133 + 
		       ((l22+1)*Njr+l23*Njq)*C1123 + 
		       (l22+1)*Njq*C1122 + 
		       (l21*Njr+l23*Njp)*C1113 + 
		       (l21*Njq+(l22+1)*Njp)*C1112 + 
		       l21*Njp*C1111);

	const double Ki0j2 =
	  l13*Nir*((l33+1)*Njr*C3333 + 
		   (l32*Njr+(l33+1)*Njq)*C3323 + 
		   (l31*Njr+(l33+1)*Njp)*C3313 + 
		   (l31*Njq+l32*Njp)*C3312 + 
		   l32*Njq*C2233 + 
		   l31*Njp*C1133) + 
	  (l12*Nir+l13*Niq)*((l33+1)*Njr*C3323 + 
			     (l32*Njr+(l33+1)*Njq)*C2323 + 
			     (l31*Njr+(l33+1)*Njp)*C2313 + 
			     l32*Njq*C2223 + 
			     (l31*Njq+l32*Njp)*C1223 + 
			     l31*Njp*C1123) + 
	  ((l11+1)*Nir+l13*Nip)*((l33+1)*Njr*C3313 + 
				 (l32*Njr+(l33+1)*Njq)*C2313 + 
				 l32*Njq*C2213 + 
				 (l31*Njr+(l33+1)*Njp)*C1313 + 
				 (l31*Njq+l32*Njp)*C1213 + 
				 l31*Njp*C1113) + 
	  ((l11+1)*Niq+l12*Nip)*((l33+1)*Njr*C3312 + 
				 l32*Njq*C2212 + 
				 (l32*Njr+(l33+1)*Njq)*C1223 + 
				 (l31*Njr+(l33+1)*Njp)*C1213 + 
				 (l31*Njq+l32*Njp)*C1212 + 
				 l31*Njp*C1112) + 
	  l12*Niq*((l33+1)*Njr*C2233 + 
		   (l32*Njr+(l33+1)*Njq)*C2223 + 
		   l32*Njq*C2222 + 
		   (l31*Njr+(l33+1)*Njp)*C2213 + 
		   (l31*Njq+l32*Njp)*C2212 + 
		   l31*Njp*C1122) + 
	  (l11+1)*Nip*((l33+1)*Njr*C1133 + 
		       (l32*Njr+(l33+1)*Njq)*C1123 + 
		       l32*Njq*C1122 + 
		       (l31*Njr+(l33+1)*Njp)*C1113 + 
		       (l31*Njq+l32*Njp)*C1112 + 
		       l31*Njp*C1111);

	const double Ki1j0 =
	  l23*Nir*(l13*Njr*C3333 + 
		   (l12*Njr+l13*Njq)*C3323 + 
		   ((l11+1)*Njr+l13*Njp)*C3313 + 
		   ((l11+1)*Njq+l12*Njp)*C3312 + 
		   l12*Njq*C2233+(l11+1)*Njp*C1133) + 
	  ((l22+1)*Nir+l23*Niq)*(l13*Njr*C3323 + 
				 (l12*Njr+l13*Njq)*C2323 + 
				 ((l11+1)*Njr+l13*Njp)*C2313 + 
				 l12*Njq*C2223 + 
				 ((l11+1)*Njq+l12*Njp)*C1223 + 
				 (l11+1)*Njp*C1123) + 
	  (l21*Nir+l23*Nip)*(l13*Njr*C3313 + 
			     (l12*Njr+l13*Njq)*C2313 + 
			     l12*Njq*C2213 + 
			     ((l11+1)*Njr + 
			      l13*Njp)*C1313 + 
			     ((l11+1)*Njq+l12*Njp)*C1213 + 
			     (l11+1)*Njp*C1113) + 
	  (l21*Niq+(l22+1)*Nip)*(l13*Njr*C3312 + 
				 l12*Njq*C2212 + 
				 (l12*Njr+l13*Njq)*C1223 + 
				 ((l11+1)*Njr+l13*Njp)*C1213 + 
				 ((l11+1)*Njq+l12*Njp)*C1212 + 
				 (l11+1)*Njp*C1112) + 
	  (l22+1)*Niq*(l13*Njr*C2233 + 
		       (l12*Njr+l13*Njq)*C2223 + 
		       l12*Njq*C2222 + 
		       ((l11+1)*Njr+l13*Njp)*C2213 + 
		       ((l11+1)*Njq+l12*Njp)*C2212 + 
		       (l11+1)*Njp*C1122) + 
	  l21*Nip*(l13*Njr*C1133 + 
		   (l12*Njr+l13*Njq)*C1123 + 
		   l12*Njq*C1122 + 
		   ((l11+1)*Njr+l13*Njp)*C1113 + 
		   ((l11+1)*Njq+l12*Njp)*C1112 + 
		   (l11+1)*Njp*C1111);

	const double Ki1j1 =
	  l23*Nir*(l23*Njr*C3333 + 
		   ((l22+1)*Njr+l23*Njq)*C3323 + 
		   (l21*Njr+l23*Njp)*C3313 + 
		   (l21*Njq+(l22+1)*Njp)*C3312 + 
		   (l22+1)*Njq*C2233 + 
		   l21*Njp*C1133) + 
	  ((l22+1)*Nir+l23*Niq)*(l23*Njr*C3323 + 
				 ((l22+1)*Njr+l23*Njq)*C2323 + 
				 (l21*Njr+l23*Njp)*C2313 + 
				 (l22+1)*Njq*C2223 + 
				 (l21*Njq+(l22+1)*Njp)*C1223 + 
				 l21*Njp*C1123) + 
	  (l21*Nir+l23*Nip)*(l23*Njr*C3313 + 
			     ((l22+1)*Njr+l23*Njq)*C2313 + 
			     (l22+1)*Njq*C2213 + 
			     (l21*Njr+l23*Njp)*C1313 + 
			     (l21*Njq+(l22+1)*Njp)*C1213 + 
			     l21*Njp*C1113) + 
	  (l21*Niq+(l22+1)*Nip)*(l23*Njr*C3312 + 
				 (l22+1)*Njq*C2212 + 
				 ((l22+1)*Njr+l23*Njq)*C1223 + 
				 (l21*Njr+l23*Njp)*C1213 + 
				 (l21*Njq+(l22+1)*Njp)*C1212 + 
				 l21*Njp*C1112) + 
	  (l22+1)*Niq*(l23*Njr*C2233 + 
		       ((l22+1)*Njr+l23*Njq)*C2223 + 
		       (l22+1)*Njq*C2222 + 
		       (l21*Njr+l23*Njp)*C2213 + 
		       (l21*Njq+(l22+1)*Njp)*C2212 + 
		       l21*Njp*C1122) + 
	  l21*Nip*(l23*Njr*C1133 + 
		   ((l22+1)*Njr+l23*Njq)*C1123 + 
		   (l22+1)*Njq*C1122 + 
		   (l21*Njr+l23*Njp)*C1113 + 
		   (l21*Njq+(l22+1)*Njp)*C1112 + 
		   l21*Njp*C1111);

	const double Ki1j2 =
	  l23*Nir*((l33+1)*Njr*C3333 + 
		   (l32*Njr+(l33+1)*Njq)*C3323 + 
		   (l31*Njr+(l33+1)*Njp)*C3313 + 
		   (l31*Njq+l32*Njp)*C3312 + 
		   l32*Njq*C2233 + 
		   l31*Njp*C1133) + 
	  ((l22+1)*Nir+l23*Niq)*((l33+1)*Njr*C3323 + 
				 (l32*Njr+(l33+1)*Njq)*C2323 + 
				 (l31*Njr+(l33+1)*Njp)*C2313 + 
				 l32*Njq*C2223 + 
				 (l31*Njq+l32*Njp)*C1223 + 
				 l31*Njp*C1123) + 
	  (l21*Nir+l23*Nip)*((l33+1)*Njr*C3313 +
			     (l32*Njr+(l33+1)*Njq)*C2313 +
			     l32*Njq*C2213 +
			     (l31*Njr+(l33+1)*Njp)*C1313 +
			     (l31*Njq+l32*Njp)*C1213 + 
			     l31*Njp*C1113) + 
	  (l21*Niq+(l22+1)*Nip)*((l33+1)*Njr*C3312 + 
				 l32*Njq*C2212 + 
				 (l32*Njr+(l33+1)*Njq)*C1223 + 
				 (l31*Njr+(l33+1)*Njp)*C1213 + 
				 (l31*Njq+l32*Njp)*C1212 + 
				 l31*Njp*C1112) +
	  (l22+1)*Niq*((l33+1)*Njr*C2233 + 
		       (l32*Njr+(l33+1)*Njq)*C2223 + 
		       l32*Njq*C2222 + 
		       (l31*Njr+(l33+1)*Njp)*C2213 + 
		       (l31*Njq+l32*Njp)*C2212 + 
		       l31*Njp*C1122) + 
	  l21*Nip*((l33+1)*Njr*C1133 + 
		   (l32*Njr+(l33+1)*Njq)*C1123 + 
		   l32*Njq*C1122 + 
		   (l31*Njr+(l33+1)*Njp)*C1113 + 
		   (l31*Njq+l32*Njp)*C1112+l31*Njp*C1111);

	const double Ki2j0 =
	  (l33+1)*Nir*(l13*Njr*C3333 + 
		       (l12*Njr+l13*Njq)*C3323 + 
		       ((l11+1)*Njr+l13*Njp)*C3313 + 
		       ((l11+1)*Njq+l12*Njp)*C3312 + 
		       l12*Njq*C2233 + 
		       (l11+1)*Njp*C1133) + 
	  (l32*Nir+(l33+1)*Niq)*(l13*Njr*C3323 + 
				 (l12*Njr+l13*Njq)*C2323 + 
				 ((l11+1)*Njr+l13*Njp)*C2313 + 
				 l12*Njq*C2223 + 
				 ((l11+1)*Njq+l12*Njp)*C1223 + 
				 (l11+1)*Njp*C1123) + 
	  (l31*Nir+(l33+1)*Nip)*(l13*Njr*C3313 + 
				 (l12*Njr+l13*Njq)*C2313 + 
				 l12*Njq*C2213 + 
				 ((l11+1)*Njr+l13*Njp)*C1313 + 
				 ((l11+1)*Njq+l12*Njp)*C1213 + 
				 (l11+1)*Njp*C1113) +
	  (l31*Niq+l32*Nip)*(l13*Njr*C3312 + 
			     l12*Njq*C2212 + 
			     (l12*Njr+l13*Njq)*C1223 + 
			     ((l11+1)*Njr+l13*Njp)*C1213 + 
			     ((l11+1)*Njq+l12*Njp)*C1212 + 
			     (l11+1)*Njp*C1112) + 
	  l32*Niq*(l13*Njr*C2233 +
		   (l12*Njr+l13*Njq)*C2223 + 
		   l12*Njq*C2222 + 
		   ((l11+1)*Njr+l13*Njp)*C2213 + 
		   ((l11+1)*Njq+l12*Njp)*C2212 + 
		   (l11+1)*Njp*C1122) + 
	  l31*Nip*(l13*Njr*C1133 + 
		   (l12*Njr+l13*Njq)*C1123 + 
		   l12*Njq*C1122 + 
		   ((l11+1)*Njr+l13*Njp)*C1113 + 
		   ((l11+1)*Njq+l12*Njp)*C1112 + 
		   (l11+1)*Njp*C1111);

	const double Ki2j1 =
	  (l33+1)*Nir*(l23*Njr*C3333 + 
		       ((l22+1)*Njr+l23*Njq)*C3323 + 
		       (l21*Njr+l23*Njp)*C3313 + 
		       (l21*Njq+(l22+1)*Njp)*C3312 + 
		       (l22+1)*Njq*C2233 + 
		       l21*Njp*C1133) + 
	  (l32*Nir+(l33+1)*Niq)*(l23*Njr*C3323 + 
				 ((l22+1)*Njr+l23*Njq)*C2323 + 
				 (l21*Njr+l23*Njp)*C2313 + 
				 (l22+1)*Njq*C2223 + 
				 (l21*Njq+(l22+1)*Njp)*C1223 + 
				 l21*Njp*C1123) + 
	  (l31*Nir+(l33+1)*Nip)*(l23*Njr*C3313 + 
				 ((l22+1)*Njr+l23*Njq)*C2313 + 
				 (l22+1)*Njq*C2213 + 
				 (l21*Njr+l23*Njp)*C1313 + 
				 (l21*Njq+(l22+1)*Njp)*C1213 + 
				 l21*Njp*C1113) + 
	  (l31*Niq+l32*Nip)*(l23*Njr*C3312 + 
			     (l22+1)*Njq*C2212 + 
			     ((l22+1)*Njr+l23*Njq)*C1223 + 
			     (l21*Njr+l23*Njp)*C1213 + 
			     (l21*Njq+(l22+1)*Njp)*C1212 + 
			     l21*Njp*C1112) + 
	  l32*Niq*(l23*Njr*C2233 + 
		   ((l22+1)*Njr+l23*Njq)*C2223 + 
		   (l22+1)*Njq*C2222 + 
		   (l21*Njr+l23*Njp)*C2213 + 
		   (l21*Njq+(l22+1)*Njp)*C2212 + 
		   l21*Njp*C1122) + 
	  l31*Nip*(l23*Njr*C1133 + 
		   ((l22+1)*Njr+l23*Njq)*C1123 + 
		   (l22+1)*Njq*C1122 + 
		   (l21*Njr+l23*Njp)*C1113 + 
		   (l21*Njq+(l22+1)*Njp)*C1112 + 
		   l21*Njp*C1111);

	const double Ki2j2 =
	  (l33+1)*Nir*((l33+1)*Njr*C3333 + 
		       (l32*Njr+(l33+1)*Njq)*C3323 + 
		       (l31*Njr+(l33+1)*Njp)*C3313 + 
		       (l31*Njq+l32*Njp)*C3312 + 
		       l32*Njq*C2233 + 
		       l31*Njp*C1133) + 
	  (l32*Nir+(l33+1)*Niq)*((l33+1)*Njr*C3323 + 
				 (l32*Njr+(l33+1)*Njq)*C2323 + 
				 (l31*Njr+(l33+1)*Njp)*C2313 + 
				 l32*Njq*C2223 + 
				 (l31*Njq+l32*Njp)*C1223 + 
				 l31*Njp*C1123) + 
	  (l31*Nir+(l33+1)*Nip)*((l33+1)*Njr*C3313 + 
				 (l32*Njr+(l33+1)*Njq)*C2313 + 
				 l32*Njq*C2213 + 
				 (l31*Njr+(l33+1)*Njp)*C1313 + 
				 (l31*Njq+l32*Njp)*C1213 + 
				 l31*Njp*C1113) + 
	  (l31*Niq+l32*Nip)*((l33+1)*Njr*C3312 + 
			     l32*Njq*C2212 + 
			     (l32*Njr+(l33+1)*Njq)*C1223 + 
			     (l31*Njr+(l33+1)*Njp)*C1213 + 
			     (l31*Njq+l32*Njp)*C1212 + 
			     l31*Njp*C1112) + 
	  l32*Niq*((l33+1)*Njr*C2233 + 
		   (l32*Njr+(l33+1)*Njq)*C2223 + 
		   l32*Njq*C2222 + 
		   (l31*Njr+(l33+1)*Njp)*C2213 + 
		   (l31*Njq+l32*Njp)*C2212 + 
		   l31*Njp*C1122) + 
	  l31*Nip*((l33+1)*Njr*C1133 + 
		   (l32*Njr+(l33+1)*Njq)*C1123 + 
		   l32*Njq*C1122 + 
		   (l31*Njr+(l33+1)*Njp)*C1113 + 
		   (l31*Njq+l32*Njp)*C1112 + 
		   l31*Njp*C1111);

	const double Knl = 
	  Nir*(Njr*s33+Njq*s23+Njp*s13) + 
	  Niq*(Njr*s23+Njq*s22+Njp*s12) + 
	  Nip*(Njr*s13+Njq*s12+Njp*s11);

	const int iBlock = iB * (numBasis*spaceDim);
	const int iBlock1 = (iB+1) * (numBasis*spaceDim);
	const int iBlock2 = (iB+2) * (numBasis*spaceDim);
	const int jBlock = jB;
	const int jBlock1 = jB+1;
	const int jBlock2 = jB+2;
	_cellMatrix[iBlock +jBlock ] += Ki0j0 + Knl;
	_cellMatrix[iBlock +jBlock1] += Ki0j1;
	_cellMatrix[iBlock +jBlock2] += Ki0j2;
	_cellMatrix[iBlock1+jBlock ] += Ki1j0;
	_cellMatrix[iBlock1+jBlock1] += Ki1j1 + Knl;
	_cellMatrix[iBlock1+jBlock2] += Ki1j2;
	_cellMatrix[iBlock2+jBlock ] += Ki2j0;
	_cellMatrix[iBlock2+jBlock1] += Ki2j1;
	_cellMatrix[iBlock2+jBlock2] += Ki2j2 + Knl;
      } // for
    } // for
  } // for
  PetscLogFlops(numQuadPts*(1+numBasis*(3+numBasis*(6*26+9))));
} // _elasticityJacobian3D

// ----------------------------------------------------------------------
// Calculate Green-Lagrange strain tensor at quadrature points of a 1-D cell.
void 
pylith::feassemble::IntegratorElasticityLgDeform::_calcTotalStrain1D(
					      double_array* strain,
					      const double_array& deform,
					      const int numQuadPts)
{ // _calcTotalStrain1D
  // Green-Lagrange strain tensor = 1/2 ( X^T X - I )
  // X: deformation tensor
  // I: identity matrix

  assert(0 != strain);

  const int dim = 1;
  const int strainSize = 1;
  assert(deform.size() == numQuadPts*dim*dim);
  assert(strain->size() == numQuadPts*strainSize);


  for (int iQuad=0; iQuad < numQuadPts; ++iQuad)
      (*strain)[iQuad] = 0.5*(deform[iQuad]*deform[iQuad] - 1.0);
} // _calcTotalStrain1D
  
// ----------------------------------------------------------------------
// Calculate Green-Lagrange strain tensor at quadrature points of a 2-D cell.
void 
pylith::feassemble::IntegratorElasticityLgDeform::_calcTotalStrain2D(
					      double_array* strain,
					      const double_array& deform,
					      const int numQuadPts)
{ // _calcTotalStrain2D
  // Green-Lagrange strain tensor = 1/2 ( X^T X - I )
  // X: deformation tensor
  // I: identity matrix

  assert(0 != strain);

  const int dim = 2;
  const int deformSize = dim*dim;
  const int strainSize = 3;
  assert(deform.size() == numQuadPts*deformSize);
  assert(strain->size() == numQuadPts*strainSize);

  for (int iQuad=0, iDeform=0, iStrain=0;
       iQuad < numQuadPts;
       ++iQuad, iDeform+=deformSize, iStrain+=strainSize) {
    (*strain)[iStrain  ] =
      0.5 * (deform[iDeform  ]*deform[iDeform  ] + 
	     deform[iDeform+2]*deform[iDeform+2] - 1.0);
    (*strain)[iStrain+1] =
      0.5 * (deform[iDeform+1]*deform[iDeform+1] + 
	     deform[iDeform+3]*deform[iDeform+3] - 1.0);
    (*strain)[iStrain+2] =
      0.5 * (deform[iDeform  ]*deform[iDeform+1] + 
	     deform[iDeform+2]*deform[iDeform+3]);
  } // for
} // _calcTotalStrain2D
  
// ----------------------------------------------------------------------
// Calculate Green-Lagrange strain tensor at quadrature points of a 3-D cell.
void 
pylith::feassemble::IntegratorElasticityLgDeform::_calcTotalStrain3D(
					      double_array* strain,
					      const double_array& deform,
					      const int numQuadPts)
{ // _calcTotalStrain3D
  // Green-Lagrange strain tensor = 1/2 ( X^T X - I )
  // X: deformation tensor
  // I: identity matrix

  assert(0 != strain);

  const int dim = 3;
  const int deformSize = dim*dim;
  const int strainSize = 6;
  assert(deform.size() == numQuadPts*dim*dim);
  assert(strain->size() == numQuadPts*strainSize);

  for (int iQuad=0, iDeform=0, iStrain=0;
       iQuad < numQuadPts;
       ++iQuad, iDeform+=deformSize, iStrain+=strainSize) {
    (*strain)[iStrain  ] =
      0.5 * (deform[iDeform  ]*deform[iDeform  ] +
	     deform[iDeform+3]*deform[iDeform+3] +
	     deform[iDeform+6]*deform[iDeform+6] - 1.0);
    (*strain)[iStrain+1] =
      0.5 * (deform[iDeform+1]*deform[iDeform+1] +
	     deform[iDeform+4]*deform[iDeform+4] +
	     deform[iDeform+7]*deform[iDeform+7] - 1.0);
    (*strain)[iStrain+2] =
      0.5 * (deform[iDeform+2]*deform[iDeform+2] +
	     deform[iDeform+5]*deform[iDeform+5] +
	     deform[iDeform+8]*deform[iDeform+8] - 1.0);
    (*strain)[iStrain+3] =
      0.5 * (deform[iDeform  ]*deform[iDeform+1] +
	     deform[iDeform+3]*deform[iDeform+4] +
	     deform[iDeform+6]*deform[iDeform+7]);
    (*strain)[iStrain+4] =
      0.5 * (deform[iDeform+1]*deform[iDeform+2] +
	     deform[iDeform+4]*deform[iDeform+5] +
	     deform[iDeform+7]*deform[iDeform+8]);
    (*strain)[iStrain+5] =
      0.5 * (deform[iDeform+0]*deform[iDeform+2] +
	     deform[iDeform+3]*deform[iDeform+5] +
	     deform[iDeform+6]*deform[iDeform+8]);
  } // for
} // _calcTotalStrain3D
  
// ----------------------------------------------------------------------
// Calculate deformation tensor.
void 
pylith::feassemble::IntegratorElasticityLgDeform::_calcDeformation(
					      double_array* deform,
					      const double_array& basisDeriv,
					      const double_array& vertices,
					      const double_array& disp,
					      const int numBasis,
					      const int numQuadPts,
					      const int dim)
{ // _calcDeformation
  assert(0 != deform);

  assert(basisDeriv.size() == numQuadPts*numBasis*dim);
  assert(disp.size() == numBasis*dim);

  const int deformSize = dim*dim;

  (*deform) = 0.0;
  for (int iQuad=0; iQuad < numQuadPts; ++iQuad)
    for (int iBasis=0, iQ=iQuad*numBasis*dim; iBasis < numBasis; ++iBasis)
      for (int iDim=0, indexD=0; iDim < dim; ++iDim)
	for (int jDim=0; jDim < dim; ++jDim, ++indexD)
	  (*deform)[iQuad*deformSize+indexD] += 
	    basisDeriv[iQ+iBasis*dim+jDim] *
	    (vertices[iBasis*dim+iDim] + disp[iBasis*dim+iDim]);

} // _calcDeformation
  

// End of file 