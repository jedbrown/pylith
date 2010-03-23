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

#include <portinfo>

#include "TestFaultCohesiveDynTet4.hh" // Implementation of class methods

#include "data/CohesiveDynDataTet4.hh" // USES CohesiveDynDataTet4

#include "pylith/topology/SubMesh.hh" // USES SubMesh
#include "pylith/feassemble/Quadrature.hh" // USES Quadrature<SubMesh>
#include "pylith/feassemble/GeometryTri3D.hh" // USES GeometryTri3D

// ----------------------------------------------------------------------
CPPUNIT_TEST_SUITE_REGISTRATION( pylith::faults::TestFaultCohesiveDynTet4 );

// ----------------------------------------------------------------------
// Setup testing data.
void
pylith::faults::TestFaultCohesiveDynTet4::setUp(void)
{ // setUp
  TestFaultCohesiveDyn::setUp();
  _data = new CohesiveDynDataTet4();

  CPPUNIT_ASSERT(0 != _quadrature);
  feassemble::GeometryTri3D geometry;
  _quadrature->refGeometry(&geometry);
  
  _flipFault = true;
} // setUp


// End of file 