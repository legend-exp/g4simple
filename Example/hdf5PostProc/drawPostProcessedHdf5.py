# draw energy histograms from each detector

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
plt.style.use("mplstyle.txt")

df =  pd.read_hdf("processed.hdf5", key="procdf")

bins = np.arange(0, 4000, 1)
plt.figure()
for det, detdf in df.groupby('detID'):
    (detdf["energy"]*1000).hist(bins=bins, histtype="step", label="detector {}".format(det))
plt.xlabel("energy [keV]")
plt.gca().set_xlim(0, 4100)
plt.gca().set_ylim(0.1, plt.gca().get_ylim()[1])
plt.gca().grid(False)
plt.yscale("log")
plt.legend(frameon=False, loc='upper right')
plt.show()

