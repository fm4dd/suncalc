/* ------------------------------------------------------------ *
 * file:        suncalc.c                                       *
 * purpose:     calculate data files for solar tracker usage.   *
 *                                                              *
 * return:      0 on success, and -1 on errors.                 *
 *                                                              *
 * requires:	solar positioning algorithm (SPA) headers       *
 *              and source files http://midcdmz.nrel.gov/spa    *
 *                                                              *
 * example:	./suncalc                                       *
 *                                                              *
 * author:      05/24/2019 Frank4DD                             *
 *                                                              *
 * This program calculates the sun position for given coords.   *
 * The results are written into a ./data-tracker folder:        *
 * dset.txt -> dataset information file                         *
 * yyyymmdd.csv -> sun position daily file in readable csv      *
 * yyyymmdd.bin -> sun position daily file(s) in binary format  *
 * srs-yyyy.csv -> sunrise/sunset yearly file in readable csv   *
 * srs-yyyy.bin -> sunrise/sunset yearly file in binary format  *
 *                                                              *
 * data volume:	i=60   (1min) day=112K, month=2MB, year=23MB    *
 * data volume:	i=600 (10min) day=24K, month=300K, year=3.0MB   *
 *                                                              *
 * Remember the program version needs to match the MCU code for *
 * successful extract of the file structures by the MCU program *
 * ------------------------------------------------------------ */
#include <stdlib.h>    // various, atoi, atof
#include <stdio.h>     // run display
#include <ctype.h>     // isprint
#include <dirent.h>    // opendir
#include <stdint.h>    // uint8_t data type
#include <sys/types.h> // folder creation
#include <sys/stat.h>  // folder creation
#include <unistd.h>    // folder creation
#include <string.h>    // data file output string
#include <getopt.h>    // arg handling
#include <time.h>      // time and date
#include <math.h>      // round()
#include "spa.h"       // SPA functions

/* ------------------------------------------------------------ *
 * global variables and defaults                                *
 * ------------------------------------------------------------ */
#define DELTA_UT1	0
#define DELTA_T		67
#define ELEVATION	1000
#define PRESSURE	1000
#define TEMPERATURE	19
#define SLOPE		0
#define AZM_ROTATION	0
#define ATM_REFRACT	0.5667

/* ------------------------------------------------------------ *
 * Tokyo Magnetic Declination: -7° 35'                         *
 * Declination is NEGATIVE (WEST), e.g. 0 + (-7.583) = 352.417  *
 * Magnetic Bearing MB + Magnetic Declination MD = True Bearing *
 * Inclination: 49° 38', Magnetic field strength: 46698.5 nT   *
 * value from http://www.magnetic-declination.com/              *
 * ------------------------------------------------------------ */

int verbose = 0;
char progver[] = "1.2";              // suncalc program version
char period[3] = "nd";               // default calculation period
char rundate[20] = "";               // program run date
char outdir[256] = "./tracker-data"; // default output folder
char dsetfile[] = "dset.txt";        // dataset parameter information file
char daycfile[20] = "";              // daily csv dataset file <yyyymmdd.csv>
char daybfile[20] = "";              // daily bin dataset file <yyyymmdd.bin>
char srsbfile[20] = "";              // yearly sunrise/sunset bin file <srs-yyyy.bin>
char srscfile[20] = "";              // yearly sunrise/sunset csv file <srs-yyyy.csv>
double longitude = 139.628999;       // long default if not set by cmdline
double latitude = 35.610381;         // lat default if not set by cmdline
double mdeclination = -7.583;        // mag declination default for lat/long above
double tz = +9;                      // timezone default if not set by cmdline
int interval = 60;                   // interval default if not set by cmdline

/* ------------------------------------------------------------ *
 * brecord structure contains the sun angles per time interval  *
 * and is stored in the daily bin file. record size: 19 bytes   *
 * ------------------------------------------------------------ */
struct brecord {
   uint8_t hour;                     // 0-23 day hour
   uint8_t minute;                   // 0-59 day minute
   uint8_t dflag;                    // 0 or 1 daylight or night flag
   uint8_t azimuth[sizeof(double)];  // byte array for double (8 bytes)
   uint8_t zenith[sizeof(double)];   // byte array for double (8 bytes)
};

/* ------------------------------------------------------------ *
 * drecord structure contains the daily sun min/max values      *
 * and is stored in the yearly srs file. record size: 14 bytes  *
 * transit elevation fits into int8_t, I use int16_t to avoid   *
 * compiler padding to memory-align the structure.              *
 * ------------------------------------------------------------ */
