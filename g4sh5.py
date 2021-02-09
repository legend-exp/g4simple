import sys, h5py
import numpy as np
import pandas as pd

'''
g4sh5 is a python module of convenience functions for reading step-wise g4simple
output stored in hdf5 files
'''


def get_n_rows(g4sntuple):
    ''' get the number of rows (steps) in a step-wise g4sntuple

    Parameters
    ----------
    g4sntuple : h5py.Dataset
        The g4simple ntuple. Should be written in step-wise mode.
    
    Returns
    -------
    n_rows : int
        The number of rows (steps) in g4sntuple

    Example
    -------
    >>> g4sfile = h5py.File('g4simpleout.hdf5', 'r')
    >>> g4sntuple = g4sfile['default_ntuples/g4sntuple']
    >>> get_n_rows(g4sntuple)
    227539 # may vary
    '''
    return g4sntuple['event/pages'].shape[0]


def get_datasets(g4sntuple, fields):
    ''' get a dictionary of hdf5 datasets for the specified fields

    Note: performs no data reads from the file. Actual reads are performed when
    you subsequently access / use the returned datasets.

    Parameters
    ----------
    g4sntuple : h5py.Dataset
        The g4simple ntuple. Should be written in step-wise mode.
    fields : list of str
        A list of names of the fields to be accessed in the ntuple
    
    Returns
    -------
    datasets : dict of h5py datasets
        A dictionary of the requested h5py datasets, keyed by their names

    Example
    -------
    >>> g4sfile = h5py.File('g4simpleout.hdf5', 'r')
    >>> g4sntuple = g4sfile['default_ntuples/g4sntuple']
    >>> datasets = get_datasets(g4sntuple, ['event', 'step', 'pid', 'KE'])
    >>> datasets['step'][:10]
    array([0, 0, 0, 0, 1, 0, 5, 0, 1, 0], dtype=int32) # may vary
    '''
    datasets = {}
    for field in fields: datasets[field] = g4sntuple[field]['pages']
    return datasets


def get_dataframe(datasets, selection=None):
    ''' get a pandas dataframe from selected data in the provided datasets

    Note 1: only reads selected data from disk. If selection = None, all data is
    read into member.

    Note 2: reads in data, then copies it into ndarrays used to build the
    dataframe. To avoid the copying, instead of using this function, work with
    the datasets directly, or force single reads into your own buffers using
    hp5.Dataset.read_direct

    Parameters
    ----------
    datasets : dict of h5py.Datasets
        The datasets from which to bulid the dataframe, as returned by
        get_datasets
    selection : slice object (optional)
        A slice object specifing a read of just a subset of the data. Allows one
        to cycle through data in a file too large to fit in memory
    
    Returns
    -------
    dataframe : pandas.DataFrame
        A pandas dataframe containing the selected data from the requested
        datasets

    Example
    -------
    >>> g4sfile = h5py.File('g4simpleout.hdf5', 'r')
    >>> g4sntuple = g4sfile['default_ntuples/g4sntuple']
    >>> datasets = get_datasets(g4sntuple, ['event', 'step', 'pid', 'x', 'y', 'z'])
    >>> dataframe = get_dataframe(datasets, slice(10,15))
    >>> dataframe
       event  step  pid         x          y          z
    0      3     1   11 -24.25767 -13.368400  31.085745
    1      3     0   11 -24.25767 -13.368400  31.085744
    2      3     1   11 -24.25767 -13.368401  31.085745
    3      3     0   11 -24.25767 -13.368400  31.085744
    4      3     1   11 -24.25767 -13.368400  31.085744   # may vary
    '''
    array_dict = {}
    for key, ds in datasets.items():
        if selection is None: array_dict[key] = np.array(ds)
        else: array_dict[key] = np.array(ds[selection])
    return pd.DataFrame(array_dict)
