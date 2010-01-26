#!/usr/bin/env python
#
# ----------------------------------------------------------------------
#
#                           Brad T. Aagaard
#                        U.S. Geological Survey
#
# <LicenseText>
#
# ----------------------------------------------------------------------
#

## @file pylith/materials/MaxwellPlaneStrain.py
##
## @brief Python object implementing plane strain linear Maxwell
## viscoelastic material.
##
## Factory: material.

from ElasticMaterial import ElasticMaterial
from materials import MaxwellPlaneStrain as ModuleMaxwellPlaneStrain

# MaxwellPlaneStrain class
class MaxwellPlaneStrain(ElasticMaterial, ModuleMaxwellPlaneStrain):
  """
  Python object implementing plane strain linear Maxwell viscoelastic
  material.

  Factory: material.
  """

  # PUBLIC METHODS /////////////////////////////////////////////////////

  def __init__(self, name="maxwellplanestrain"):
    """
    Constructor.
    """
    ElasticMaterial.__init__(self, name)
    self.availableFields = \
        {'vertex': \
           {'info': [],
            'data': []},
         'cell': \
           {'info': ["mu", "lambda", "density", "maxwell_time"],
            'data': ["total_strain", "viscous_strain", "stress"]}}
    self._loggingPrefix = "MaMx2D "
    return


  def _createModuleObj(self):
    """
    Call constructor for module object for access to C++ object.
    """
    ModuleMaxwellPlaneStrain.__init__(self)
    return
  

# FACTORIES ////////////////////////////////////////////////////////////

def material():
  """
  Factory associated with MaxwellPlaneStrain.
  """
  return MaxwellPlaneStrain()


# End of file 