# pg_smile
The C++ code that drives the TAGMI tool (http://iwmi-tagmi.cloudapp.net/index.php). See description of the interactive interface programming at: https://github.com/douglaswang/TAGMI

## About
This repository contains the C++ code that connects the SMILE library for running the Bayesian network (https://dslpitt.org/dsl/genie_smile.html) to a PostgreSQL database with PostGIS plugin to allow spatial mapping of data in the database.

The program provides the function allows PostgreSQL to call on the SMILE library code and therefore to run the Bayes model automatically in succession for all units in a study area (e.g. districts in a country), and delivers results to the TAGMI interface be displayed as a spatial map.
