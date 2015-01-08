#include "SimpleMOC-kernel_header.h"

// Gets I from user and sets defaults
Input set_default_input( void )
{
	Input I;

	I.source_regions = 2250;
	I.course_axial_intervals = 9;
	I.fine_axial_intervals = 5;
	I.segments = 50000000;
	I.egroups = 100;

	#ifdef PAPI
	I.papi_event_set = 0;
	#endif

	#ifdef OPENMP
	I.nthreads = omp_get_max_threads();
	#endif

	return I;
}

Source * initialize_sources( Input I, Source_Arrays * SA )
{
	// Source Data Structure Allocation
	Source * sources = (Source *) malloc( I.source_regions * sizeof(Source));
	
	// Allocate Fine Source Data
	long N_fine = I.source_regions * I.fine_axial_intervals * I.egroups;
	SA->fine_source_arr = (float *) malloc( N_fine * sizeof(float));
	for( int i = 0; i < I.source_regions; i++ )
		sources[i].fine_source_id = i*I.fine_axial_intervals*I.egroups;

	// Allocate Fine Flux Data
	SA->fine_flux_arr = (float *) malloc( N_fine * sizeof(float));
	for( int i = 0; i < I.source_regions; i++ )
		sources[i].fine_flux_id = i*I.fine_axial_intervals*I.egroups;

	// Allocate SigT Data
	long N_sigT = I.source_regions * I.egroups;
	SA->sigT_arr = (float *) malloc( N_sigT * sizeof(float));
	for( int i = 0; i < I.source_regions; i++ )
		sources[i].sigT_id = i * I.egroups;

	// Allocate Locks
	#ifdef OPENMP
	SA->locks_arr = init_locks(I);
	for( int i = 0; i < I.source_regions; i++)
		sources[i].locks_id = i * I.course_axial_intervals;
	#endif
	
	// Initialize fine source and flux to random numbers
	for( long i = 0; i < N_fine; i++ )
	{
		SA->fine_source_arr[i] = rand() / RAND_MAX;
		SA->fine_flux_arr[i] = rand() / RAND_MAX;
	}

	// Initialize SigT Values
	for( int i = 0; i < N_sigT; i++ )
		SA->sigT_arr[i] = rand() / RAND_MAX;

	return sources;
}

// Builds a table of exponential values for linear interpolation
Table buildExponentialTable( void )
{
	// define table
	Table table;

	//float precision = 0.01;
	float maxVal = 10.0;	

	// compute number of arry values
	//int N = (int) ( maxVal * sqrt(1.0 / ( 8.0 * precision * 0.01 ) ) );
	int N = 353; 

	// compute spacing
	float dx = maxVal / (float) N;

	// store linear segment information (slope and y-intercept)
	for( int n = 0; n < N; n++ )
	{
		// compute slope and y-intercept for ( 1 - exp(-x) )
		float exponential = exp( - n * dx );
		table.values[ 2*n ] = - exponential;
		table.values[ 2*n + 1 ] = 1 + ( n * dx - 1 ) * exponential;
	}

	// assign data to table
	table.dx = dx;
	table.maxVal = maxVal - table.dx;
	table.N = N;

	return table;
}

#ifdef INTEL
SIMD_Vectors aligned_allocate_simd_vectors(Input I)
{
	SIMD_Vectors A;
	A.q0 = (float *) _mm_malloc(I.egroups * sizeof(float), 64);
	A.q1 = (float *) _mm_malloc(I.egroups * sizeof(float), 64);
	A.q2 = (float *) _mm_malloc(I.egroups * sizeof(float), 64);
	A.sigT = (float *) _mm_malloc(I.egroups * sizeof(float), 64);
	A.tau = (float *) _mm_malloc(I.egroups * sizeof(float), 64);
	A.sigT2 =(float *) _mm_malloc(I.egroups * sizeof(float), 64);
	A.expVal =(float *) _mm_malloc(I.egroups * sizeof(float), 64);
	A.reuse = (float *) _mm_malloc(I.egroups * sizeof(float), 64);
	A.flux_integral = (float *) _mm_malloc(I.egroups * sizeof(float), 64);
	A.tally = (float *) _mm_malloc(I.egroups * sizeof(float), 64);
	A.t1 = (float *) _mm_malloc(I.egroups * sizeof(float), 64);
	A.t2 = (float *) _mm_malloc(I.egroups * sizeof(float), 64);
	A.t3 = (float *) _mm_malloc(I.egroups * sizeof(float), 64);
	A.t4 = (float *) _mm_malloc(I.egroups * sizeof(float), 64);
	return A;
}
#endif

SIMD_Vectors allocate_simd_vectors(Input I)
{
	SIMD_Vectors A;
	float * ptr = (float * ) malloc( I.egroups * 14 * sizeof(float));
	A.q0 = ptr;
	ptr += I.egroups;
	A.q1 = ptr;
	ptr += I.egroups;
	A.q2 = ptr;
	ptr += I.egroups;
	A.sigT = ptr;
	ptr += I.egroups;
	A.tau = ptr;
	ptr += I.egroups;
	A.sigT2 = ptr;
	ptr += I.egroups;
	A.expVal = ptr;
	ptr += I.egroups;
	A.reuse = ptr;
	ptr += I.egroups;
	A.flux_integral = ptr;
	ptr += I.egroups;
	A.tally = ptr;
	ptr += I.egroups;
	A.t1 = ptr;
	ptr += I.egroups;
	A.t2 = ptr;
	ptr += I.egroups;
	A.t3 = ptr;
	ptr += I.egroups;
	A.t4 = ptr;

	return A;
}

#ifdef OPENMP
// Intialized OpenMP Source Region Locks
omp_lock_t * init_locks( Input I )
{
	// Allocate locks array
	long n_locks = I.source_regions * I.course_axial_intervals; 
	omp_lock_t * locks = (omp_lock_t *) malloc( n_locks* sizeof(omp_lock_t));

	// Initialize locks array
	for( long i = 0; i < n_locks; i++ )
		omp_init_lock(&locks[i]);

	return locks;
}	
#endif

// Timer function. Depends on if compiled with MPI, openmp, or vanilla
double get_time(void)
{
    #ifdef OPENMP
    return omp_get_wtime();
    #endif

    time_t time;
    time = clock();

    return (double) time / (double) CLOCKS_PER_SEC;
}
