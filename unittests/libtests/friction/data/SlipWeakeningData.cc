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

#include "SlipWeakeningData.hh"

const int pylith::friction::SlipWeakeningData::_numLocs = 2;

const int pylith::friction::SlipWeakeningData::_numProperties = 3;

const int pylith::friction::SlipWeakeningData::_numStateVars = 0;

const int pylith::friction::SlipWeakeningData::_numDBProperties = 3;

const int pylith::friction::SlipWeakeningData::_numDBStateVars = 0;

const int pylith::friction::SlipWeakeningData::_numPropsVertex = 3;

const int pylith::friction::SlipWeakeningData::_numVarsVertex = 0;

const double pylith::friction::SlipWeakeningData::_lengthScale =   1.00000000e+03;

const double pylith::friction::SlipWeakeningData::_timeScale =   1.00000000e+00;

const double pylith::friction::SlipWeakeningData::_pressureScale =   2.25000000e+10;

const double pylith::friction::SlipWeakeningData::_densityScale =   1.00000000e+03;

const int pylith::friction::SlipWeakeningData::_numPropertyValues[] = {
1,
};

const int* pylith::friction::SlipWeakeningData::_numStateVarValues = 0;

const char* pylith::friction::SlipWeakeningData::_dbPropertyValues[] = {
"static-coefficient",
"dynamic-coefficient",
"slip-weakeneing-parameter",
};

const char** pylith::friction::SlipWeakeningData::_dbStateVarValues = 0;

const double pylith::friction::SlipWeakeningData::_dbProperties[] = {
  0.6,
  0.6,
  0.8,
  0.8,
  0.7,
  0.7,
};

const double* pylith::friction::SlipWeakeningData::_dbStateVars = 0;

const double pylith::friction::SlipWeakeningData::_properties[] = {
  0.6,
  0.6,
  0.6,
  0.6,
  0.6,
  0.6,
};

const double* pylith::friction::SlipWeakeningData::_stateVars = 0;

const double pylith::friction::SlipWeakeningData::_propertiesNondim[] = {
   0.6,
   0.6,
};

const double* pylith::friction::SlipWeakeningData::_stateVarsNondim = 0;

const double pylith::friction::SlipWeakeningData::_friction[] = {
  1.32,
   0.0,
};

const double pylith::friction::SlipWeakeningData::_slip[] = {
  0.12,
  0.25,
};

const double pylith::friction::SlipWeakeningData::_slipRate[] = {
  0.74,
  0.64,
};

const double pylith::friction::SlipWeakeningData::_normalTraction[] = {
  -2.2,
  0.8,
};

const double* pylith::friction::SlipWeakeningData::_stateVarsUpdated = 0;

pylith::friction::SlipWeakeningData::SlipWeakeningData(void)
{ // constructor
  numLocs = _numLocs;
  numProperties = _numProperties;
  numStateVars = _numStateVars;
  numDBProperties = _numDBProperties;
  numDBStateVars = _numDBStateVars;
  numPropsVertex = _numPropsVertex;
  numVarsVertex = _numVarsVertex;
  lengthScale = _lengthScale;
  timeScale = _timeScale;
  pressureScale = _pressureScale;
  densityScale = _densityScale;
  numPropertyValues = const_cast<int*>(_numPropertyValues);
  numStateVarValues = const_cast<int*>(_numStateVarValues);
  dbPropertyValues = const_cast<char**>(_dbPropertyValues);
  dbStateVarValues = const_cast<char**>(_dbStateVarValues);
  dbProperties = const_cast<double*>(_dbProperties);
  dbStateVars = const_cast<double*>(_dbStateVars);
  properties = const_cast<double*>(_properties);
  stateVars = const_cast<double*>(_stateVars);
  propertiesNondim = const_cast<double*>(_propertiesNondim);
  stateVarsNondim = const_cast<double*>(_stateVarsNondim);
  friction = const_cast<double*>(_friction);
  slip = const_cast<double*>(_slip);
  slipRate = const_cast<double*>(_slipRate);
  normalTraction = const_cast<double*>(_normalTraction);
  stateVarsUpdated = const_cast<double*>(_stateVarsUpdated);
} // constructor

pylith::friction::SlipWeakeningData::~SlipWeakeningData(void)
{}


// End of file