struct drecord {
   uint8_t month;                    // 1-12 month of the year
   uint8_t day;                      // 1-31 day of the year
   uint8_t risehour;                 // 0-23 sunrise hour
   uint8_t riseminute;               // 0-59 sunrise minute
   uint16_t riseazimuth;             // 0-359 (round to full degree to reduce datatype storage)
   uint8_t transithour;              // 0-23 zenith peak hour
   uint8_t transitminute;            // 0-59 zenit peak minute
   int16_t transitelevation;         // -90..90 (max sun height, rounded to full degree)
   uint8_t sethour;                  // 0-23 sunset hour
   uint8_t setminute;                // 0-59 sunset minute
   uint16_t setazimuth;              // 0-359 (see above)
};

/* ------------------------------------------------------------ *
 * print_usage() prints the programs commandline instructions.  *
 * ------------------------------------------------------------ */
void usage() {
   static char const usage[] = "Usage: ./suncalc [-x <longitude>] [-y <latitude>] [-t <timezone>] [-i <interval>] [-p period nd|nm|nq|ny|td|tm|tq|ty] [-o outfolder] [-v]\n\
\n\
Command line parameters have the following format:\n\
   -x   location longitude, Example: -x 139.628999 (default)\n\
   -y   location latitude, Example: -y 35.610381 (default)\n\
   -t   location timezone offset in hours, Example: -t +9 (default)\n\
   -i   calculation interval in seconds between 60 and 3600, Example -i 60 (default)\n\
        the value must be a multiple of 86400 (1 day): e.g. 300, 600, 900, 1200, 1800\n\
   -p   calculation period:\n\
           nd = next day (tomorrow, default)\n\
           nm = next month (2M)\n\
           nq = next quarter\n\
           ny = next year (starting Jan-1 until Dec-31, 23M)\n\
           td = this day (today, 112K)\n\
           tm = this month (starting today, 2M)\n\
           tq = this quarter (starting today)\n\
           ty = this year (starting Jan-1 until Dec-31, 23M)\n\
           2y = two years (starting this year, 46M)\n\
           tf = ten years forward (starting this year, 230M)\n\
   -o   output folder, Example: -o ./tracker-data (default)\n\
   -h   display this message\n\
   -v   enable debug output\n\
\n\
Usage examples:\n\
./suncalc -x 139.628999 -y 35.610381 -t +9 -i 600 -p nd -o ./tracker-data -v\n\n\
zip -r tracker-data.zip tracker-data\n";
   printf("suncalc v%s\n\n", progver);
   printf(usage);
}

/* ------------------------------------------------------------ *
 * handle_spa_errors() turn spa error code into readabe strings *
 * ------------------------------------------------------------ */
void handle_spa_errors(spa_data spa, int errcode) {
   if(errcode == 1) printf("Dataset year error, value %d - valid range -2000 to 6000.\n", spa.year);
   if(errcode == 2) printf("Dataset month error, value %d - valid range: 1 to  12.\n", spa.month);
   if(errcode == 3) printf("Dataset day error, value %d - valid range: 1 to  31.\n", spa.day);
   if(errcode == 4) printf("Dataset hour error, value %d - valid range: 0 to  24.\n", spa.hour);
   if(errcode == 5) printf("Dataset minute error, value %d - valid range: 0 to  59.\n", spa.minute);
   if(errcode == 6) printf("Dataset second error, value %e - valid range: 0 to  <60.\n", spa.second);
}

/* ------------------------------------------------------------ *
 * debug_spa_input() displays the spa input for troubleshooting *
 * ------------------------------------------------------------ */
void debug_spa_input(spa_data spa) {
   printf("spa.year:          %d\n", spa.year);
   printf("spa.month:         %d\n", spa.month);
   printf("spa.day:           %d\n", spa.day);
   printf("spa.hour:          %d\n", spa.hour);
   printf("spa.minute:        %d\n", spa.minute);
   printf("spa.second:        %f\n", spa.second);
   printf("spa.timezone:      %f\n", spa.timezone);
   printf("spa.delta_ut1:     %f\n", spa.delta_ut1);
   printf("spa.delta_t:       %f\n", spa.delta_t);
   printf("spa.longitude:     %f\n", spa.longitude);
   printf("spa.latitude:      %f\n", spa.latitude);
   printf("spa.elevation:     %f\n", spa.elevation);
   printf("spa.pressure:      %f\n", spa.pressure);
   printf("spa.temperature:   %f\n", spa.temperature);
   printf("spa.slope:         %f\n", spa.slope);
   printf("spa.azm_rotation:  %f\n", spa.azm_rotation);
   printf("spa.atmos_refract: %f\n", spa.atmos_refract);
}

