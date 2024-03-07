# Configuration file for the Sphinx documentation builder.
#
# This file only contains a selection of the most common options. For a full
# list see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Path setup --------------------------------------------------------------

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
#
import os
import sys

ba_root = os.getenv('BALLISTICA_ROOT') + '/'
assets_dirs : dict = {'ba_data':'src/assets/ba_data/python/', 
                          'dummy_modules':'build/dummymodules/', 
                          'efro_tools':'tools/', # for efro and bacommon package
                          }
    
sys.path.append(os.path.abspath(ba_root+assets_dirs['ba_data']))
sys.path.append(os.path.abspath(ba_root+assets_dirs['dummy_modules']))
sys.path.append(os.path.abspath(ba_root+assets_dirs['efro_tools']))

# -- Options for HTML output -------------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
html_theme = 'sphinx_rtd_theme' #'python_docs_theme' #"groundwork"


# -- Project information -----------------------------------------------------

keep_warnings = True # Supressing warnings
project = 'ballistica-bombsquad'

copyright = '2024, Efroemling'
author = 'Efroemling'

# The full version, including alpha/beta/rc tags
# TODO: make this update from some variable
version = '1.7.33'
release = '43241'


# -- General configuration ---------------------------------------------------

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
autosummary_generate = True
extensions = ["sphinx.ext.napoleon", # https://stackoverflow.com/questions/45880348/how-to-remove-the-cause-of-an-unexpected-indentation-warning-when-generating-cod
               "sphinx.ext.autodoc",
]

# Add any paths that contain templates here, relative to this directory.
templates_path = ['_templates']

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This pattern also affects html_static_path and html_extra_path.
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']



# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ['_static']
