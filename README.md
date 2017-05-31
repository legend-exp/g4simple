# g4simple
Under development

Based on one-file simulation by Jason Detwiler.

Physics List: uses Geant4's named physics lists, set them using macro commands 
(see example)

Generator: uses Geant4's GPS. Set it up using macro commands (see example)

Geometry: currently a specific geometry, will be converted to GDML-based. Also
plan to add a G4tgrLineProcessor-based geometry input scheme.

Output: currently specific to the implemented geometry, will be converted 
to a generic scheme (first ROOT, then JSON tracks, then ultimately maybe HDF5)


See similar project by Jing Liu at https://github.com/jintonic/gears