/* ------------------------------------------------------------ *
 * remove_data() delete old dataset if same folder gets reused  *
 * ------------------------------------------------------------ */
void remove_data(const char *path) {
   DIR *d = opendir(path);
   size_t path_len = strlen(path);
   int r = -1;

   if(d) {
      struct dirent *p;
      r = 0;

      while (!r && (p=readdir(d))) {
          int r2 = -1;
          char *buf;
          size_t len;

          /* ------------------------------------------------------------ *
           * Skip "." and ".." folders                                    * 
           * ------------------------------------------------------------ */
          if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) {
             continue;
          }

          len = path_len + strlen(p->d_name) + 2; 
          buf = malloc(len);

          if (buf) {
             struct stat statbuf;
             snprintf(buf, len, "%s/%s", path, p->d_name);
             if (!stat(buf, &statbuf)) {
                if(verbose == 1) printf("Debug: delete old dataset file %s\n", buf);
                r2 = unlink(buf);
             }
             free(buf);
          }
          r = r2;
      }
      closedir(d);
   }
}
/* ------------------------------------------------------------ *
 * write_dsetfile() create the dataset description file         *
 * ------------------------------------------------------------ */
void write_dsetfile(spa_data spa, int num) {
   FILE *dset;
   char fpath[1024];
   snprintf(fpath, sizeof(fpath), "%s/%s", outdir, dsetfile);
   if(! (dset=fopen(fpath, "w"))) {
      printf("Error open %s for writing.\n", fpath);
      exit(-1);
   }
   else printf("Create dataset file [%s]\n", fpath);

   fprintf(dset, "prgversion: %s\n", progver);
   fprintf(dset, "prgrundate: %s\n", rundate);
   fprintf(dset, "start-date: %d%02d%02d\n", spa.year, spa.month, spa.day);
   fprintf(dset, "locationlg: %f\n", spa.longitude);
   fprintf(dset, "locationla: %f\n", spa.latitude);
   fprintf(dset, "locationtz: %f\n", spa.timezone);
   fprintf(dset, "mag-declin: %f\n", mdeclination);
   fprintf(dset, "dayfiles-#: %d\n", num);
   fprintf(dset, "daybinsize: %ld Bytes\n", sizeof(struct brecord));
   fprintf(dset, "srsbinsize: %ld Bytes\n", sizeof(struct drecord));
   fclose(dset);
}

/* ----------------------------------------------------------- *
 * srsazimut() calculates the azimuth values at sunrise sunset *
 * ----------------------------------------------------------- */
uint16_t srsazimuth(spa_data spa, struct tm srs_tm) {
   int result = 0;
   uint16_t azimuth = 0;
   /* -------------------------------------------------------- *
    * copy the original spa structure to local copy called srs *
    * -------------------------------------------------------- */
   spa_data srs = spa;
   /* -------------------------------------------------------- *
    * set the sunrise/sunset time for spa calculation          *
    * -------------------------------------------------------- */
   srs.hour   = srs_tm.tm_hour;
   srs.minute = srs_tm.tm_min;
   srs.second = srs_tm.tm_sec;
  /* -------------------------------------------------------- *
   * call the calculation function and pass the SPA structure *
   * -------------------------------------------------------- */
   result = spa_calculate(&srs);
   if(result > 0) handle_spa_errors(srs, result);
  /* -------------------------------------------------------- *
   * round the double value of azimuth to full degrees        *
   * -------------------------------------------------------- */
   azimuth = (uint16_t) round(srs.azimuth);
   return azimuth;
}

/* ----------------------------------------------------------- *
 * transelevation() calculates the day's max elevation angle   *
 * ----------------------------------------------------------- */
int16_t transelevation(spa_data spa, struct tm transit_tm) {
   int result = 0;
   int16_t zenith = 0;
   int16_t elevation = 0;
   /* -------------------------------------------------------- *
    * copy the original spa structure to local copy transit    *
    * -------------------------------------------------------- */
   spa_data transit = spa;

   /* -------------------------------------------------------- *
    * set the sunrise/sunset time for spa calculation          *
    * -------------------------------------------------------- */
   transit.hour   = transit_tm.tm_hour;
   transit.minute = transit_tm.tm_min;
   transit.second = transit_tm.tm_sec;
  /* -------------------------------------------------------- *
   * call the calculation function and pass the SPA structure *
   * -------------------------------------------------------- */
   result = spa_calculate(&transit);
   if(result > 0) handle_spa_errors(transit, result);
  /* -------------------------------------------------------- *
   * round the double value of azimuth to full degrees        *
   * -------------------------------------------------------- */
   zenith = (int16_t) round(transit.zenith);
  /* -------------------------------------------------------- *
   * Because the spa algorithm returns the zenith distance we *
   * convert it into elevation. elevation + z-distance = 90   *
   * For nighttime, the elevation becomes negative, but here  *
   * we only use it for transit time peak value (solar noon). *
   * -------------------------------------------------------- */
   elevation = 90 - zenith;
   return elevation;
}

