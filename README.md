# g4simple
Perhaps the simplest fully-featured G4 application.

Based on one-file simulation by Jason Detwiler.

Physics List: uses Geant4's named physics lists, set them using macro commands 
(see example in run.mac)

Generator: uses Geant4's GPS. Set it up using macro commands (see example in
run.mac)

Geometry: uses g4gdml (see example in run.mac). Also plan to add a
G4tgrLineProcessor-based geometry input scheme.

Output: uses Geant4's analysis manager (root, hdf5, xml, csv), with several
configurable options for output format, sensitive volumes, etc (see example in
run.mac)

Visualization: uses avaialable options in your G4 build (see example in
visualize.mac)

Postprocessing: you will want to postprocess the output to apply the detector
response. See example code in PostProcExamples that runs on the output of
run.mac.


See similar project by Jing Liu at https://github.com/jintonic/gears
