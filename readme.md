# suncalc

## Default data creation

```
fm@ubu1804:~/suncalc$ ./suncalc
No arguments, creating dataset with program defaults.
See ./suncalc -h for further usage.
Created new output folder [./tracker-data]
Create dataset file   [./tracker-data/dset.txt]
Create srs bin file [./tracker-data/srs-2019.bin]
Create srs csv file [./tracker-data/srs-2019.csv]
Create day csv file [./tracker-data/20190728.csv]
Create day bin file [./tracker-data/20190728.bin]
```

## Usage
```
fm@ubu1804:~/suncalc$ ./suncalc -h
suncalc v1.2

Usage: ./suncalc [-x <longitude>] [-y <longitude>] [-t <timezone>] [-i <interval>] [-p period nd|nm|nq|ny|td|tm|tq|ty] [-o outfolder] [-v]

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