/* ----------------------------------------------------------- *
 * parseargs() checks the commandline arguments with C getopt  *
 * ----------------------------------------------------------- */
void parseargs(int argc, char* argv[]) {
   int arg;
   opterr = 0;

   if(argc == 1 || (argc == 2 && strcmp(argv[1], "-v") == 0)) {
       printf("No arguments, creating dataset with program defaults.\n");
       printf("See ./suncalc -h for further usage.\n");
   }

   while ((arg = (int) getopt (argc, argv, "x:y:t:i:p:o:hv")) != -1) {
      switch (arg) {
         // arg -v verbose, type: flag, optional
         case 'v':
            verbose = 1; break;

         // arg -x longitude type: double
         case 'x':
            if(verbose == 1) printf("Debug: arg -x, value %s\n", optarg);
            longitude = strtof(optarg, NULL);
            if (longitude  == 0.0 ) {  
               printf("Error: Cannot get valid longitude.\n");
               exit(-1);
            }
            break;

         // arg -y latitude type: double
         case 'y':
            if(verbose == 1) printf("Debug: arg -y, value %s\n", optarg);
            latitude = strtof(optarg, NULL);
            if (latitude  == 0.0 ) {  
               printf("Error: Cannot get valid latitude.\n");
               exit(-1);
            }
            break;

         // arg -t timezone offset type: double
         case 't':
            if(verbose == 1) printf("Debug: arg -t, value %s\n", optarg);
            tz = strtof(optarg, NULL);
            if(tz < -11  || tz > +11 ) {  
               printf("Error: Cannot get valid timezone offset.\n");
               exit(-1);
            }
            break;

         // arg -i interval type: int
         case 'i':
            if(verbose == 1) printf("Debug: arg -i, value %s\n", optarg);
            interval = atoi(optarg);
            if(interval < 60  || interval > 3600) {  
               printf("Error: Cannot get valid interval.\n");
               exit(-1);
            }
            if(86400 % interval != 0){
                printf("Error: interval is not a  multiple of 86400 (1 day)\n");
                exit(-1);
            }
            break;

         // arg -p sets the output period, type: string
         case 'p':
            if(verbose == 1) printf("Debug: arg -p, value %s\n", optarg);
            if(strlen(optarg) == 2) {
              strncpy(period, optarg, sizeof(period));
            }
            else {
               printf ("Error: period [%s] option length [%d].\n", optarg, (int) strlen(optarg));
               usage();
               exit(-1);
            }
            break;

         // arg -o output directory
         // writes the data files in that folder. example: ./tracker-data
         case 'o':
            if(verbose == 1) printf("Debug: arg -o, value %s\n", optarg);
            strncpy(outdir, optarg, sizeof(outdir));
            break;

         // arg -h usage, type: flag, optional
         case 'h':
            usage(); exit(0);
            break;

         case '?':
            if(isprint (optopt))
               printf ("Error: Unknown option `-%c'.\n", optopt);
            else {
               printf ("Error: Unknown option character `\\x%x'.\n", optopt);
               usage();
               exit(-1);
            }
            break;

         default:
            usage();
            break;
      }
   }
}

/* ------------------------------------------------------------ *
 * main() function to execute the program                       *
 * ------------------------------------------------------------ */
