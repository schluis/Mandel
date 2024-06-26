#include "libBMP.h"
#include "helperFunctions.h"
#include "readfile.h"

#include <stdio.h>
#include <windows.h>
#include <utilapiset.h>
#include <math.h>
#include <complex.h>
#include <time.h>
#include <stdlib.h>
#include <inttypes.h>

#define THREADS 48

void image_coordinates_to_math_coordinates(long *bX, long *bY, double *mX, double *mY);
void math_coordinates_to_image_cooridnates(double *bX, double *bY, double *mX, double *mY);
long to_pos(long x, long y);

long recursion(double _Complex c, double _Complex z, long tiefe);
void calculate_set(uint32_t *data, long thread_nr, long threads);
void draw_color(uint32_t *data, long tiefe, double *bX, double *bY, double *mX, double *mY);
void calculate_image_position(double zoom);
uint32_t combine_color(uint32_t r, uint32_t g, uint32_t b);

DWORD WINAPI calculate_segment(LPVOID lpParam);

long thread_table[THREADS] = {0};
double x_max, x_min, y_max, y_min;
clock_t start, end;
double cpu_time_used;

double width = 0;
double height = 0;
double middle_x = 0;
double middle_y = 0;
int zoom_base = 0;
int zoom_speed = 0;
int max_iterations = 0;
double x_max_base = 0;
double x_min_base = 0;
double y_max_base = 0;
double y_min_base = 0;
int save_to_image = 0;
int zoom_start = 0;
int zoom_end = 0;

int main(void) {
    char *config= readFile("default.config"); // load configuration from default.config
	width = floor(find_double_parameter(config, "width"));
	height = floor(width * 5/7);
	middle_x = find_double_parameter(config, "middle_x");
	middle_y = find_double_parameter(config, "middle_y");
	zoom_base = find_int_parameter(config, "zoom");
	zoom_speed = find_int_parameter(config, "zoom_speed");
	max_iterations = find_int_parameter(config, "max_iterations");
	x_max_base = find_double_parameter(config, "x_max");
	x_min_base = find_double_parameter(config, "x_min");
	y_max_base = find_double_parameter(config, "y_max");
	y_min_base = find_double_parameter(config, "y_min");
	save_to_image = find_int_parameter(config, "save_to_image");
	zoom_start = find_int_parameter(config, "start");
	zoom_end = find_int_parameter(config, "end");


	if (width * height * 4 + 54 > 4000000000) {
		printf("\nImage size too big!\n\n");
		Beep(880,200);
		exit(-1);
	}
	
	start = clock();
	Beep(540,200);
	srand(time(NULL));

	for (int step = zoom_start; step <= zoom_end; step++) {
		memset(thread_table, 0, sizeof(thread_table));

		uint32_t *data = (uint32_t*) malloc(sizeof(uint32_t) * width * height); 	// Bilddaten
		
		calculate_image_position(zoom_base + pow(zoom_speed, step));
		
		printf("\nCalculating graph\n");
		
		for (long i = 0; i < THREADS; i++) {
			CreateThread(NULL, 0, calculate_segment, data, 0, NULL);
		}
		
		int not_finished = THREADS;
		int old_finished = 0;
		while(not_finished) {
			not_finished = THREADS;
			
			for (int i = 0; i < THREADS; i++) {
				if(*(thread_table + i) == 2) {
					not_finished--;
				}
			}
			if(not_finished != old_finished) {
				printf("Threads finished: %d%%\n", (THREADS-not_finished)* 100/THREADS);
				old_finished = not_finished;
			}
		}	
		
		char time_str[60];
		char identifier[80];
		char filename[100];
		time_t now = time(NULL);
		struct tm *t = localtime(&now);

		strftime(time_str, sizeof(time_str)-1, "%d%m%Y%H%M", t);
		sprintf(identifier, "mandel_%d_%s", step, time_str);

		if (save_to_image) {
			printf("\nPainting graph\n");
			sprintf(filename, "images/%s.bmp", identifier);
			bmp_create(filename, data,  width, height);
		} else {
			printf("\nSaving data\n");
			FILE *fp;
			sprintf(filename, "data/%s.dat", identifier);
   			fp = fopen(filename, "w");
			for (int i = 0; i < width * height; i++) {
   				fprintf(fp, "%"PRIu32"\n", data[i]);
			}
   			fclose(fp);
		}

		printf("\nSuccess!\n");
		
		free(data);
	}

	end = clock();
	cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
	printf("Needed %f Seconds / %f Minutes / %f Hours\n\n", cpu_time_used, cpu_time_used / 60, cpu_time_used / 3600);
	
	Beep(540,200);
	
	return(0);
}

