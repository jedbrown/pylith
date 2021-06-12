// -*- C++ -*-
//
// ======================================================================
//
// Brad T. Aagaard, U.S. Geological Survey
// Charles A. Williams, GNS Science
// Matthew G. Knepley, University of Chicago
//
// This code was developed as part of the Computational Infrastructure
// for Geodynamics (http://geodynamics.org).
//
// Copyright (c) 2010-2017 University of California, Davis
//
// See COPYING for license information.
//
// ======================================================================
//

#include <portinfo>

#include "MeshIOPETSc.hh" // implementation of class methods

#include "MeshBuilder.hh" // USES MeshBuilder
#include "pylith/topology/Mesh.hh" // USES Mesh

#include "pylith/utils/array.hh" // USES scalar_array, int_array, string_vector
#include "spatialdata/utils/LineParser.hh" // USES LineParser

#include "pylith/utils/journals.hh" // USES PYLITH_COMPONENT_*

#include <iomanip> // USES setw(), setiosflags(), resetiosflags()
#include <strings.h> // USES strcasecmp()
#include <cassert> // USES assert()
#include <fstream> // USES std::ifstream, std::ofstream
#include <stdexcept> // USES std::runtime_error
#include <sstream> // USES std::ostringstream
#include <typeinfo> // USES std::typeid

// ---------------------------------------------------------------------------------------------------------------------
namespace pylith {
    namespace meshio {
        class _MeshIOPETSc {
public:

            static const char* groupTypeNames[];
        }; // _MeshIOPETSc
        const char* _MeshIOPETSc::_MeshIOPETSc::groupTypeNames[2] = {
            "vertices",
            "cells",
        };
    } // meshio
} // pylith

// ---------------------------------------------------------------------------------------------------------------------
// Constructor
pylith::meshio::MeshIOPETSc::MeshIOPETSc(void) :
    _useIndexZero(true) { // constructor
    PyreComponent::setName("meshiopetsc");
} // constructor


// ---------------------------------------------------------------------------------------------------------------------
// Destructor
pylith::meshio::MeshIOPETSc::~MeshIOPETSc(void) { // destructor
    deallocate();
} // destructor


// ---------------------------------------------------------------------------------------------------------------------
// Deallocate PETSc and local data structures.
void
pylith::meshio::MeshIOPETSc::deallocate(void) { // deallocate
    PYLITH_METHOD_BEGIN;

    MeshIO::deallocate();

    PYLITH_METHOD_END;
} // deallocate


// ---------------------------------------------------------------------------------------------------------------------
// Read mesh.
void
pylith::meshio::MeshIOPETSc::_read(void) { // _read
    PYLITH_METHOD_BEGIN;
    PYLITH_COMPONENT_DEBUG("_read()");
    
    assert(_mesh);

    MPI_Comm comm = _mesh->comm();

    PetscDM dm = NULL;
    DMCreate(comm, &dm);
    DMSetType(dm, DMPLEX);
    DMSetFromOptions(dm);
    _mesh->dmMesh(dm);

    PYLITH_METHOD_END;
} // read


// ---------------------------------------------------------------------------------------------------------------------
// Write mesh to file.
void
pylith::meshio::MeshIOPETSc::_write(void) const {  } // write


