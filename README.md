# g4simple
Perhaps the simplest fully-featured G4 application.

Based on one-file simulation by Jason Detwiler.

Physics List: uses Geant4's 
[named physics lists](https://geant4.web.cern.ch/node/155), 
set them using macro commands (see example run.mac)

Generator: uses Geant4's 
[GPS](http://geant4-userdoc.web.cern.ch/geant4-userdoc/UsersGuides/ForApplicationDeveloper/html/GettingStarted/generalParticleSource.html). 
Set it up using macro commands (see example run.mac).

Geometry: uses 
[GDML](http://lcgapp.cern.ch/project/simu/framework/GDML/doc/GDMLmanual.pdf) 
(see example run.mac). Also plan to add a G4tgrLineProcessor-based geometry
input scheme.

Output: uses Geant4's analysis manager (root, hdf5, xml, csv), with several
configurable options for output format, sensitive volumes, etc (see example
run.mac).

Visualization: uses avaialable options in your G4 build (see example vis.mac).

Postprocessing: you will want to postprocess the output to apply the detector
response. See example code that runs on the output of run.mac.


See similar project by Jing Liu at https://github.com/jintonic/gears
