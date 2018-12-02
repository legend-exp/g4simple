# draw energy histograms from each detector

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
plt.style.use("mplstyle.txt")

df =  pd.read_hdf("processed.hdf5", key="procdf")

bins = np.arange(0, 1.1, 0.001)
plt.figure()
for det, detdf in df.groupby('detID'):
    detdf["energy"].hist(bins=bins, histtype="step", label="detector {}".format(det))
plt.xlabel("energy [MeV]")
plt.gca().set_xlim(0, 1.1)
plt.gca().set_ylim(0.1, plt.gca().get_ylim()[1])
plt.gca().grid(False)
plt.yscale("log")
plt.legend(frameon=False, loc='upper left')
plt.show()

