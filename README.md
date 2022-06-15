# g4simple
Perhaps the simplest fully-featured G4 application.

Based on one-file simulation by Jason Detwiler.

## Installation:
Compile geant4 with GDML support (and optionally HDF5 support), then do:

```make```

## Physics List: 
uses Geant4's 
[named physics lists](https://geant4.web.cern.ch/node/155), 
set them using macro commands (see example run.mac)

## Generator: 
uses Geant4's 
[GPS](http://geant4-userdoc.web.cern.ch/geant4-userdoc/UsersGuides/ForApplicationDeveloper/html/GettingStarted/generalParticleSource.html). 
Set it up using macro commands (see example run.mac).

## Geometry: 
uses 
[GDML](http://gdml.web.cern.ch/GDML) 
(see example run.mac). Can use materials from Geant4's
[NIST Material Database](http://geant4-userdoc.web.cern.ch/geant4-userdoc/UsersGuides/ForApplicationDeveloper/html/Appendix/materialNames.html) (note: the GDML parser will complain that the materials have not been defined, but Geant4 will still run without error).
Also supports Geant4's [text file geometry scheme](http://geant4.cern.ch/files/geant4/collaboration/working_groups/geometry/docs/textgeom/textgeom.pdf).

## Output: 
uses Geant4's analysis manager (root, hdf5, xml, csv), with several
configurable options for output format, sensitive volumes (including [regex-based pattern matching / replacement](http://www.cplusplus.com/reference/regex/ECMAScript)), etc. (see example
run.mac). Records event/track/step numbers, 
[PIDs](http://pdg.lbl.gov/2018/reviews/rpp2018-rev-monte-carlo-numbering.pdf) 
(see also the python package [particle](https://pypi.org/project/Particle/)),
positions, energies, etc.

## Other macro commands
see the example run.mac, or run g4simple and type "help" and choose the g4simple option. Note: more commands become available after setting a physics list.

## Visualization
uses available options in your G4 build (see example vis.mac).

## Postprocessing
you will want to postprocess the output to apply e.g. detector
response. See example code that runs on the output of run.mac.

## Ouput parameters:

Output parameter values are in [Geant4 internal units](https://geant4.web.cern.ch/sites/geant4.web.cern.ch/files//geant4/collaboration/working_groups/electromagnetic/gallery/units/SystemOfUnits.html):
* Length in mm
* Time in ns
* Energy in MeV

The available output parameters are:
* int nEvents: number of events run in the simulation
* int event: event number of the recorded step
* int pid: particle ID of the particle making the step
* int trackID: track ID for the recorded step
* int parentID: parent track ID of the track being recorded
* int step: current step number
* double KE: kinetic energy at the start (step=0) or end (step>0) of the step
* double Edep: energy deposited along the step
* double x: global x coordinate of the pre- (step=0) or post- (step>0) step position
* double y: global y coordinate of the pre- (step=0) or post- (step>0) step position
* double z: global z coordinate of the pre- (step=0) or post- (step>0) step position
* double lx: local x coordinate of the pre- (step=0) or post- (step>0) step position
* double ly: local y coordinate of the pre- (step=0) or post- (step>0) step position
* double lz: local z coordinate of the pre- (step=0) or post- (step>0) step position
* double pdx: global x component of the pre- (step=0) or post- (step>0) step momentum
* double pdy: global x component of the pre- (step=0) or post- (step>0) step momentum
* double pdz: global x component of the pre- (step=0) or post- (step>0) step momentum
* double t: global time of the pre- (step=0) or post- (step>0) step point
* int volID: the ID of the volume being traversed (user-defined) (see example run.mac)
* int iRep: the replica number of the volume being traversed

You can turn on and off different output fields using the macro silenceOutput/addOutput macro commands (see examples in run.mac).

Note: Each pair of rows in the output corresponds to the pre- and post-step point of the corresponding step, with the step number, Edep, and volume traversed for the step recorded along with the post-step. Note that this means that volume ID changes occur at the first step point *inside* a volume, not at the point recorded on the boundary. This may be counter-intuitive for those familiar with G4, where the step point on the boundary is marked as being "in" the volume being entered.

For every energy-depositing particle traversing a sensitive volume, the g4simple output will include the step info for the first step point in the volume. If the previous volume was not a sensitive volume, that step will have Edep = 0.

## See Also
Similar project by Jing Liu at https://github.com/jintonic/gears
