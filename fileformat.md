# Suncalc File Format Specification

Below is the file spec design I created for the data files that are read by Suntracker 2
revision 2 and Suntracker 2 revision 3 devices. Initially I thought I can generate CSV files,
but it turns out reading strings is a extra burden on the Arduino, resulting in too
big of a delay. I switched to binary files instead which get read much faster and have
less overhead. However this means the binary file needs to match exactly, down to each single
byte. Therefore careful documentation below is needed to ensure it matches up.

## File dset.txt

The dset.txt file contains information about the dataset that was generated with suncalc.
It also has the location longitude / latitude information, the magnetic declination, and
the number of data files that were generated (not counting the csv files of equal number).

### Specs

File format: ASCII text file with colon-separated key-value record lines  
Record size: keys = 12 chars (including colon). Values = 14 chars max (OLED font max)  
Record count: 10 records  
File size: approx 210 Bytes  

### Example File Content

```
prgversion: 1.2
prgrundate: Mon 2019-08-12
start-date: 20190901
locationlg: 139.628999
locationla: 35.610381
locationtz: 9.000000
mag-declin: -7.583000
dayfiles-#: 30
daybinsize: 19 Bytes
srsbinsize: 14 Bytes
```

## File srs[yyyy].bin - Daily sunrise, transit and sunset data file

The srs[yyyy].bin file has a 14-Byte long record for each calculated day of the year, indicated by its file name.
Every row contains the data set for one day containing the sunrise time, the sunrise azimuth angle,
the transit time, the transit elevation angle, the sunset time and the sunset azimuth angle. The record
count depends on the dataset size argument given to suncalc, e.g. the argument '-p nm' generates 30 or 31
records for next month. If the argument was for a year, all days of the year will be in the file. For
multi-year calculations, one file will be created for each year.

### Specs

File format: Fixed record length binary file  
Record size: 14 Bytes  
Record count: 365 or 366 (matching the count of days within a year)  
Min File size: min 14 Bytes (1 day data generation)  
Max File size: 5124 Bytes (year with 366 days x 14 Bytes)  

### Record Description

| Byte Position | # of Bytes | Data Type | Name          | Description                      | Range |
| ------------- | ---------- | --------- | ------------- | -------------------------------- | ----- |
| 1             | 1          | uint8_t   | month         | The month of the year in decimal | 1..12 |
| 2             | 1          | uint8_t   | day           | The day of the month in decimal  | 1..31 |
| 3             | 1          | uint8_t   | risehour      | Sunrise hour for this day        | 0..23 |
| 4             | 1          | uint8_t   | risemin       | Sunrise minute for this day      | 0..59 |
| 5             | 2          | uint16_t  | riseazimuth   | Sun Azimuth angle at sunrise     | 0..359 |
| 7             | 1          | uint8_t   | transithour   | The suns max elevation hour      | 0..23 |
| 8             | 1          | uint8_t   | transit_min   | The suns max elevation minute    | 0..59 |
| 9             | 2          | int16_t   | transitelevation | The suns max elevation angle  | -90..90 |
| 11            | 1          | uint8_t   | sethour       | The Sunset hour of the day       | 0..23 |
| 12            | 1          | uint8_t   | setmin        | The Sunset minute of the day     | 0..59 |
| 13            | 2          | uint16_t  | setazimuth    | Sun Azimuth angle at sunset      | 0..359 |

### Example

In the file example below, the byte data is shown in decimal format. Note that byte 5-6, 9-10, and 13-14
 belong together. The data shows day 2019-07-28, sunrise is at 4:46, the sunrise azimuth angle is 66 degrees.

```
fm@Porthos:~/projects/git/suncalc$ od -N14 -A d -t dC tracker-data/srs-2019.bin
0000000    7   28    4   46   66    0   11   48   73    0   18   49   38    1
0000014
```

### Debug Notes

suncalc generates the CSV equivalent srs-[yyyy].csv for easy debug purpose.
The CSV has the same record count as the binary data file.

## File [yyyymmdd].bin - All sun position angles for one single day

The [yyyymmdd].bin file has 19-byte long records for the specific day specified in its file name.
The record contains the days hour, minute, and the suns azimuth and zenith angle at that point in time.
The record count depends on the time interval that was choosen for suncalc. By default, it generates
one record for each minute of the day.

### Specs

File format: Fixed record length binary file  
Record size: 19 Bytes  
Record count: depending on time interval set, default 1440 records (1440 minutes in one day)  
Max File size: 27360 Bytes (19 Bytes x 1440 records)  

### Record Description

| Byte Position | # of Bytes | Data Type | Name          | Description                      | Range |
| ------------- | ---------- | --------- | ------------- | -------------------------------- | ----- |
| 1             | 1          | uint8_t   | hour          | The hour of the day              | 0..23 |
| 2             | 1          | uint8_t   | minute        | The minute of the day            | 0..59 |
| 3             | 1          | uint8_t   | dflag         | 1=Daytime / 0=Nighttime flag     | 0 or 1 |
| 4             | 8          | double    | azimuth       | The Azimuth angle at that time   | 0..359 |
| 12            | 8          | double    | zenith        | The Zenith angle at that time    | 0..180 |

### Debug Notes

suncalc generates the CSV equivalent [yyyymmdd].csv for easy debug purpose.
The CSV has the same record count as the binary data file, e.g. by default 1440 lines for each minute
of the day.

### Notes on Data Precision

The srs-[yyyy].bin data is rounded to the nearest degree by suncalc, and the data is consumed as-is by
the Arduino MKR Zero. The [yyyymmdd].bin daily position data is kept as double with the original precision
 as calculated. It is currently rounded by the Ardino MKR Zero before displayed, or used to calculate
the 32 LED position value.

### Notes on Endianness

The file-creating Linux system and the target system Arduino MKR Zero are both "little-endian".
This means the byte-order is the same, following the "LSB 0" scheme. In "LSB 0", the bit
 numbering starts at zero for the least significant bit (LSB).

### Notes on C Structure Padding

Padding aligns C-structure members to "natural" address boundaries. If a structure has members
 of different size, the bigger (multi-byte) size members define the size for smaller members.
This means smaller members such as 1-Byte uint8_t or char take the size, filling the unused
 space with null bytes. Padding is on by default, its is purpose to maintain even numbered
 addresses(aligned) so that execution is faster.

Currently, all structures in suncalc are memory-aligned, and no padding occurs at all. Any
changes to the structures requires careful check for resulting record and file sizes to
identify padding, which may throw off the Arduino picking up the correct byte positions.