DWORD WINAPI calculate_segment(LPVOID lpParam) {
	uint32_t *data = (uint32_t*)lpParam;
	
	int thread_nr = 0;
	for(int i = 0; i < THREADS; i++) {  	// Extremely intelligent method to assign the tread_nr
		if(*(thread_table + i) == 0) {
			*(thread_table + i) += 1;
			thread_nr = i;
			break;
		}
	}
	calculate_set(data, thread_nr, THREADS);
	
	*(thread_table + thread_nr) += 1; 		// marking as finished
	
	return(0);
}

void calculate_set(uint32_t *data, long thread_nr, long threads) { // thread_nr 0 ... n - 1, threads n
	double bX = 0.0;
	double bY = 0.0;
	double mX = 0.0;
	double mY = 0.0;
	
	long tiefe = 0;
	double _Complex c;

	
	for (long xw = width / threads * thread_nr; xw < width / threads * (thread_nr  + 1); xw++) {	// Splitting the picture in |THREADS| columns
		for (long yh = 0; yh <= height; yh++) {
			image_coordinates_to_math_coordinates(&xw, &yh, &mX, &mY);
			
			c = mX + I * mY;			
			
			tiefe = recursion(c, 0, 0);

			draw_color(data, tiefe, &bX, &bY, &mX, &mY);
		}
	}
}

long recursion(double _Complex c, double _Complex z, long tiefe) {
	if (tiefe > max_iterations) {
		return(tiefe - 1);
	}
	if (cabs(z) >= 2.0) {
		return(tiefe);
	}
	
	z = z * z + c;
	tiefe++;
	
	return(recursion(c, z, tiefe));
}

void draw_color(uint32_t *data, long tiefe, double *bX, double *bY, double *mX, double *mY) {	// Write pixel_data to data
	math_coordinates_to_image_cooridnates(bX, bY, mX, mY);

	if (save_to_image) {	
		uint32_t r, g, b, h_value, s_value, v_value;
		h_value = map_value(tiefe%30, 0, 30, 0.0, 359.0); 
		//h_value = tiefe%359; 
		//h_value = map_value(pow(log(tiefe),3), 0, pow(log(N_MAX),3), 0.0, 359.0);
		//h_value = map_value(tiefe, 0.0, N_MAX/tiefe, 0.0, 359.0);
		//s_value = map_value(tiefe%100, 0, 100, 0.0, 100.0); 
		s_value = 100;
		v_value = 100;//map_value(tiefe%100, 0, 100, 0.0, 100.0);

		HSV_to_RGB(&r, &g, &b, h_value, s_value, v_value);
	
		*(data + to_pos((long)round(*bX), (long)round(*bY))) = combine_color(r, g, b);
	} else {
		*(data + to_pos((long)round(*bX), (long)round(*bY))) = tiefe;
	}
}

void image_coordinates_to_math_coordinates(long *bX, long *bY, double *mX, double *mY) {
	*mX = x_min + ((*bX * (x_max - x_min)) / (width));
	*mY = y_min + ((*bY * (y_max - y_min)) / (height));
}

void math_coordinates_to_image_cooridnates(double *bX, double *bY, double *mX, double *mY) {
	*mY = map_value(*mY, y_min, y_max, y_max, y_min); 
	*bX = ((*mX - x_min) * (width)) / (x_max - x_min);
	*bY = ((*mY - y_min) * (height-1)) / (y_max - y_min);
}

long to_pos(long x, long y) {
	return((y * width) + x);
}

void calculate_image_position(double zoom) {
	x_min = map_value(middle_x - 10000.0/zoom, -100.0, 100.0, x_min_base, x_max_base);
	x_max = map_value(middle_x + 10000.0/zoom, -100.0, 100.0, x_min_base, x_max_base);
	y_min = map_value(middle_y + 10000.0/zoom, -100.0, 100.0, y_min_base, y_max_base);
	y_max = map_value(middle_y - 10000.0/zoom, -100.0, 100.0, y_min_base, y_max_base);
}
