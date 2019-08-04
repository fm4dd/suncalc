# suncalc

## Default data creation

```
fm@ubu1804:~/suncalc$ ./suncalc
No arguments, creating dataset with program defaults.
See ./suncalc -h for further usage.
Created new output folder [./tracker-data]
Create dataset file [./tracker-data/dset.txt]
Create srs bin file [./tracker-data/srs-2019.bin]
Create srs csv file [./tracker-data/srs-2019.csv]
Create day csv file [./tracker-data/20190728.csv]
Create day bin file [./tracker-data/20190728.bin]
```

## Usage
```
fm@ubu1804:~/suncalc$ ./suncalc -h
suncalc v1.2

Usage: ./suncalc [-x <longitude>] [-y <latitude>] [-t <timezone>] [-i <interval>] [-p period nd|nm|nq|ny|td|tm|tq|ty] [-o outfolder] [-v]

Command line parameters have the following format:
   -x   location longitude, Example: -x 139.628999 (default)
   -y   location latitude, Example: -y 35.610381 (default)
   -t   location timezone offset in hours, Example: -t +9 (default)
   -i   calculation interval in seconds between 60 and 3600, Example -i 60 (default)
        the value must be a multiple of 86400 (1 day): e.g. 300, 600, 900, 1200, 1800
   -p   calculation period:
           nd = next day (tomorrow, default)
           nm = next month (2M)
           nq = next quarter
           ny = next year (starting Jan-1 until Dec-31, 23M)
           td = this day (today, 112K)
           tm = this month (starting today, 2M)
           tq = this quarter (starting today)
           ty = this year (starting Jan-1 until Dec-31, 23M)
           2y = two years (starting this year, 46M)
           tf = ten years forward (starting this year, 230M)
   -o   output folder, Example: -o ./tracker-data (default)
   -h   display this message
   -v   enable debug output

Usage examples:
./suncalc -x 139.628999 -y 35.610381 -t +9 -i 600 -p nd -o ./tracker-data -v

zip -r tracker-data.zip tracker-data
```
## Library Reference

This program currently uses NREL's Solar Position Algorithm (SPA) functions.
The SPA license, source code and reference information can be found at
https://midcdmz.nrel.gov/spa/.

#### NOTICE 

Copyright © 2008-2011 Alliance for Sustainable Energy, LLC, All Rights Reserved
The Solar Position Algorithm ("Software") is code in development prepared by employees of the Alliance for Sustainable Energy, LLC, (hereinafter the "Contractor"), under Contract No. DE-AC36-08GO28308 ("Contract") with the U.S. Department of Energy (the "DOE"). The United States Government has been granted for itself and others acting on its behalf a paid-up, non-exclusive, irrevocable, worldwide license in the Software to reproduce, prepare derivative works, and perform publicly and display publicly. Beginning five (5) years after the date permission to assert copyright is obtained from the DOE, and subject to any subsequent five (5) year renewals, the United States Government is granted for itself and others acting on its behalf a paid-up, non-exclusive, irrevocable, worldwide license in the Software to reproduce, prepare derivative works, distribute copies to the public, perform publicly and display publicly, and to permit others to do so. If the Contractor ceases to make this computer software available, it may be obtained from DOE's Office of Scientific and Technical Information's Energy Science and Technology Software Center (ESTSC) at P.O. Box 1020, Oak Ridge, TN 37831-1020. THIS SOFTWARE IS PROVIDED BY THE CONTRACTOR "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE CONTRACTOR OR THE U.S. GOVERNMENT BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER, INCLUDING BUT NOT LIMITED TO CLAIMS ASSOCIATED WITH THE LOSS OF DATA OR PROFITS, WHICH MAY RESULT FROM AN ACTION IN CONTRACT, NEGLIGENCE OR OTHER TORTIOUS CLAIM THAT ARISES OUT OF OR IN CONNECTION WITH THE ACCESS, USE OR PERFORMANCE OF THIS SOFTWARE. 

As required by the authors, spa.c is not redistributed.