int main(int argc, char *argv[]) {
   int result;

   /* ---------------------------------------------------------- *
    * process the cmdline parameters                             *
    * ---------------------------------------------------------- */
   parseargs(argc, argv);

   /* ---------------------------------------------------------- *
    * get current time (now), write program start if verbose     *
    * ---------------------------------------------------------- */
   time_t tsnow, tstart, tend, tcalc, trise = 0, tset = 0;
   tsnow = time(NULL);
   struct tm *now, start_tm, end_tm, calc_tm, rise_tm, transit_tm, set_tm;
   now = localtime(&tsnow);
   strftime(rundate, sizeof(rundate), "%a %Y-%m-%d", now);
   if(verbose == 1) printf("Debug: ts [%lld][%s]\n", (long long) tsnow, rundate);

   /* ----------------------------------------------------------- *
    * always run over a full day: start 00:00:00 end 00:00:00 +1d *
    * ----------------------------------------------------------- */
   start_tm = *now;      // initialize start date with current date
   end_tm = *now;        // initialize end date with current date
   start_tm.tm_hour = 0;
   start_tm.tm_min  = 0;
   start_tm.tm_sec  = 0;
   end_tm.tm_hour = 0;
   end_tm.tm_min  = 0;
   end_tm.tm_sec  = 0;

   /* ----------------------------------------------------------- *
    * "-p" set the dataset period to calculate                    *
    * ----------------------------------------------------------- */
   if(strlen(period) > 0) {
      if(strcmp(period, "nd") == 0) {      // tomorrow
         start_tm.tm_mday += 1;
         end_tm.tm_mday += 2;
      }
      else if(strcmp(period, "nm") == 0) { // next month
         start_tm.tm_mon += 1;             // count 1 month forward
         start_tm.tm_mday = 1;             // set start day to 1st
         end_tm.tm_mon += 2;               // count 2 months forward
         end_tm.tm_mday = 1;               // set end day to 1st
      }
      else if(strcmp(period, "nq") == 0) { // next quarter
      } 
      else if(strcmp(period, "ny") == 0) { // next year
         start_tm.tm_year += 1;            // count year forward
         start_tm.tm_mon   = 0;            // set month to Jan
         start_tm.tm_mday  = 1;            // set day to 1st
         end_tm.tm_year += 2;              // count 2 years forward
         end_tm.tm_mon   = 0;              // set month to Jan
         end_tm.tm_mday  = 1;              // set day to 1st
      }
      else if(strcmp(period, "td") == 0) { // this month
         end_tm.tm_mday += 1;
      }
      else if(strcmp(period, "tm") == 0) { // this month
         start_tm.tm_mday = 1;             // set start day to 1st
         end_tm.tm_mon += 1;               // count 1 month forward
         end_tm.tm_mday = 1;               // set end day to 1st
      }
      else if(strcmp(period, "tq") == 0) { // this quarter
      } 
      else if(strcmp(period, "ty") == 0) { // this year
         start_tm.tm_mon   = 0;            // set month to Jan
         start_tm.tm_mday  = 1;            // set start day to 1st
         end_tm.tm_year += 1;              // count 1 year forward
         end_tm.tm_mon   = 0;              // set month to Jan
         end_tm.tm_mday  = 1;              // set day to 1st
      }
      else if(strcmp(period, "2y") == 0) { // this year + next year
         start_tm.tm_mon   = 0;            // set month to Jan
         start_tm.tm_mday  = 1;            // set start day to 1st
         end_tm.tm_year += 2;              // count 2 years forward
         end_tm.tm_mon   = 0;              // set month to Jan
         end_tm.tm_mday  = 1;              // set day to 1st
      }
      else if(strcmp(period, "tf") == 0) { // this year + next year
         start_tm.tm_mon   = 0;            // set month to Jan
         start_tm.tm_mday  = 1;            // set start day to 1st
         end_tm.tm_year += 10;             // count 10 years forward
         end_tm.tm_mon   = 0;              // set month to Jan
         end_tm.tm_mday  = 1;              // set day to 1st
      }
      else {
         printf("Error: invalid dataset period %s.\n", period);
         exit(-1);
      }
   }
   tstart = mktime(&start_tm);
   tend = mktime(&end_tm);
   if(verbose == 1) printf("Debug: Data set start [%d-%02d-%02d %02d:%02d:%02d]\n",
                            start_tm.tm_year + 1900, start_tm.tm_mon + 1, start_tm.tm_mday,
                            start_tm.tm_hour, start_tm.tm_min, start_tm.tm_sec);
   if(verbose == 1) printf("Debug: Data set end   [%d-%02d-%02d %02d:%02d:%02d]\n",
                            end_tm.tm_year + 1900, end_tm.tm_mon + 1, end_tm.tm_mday,
                            end_tm.tm_hour, end_tm.tm_min, end_tm.tm_sec);

   /* -------------------------------------------------------- *
    * open target folder, create if it does not exist          *
    * -------------------------------------------------------- */
   struct stat st = {0};
   if (stat(outdir, &st) == -1) {
      mkdir(outdir, 0700);
      printf("Created new output folder [%s]\n", outdir);
   }
   else {
      if(verbose == 1) printf("Debug: Found output folder [%s], overwriting data.\n", outdir);
      remove_data(outdir);
   }

   /* -------------------------------------------------------- *
    * configure the calculation values                         *
    * some hardcoded values: elevation, pressure, temperature, *
    * slope, atmospheric refraction, delta_t                   *
    * -------------------------------------------------------- */
   spa_data spa;  //declare the SPA structure
   spa.year          = (int) start_tm.tm_year+1900;
   spa.month         = start_tm.tm_mon+1;
   spa.day           = start_tm.tm_mday;
   spa.hour          = 0;
   spa.minute        = 0;
   spa.second        = 0;
   spa.timezone      = tz;  // - for trailing GMT, e.g. US, + for ahead of GMT, e.g. Japan
   spa.delta_ut1     = DELTA_UT1;
   spa.delta_t       = DELTA_T;
   spa.longitude     = longitude;
   spa.latitude      = latitude;
   spa.elevation     = ELEVATION;
   spa.pressure      = PRESSURE;
   spa.temperature   = TEMPERATURE;
   spa.slope         = SLOPE;
   spa.azm_rotation  = AZM_ROTATION;
   spa.atmos_refract = ATM_REFRACT;
   spa.function      = SPA_ALL;
   //if(verbose == 1) debug_spa_input(spa);

   /* -------------------------------------------------------- *
    * calculate datefile count and write dataset info file     *
    * -------------------------------------------------------- */
   tcalc = tstart;
   int days = difftime(tend,tstart) / 86400;
   int rows = 86400 / interval;
   if(verbose == 1) printf("Debug: data days/rows [%d/%d]\n", days, rows);
   write_dsetfile(spa, days);

   /* -------------------------------------------------------- *
    * cycle through the calculation period                     *
    * -------------------------------------------------------- */
   FILE *fdayc = NULL;
   FILE *fdayb = NULL;
   FILE *fsrsb = NULL;
   FILE *fsrsc = NULL;
   char fpath[1024];
   int dayflag = 0;

   while(tcalc < tend) {
      /* -------------------------------------------------------- *
       * assign the date and time, calculate the solar position   *
       * -------------------------------------------------------- */
      calc_tm    = *localtime(&tcalc);
      spa.year   = (int) calc_tm.tm_year+1900;
      spa.month  = calc_tm.tm_mon+1;
      spa.day    = calc_tm.tm_mday;
      spa.hour   = calc_tm.tm_hour;
      spa.minute = calc_tm.tm_min;
      spa.second = calc_tm.tm_sec;
      /* -------------------------------------------------------- *
       * call the calculation function and pass the SPA structure *
       * -------------------------------------------------------- */
      result = spa_calculate(&spa);
      if(result > 0) handle_spa_errors(spa, result);
      /* -------------------------------------------------------- *
       * check if we got a new day to process                     *
       * -------------------------------------------------------- */
      if( calc_tm.tm_hour == 0 && calc_tm.tm_min == 0){
         /* -------------------------------------------------------- *
          * if previous data file are still open, close them first   *
          * -------------------------------------------------------- */
         if(fdayc) fclose(fdayc);
         if(fdayb) fclose(fdayb);
         if(fsrsb) fclose(fsrsb);
         /* -------------------------------------------------------- *
          * assign the days sunrise, suntransit and sunset time      *
          * -------------------------------------------------------- */
         float min, sec;
         min = 60.0*(spa.sunrise - (int)(spa.sunrise));
         sec = 60.0*(min - (int)min);
         rise_tm = calc_tm;
         rise_tm.tm_hour = (int)(spa.sunrise);
         rise_tm.tm_min = (int)min;
         rise_tm.tm_sec = (int)sec;

         min = 60.0*(spa.suntransit - (int)(spa.suntransit));
         sec = 60.0*(min - (int)min);
         transit_tm = calc_tm;
         transit_tm.tm_hour = (int)(spa.suntransit);
         transit_tm.tm_min = (int)min;
         transit_tm.tm_sec = (int)sec;

         min = 60.0*(spa.sunset - (int)(spa.sunset));
         sec = 60.0*(min - (int)min);
         set_tm = calc_tm;
         set_tm.tm_hour = (int)(spa.sunset);
         set_tm.tm_min = (int)min;
         set_tm.tm_sec = (int)sec;
         if(verbose == 1) printf("Debug: sunrise sunset [%02d:%02d:%02d] [%02d:%02d:%02d]\n",
                                  rise_tm.tm_hour, rise_tm.tm_min, rise_tm.tm_sec,
                                  set_tm.tm_hour, set_tm.tm_min, set_tm.tm_sec);
         trise = mktime(&rise_tm);
         tset = mktime(&set_tm);

         /* -------------------------------------------------------- *
          * Do we have a yearly sunrise/sunset file srs-yyyy.bin ?   *
          * -------------------------------------------------------- */
         snprintf(srsbfile, sizeof(srsbfile), "srs-%04d.bin", calc_tm.tm_year + 1900);
         if(verbose == 1) printf("Debug: srsb file name  [%s]\n", srsbfile);
         snprintf(fpath, sizeof(fpath), "%s/%s", outdir, srsbfile);

         struct stat st = {0};
         if (stat(fpath, &st) == -1) {      // if we dont have the file, create
            if(! (fsrsb=fopen(fpath, "w"))) {
               printf("Error open %s for writing\n", fpath);
               exit(-1);
            } 
           printf("Create srs bin file [%s]\n", fpath);
         }
         else {                             // if we have the file, append to it
            if(! (fsrsb=fopen(fpath, "a"))) {
               printf("Error open %s for appending\n", fpath);
               exit(-1);
            } 
           printf("Update srs bin file [%s]\n", fpath);
         }
         /* -------------------------------------------------------- *
          * Do we have a yearly sunrise/sunset file srs-yyyy.csv ?   *
          * -------------------------------------------------------- */
         snprintf(srscfile, sizeof(srscfile), "srs-%04d.csv", calc_tm.tm_year + 1900);
         if(verbose == 1) printf("Debug: srsc file name  [%s]\n", srscfile);
         snprintf(fpath, sizeof(fpath), "%s/%s", outdir, srscfile);

         if (stat(fpath, &st) == -1) {      // if we dont have the file, create
            if(! (fsrsc=fopen(fpath, "w"))) {
               printf("Error open %s for writing\n", fpath);
               exit(-1);
            } 
           printf("Create srs csv file [%s]\n", fpath);
         }
         else {                             // if we have the file, append to it
            if(! (fsrsc=fopen(fpath, "a"))) {
               printf("Error open %s for appending\n", fpath);
               exit(-1);
            } 
           printf("Update srs csv file [%s]\n", fpath);
         }
         /* -------------------------------------------------------- *
          * Get sunrise and sunset azimuth values for the new day    *
          * -------------------------------------------------------- */
         uint16_t razi = 0;
         uint16_t sazi = 0;
         razi = srsazimuth(spa, rise_tm);
         sazi = srsazimuth(spa, set_tm);
         if(verbose == 1) printf("Debug: sunrise/sunset [%d - %d] azimuth range [%d] \n", razi, sazi, sazi-razi);

         /* -------------------------------------------------------- *
          * Get zenith max elevation angle at sun transit (noon)time *
          * -------------------------------------------------------- */
         int16_t tele = 0;
         tele = transelevation(spa, transit_tm);
         if(verbose == 1) printf("Debug: suntransit at [%d:%d] elevation [%d] \n",
                                  transit_tm.tm_hour, transit_tm.tm_min, tele);

         /* -------------------------------------------------------- *
          * create sunrise/sunset file binary data output structure  *
          * -------------------------------------------------------- */
         struct drecord srs;
         srs.month            = calc_tm.tm_mon+1;
         srs.day              = calc_tm.tm_mday;
         srs.risehour         = rise_tm.tm_hour;
         srs.riseminute       = rise_tm.tm_min;
         srs.riseazimuth      = razi;
         srs.transithour      = transit_tm.tm_hour;
         srs.transitminute    = transit_tm.tm_min;
         srs.transitelevation = tele;
         srs.sethour          = set_tm.tm_hour;
         srs.setminute        = set_tm.tm_min;
         srs.setazimuth       = sazi;

         /* -------------------------------------------------------- *
          * add record to the sunrise/sunset csv file srsyyyy.csv    *
          * -------------------------------------------------------- */
         fprintf(fsrsc, "%04d-%02d-%02d,%02d:%02d,%d,%02d:%02d,%d,%02d:%02d,%d\n",
              calc_tm.tm_year + 1900, srs.month, srs.day, 
              srs.risehour, srs.riseminute, srs.riseazimuth,
              srs.transithour, srs.transitminute, srs.transitelevation,
              srs.sethour, srs.setminute, srs.setazimuth);
         fflush(fsrsc);

         /* -------------------------------------------------------- *
          * add record to the sunrise/sunset binary file srsyyyy.bin *
          * -------------------------------------------------------- */
         fwrite(&srs, sizeof(srs), 1, fsrsb);
         fflush(fsrsb);

         /* -------------------------------------------------------- *
          * create day csv file yyyymmdd.csv under the outdir folder *
          * -------------------------------------------------------- */
         snprintf(daycfile, sizeof(daycfile), "%04d%02d%02d.csv",
                  calc_tm.tm_year + 1900, calc_tm.tm_mon + 1, calc_tm.tm_mday);
         if(verbose == 1) printf("Debug: csv file name [%s]\n", daycfile);
         snprintf(fpath, sizeof(fpath), "%s/%s", outdir, daycfile);
         /* -------------------------------------------------------- *
          * open a new csv data file for writing                     *
          * -------------------------------------------------------- */
         if(! (fdayc=fopen(fpath, "w"))) {
            printf("Error open %s for writing\n", fpath);
            exit(-1);
         } else printf("Create day csv file [%s]\n", fpath);
 
         /* -------------------------------------------------------- *
          * create the bin file yyyymmdd.bin under the outdir folder *
          * -------------------------------------------------------- */
         snprintf(daybfile, sizeof(daybfile), "%04d%02d%02d.bin",
                  calc_tm.tm_year + 1900, calc_tm.tm_mon + 1, calc_tm.tm_mday);
         if(verbose == 1) printf("Debug: bin file name [%s]\n", daybfile);
         snprintf(fpath, sizeof(fpath), "%s/%s", outdir, daybfile);
         /* -------------------------------------------------------- *
          * open a new bin data file for writing                     *
          * -------------------------------------------------------- */
         if(! (fdayb=fopen(fpath, "w"))) {
            printf("Error open %s for writing\n", fpath);
            exit(-1);
         } else printf("Create day bin file [%s]\n", fpath);
      } // end of new day processing
      
      /* -------------------------------------------------------- *
       * Create dayflag, needs to occur after sunrise/sunset calc *
       * -------------------------------------------------------- */
      if(tcalc >= trise && tcalc <= tset) dayflag = 1;
      else dayflag = 0;
      /* -------------------------------------------------------- *
       * debug calculation output                                 *
       * -------------------------------------------------------- */
      if(verbose == 1) printf("Debug: calc data set [%04d-%02d-%02d %02d:%02d:%02d] Z[%07.3f] A[%07.3f] DF[%d]\n",
                            calc_tm.tm_year + 1900, calc_tm.tm_mon + 1, calc_tm.tm_mday, calc_tm.tm_hour,
                            calc_tm.tm_min, calc_tm.tm_sec, spa.zenith, spa.azimuth, dayflag);
      /* -------------------------------------------------------- *
       * write the result to the data file using this csv format: *
       * time hh:mm, dayflag night=0, azimuth angle, zenith angle *
       * -------------------------------------------------------- */
      fprintf(fdayc, "%02d:%02d,%d,%.3f,%.3f\n",
              calc_tm.tm_hour, calc_tm.tm_min, dayflag, spa.azimuth, spa.zenith);

      /* -------------------------------------------------------- *
       * create binary file data output                           *
       * -------------------------------------------------------- */
      struct brecord frec;
      frec.hour     = calc_tm.tm_hour;
      frec.minute   = calc_tm.tm_min;
      frec.dflag    = dayflag;
      memcpy(frec.azimuth,&spa.azimuth,sizeof(spa.azimuth));
      memcpy(frec.zenith,&spa.zenith,sizeof(spa.zenith));

      /* -------------------------------------------------------- *
       * debug binary file data output                            *
       * -------------------------------------------------------- */
      double azi;
      memcpy(&azi, frec.azimuth, sizeof(double));
      double zen;
      memcpy(&zen, frec.zenith, sizeof(double));

      if(verbose == 1) printf("Debug: bin data set  [%02d] [%02d] [%d] [0x%.2x 0x%.2x 0x%.2x 0x%.2x] [%07.3f] [0x%.2x 0x%.2x 0x%.2x 0x%.2x] [%07.3f]\n",
                            frec.hour, frec.minute, frec.dflag,
                            frec.azimuth[0], frec.azimuth[1], frec.azimuth[2], frec.azimuth[3],
                            azi,
                            frec.zenith[0], frec.zenith[1], frec.zenith[2], frec.zenith[3],
                            zen);

      /* -------------------------------------------------------- *
       * write the byte array struct to the bin file w/o newline  *
       * -------------------------------------------------------- */
      fwrite(&frec, sizeof(frec), 1, fdayb);

      /* -------------------------------------------------------- *
       * calculate next time interval                             *
       * -------------------------------------------------------- */
      tcalc=tcalc+interval;
   }
   /* -------------------------------------------------------- *
    * close the last fdayc and binary files                    *
    * -------------------------------------------------------- */
   if(fdayc) fclose(fdayc);
   if(fdayb) fclose(fdayb);
   if(fsrsb) fclose(fsrsb);
   return 0;
}
