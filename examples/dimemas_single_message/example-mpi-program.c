/* example-mpi-program.c
 * Patrick MacArthur <pmacarth@iol.unh.edu>
 * A simple MPI demo.
 */

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <mpi.h>

#define BUFSIZE 1025
char buf[BUFSIZE];

int
main(int argc, char *argv[])
{
	MPI_Status status;
	int rc, numtasks, rank;

	if ((rc = MPI_Init(&argc, &argv)) != MPI_SUCCESS) {
		fprintf(stderr, "MPI_Init failed!  Giving up...\n");
		MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
	}

	MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	if (numtasks != 2) {
		fprintf(stderr, "This program requires exactly 2 processes\n");
		MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
	}

	if (rank == 0) {
		snprintf(buf, BUFSIZE, "Hello, world!\n");
		MPI_Send(buf, BUFSIZE, MPI_CHAR, 1, 0, MPI_COMM_WORLD);
	} else {
		MPI_Recv(buf, BUFSIZE, MPI_CHAR, 0, 0, MPI_COMM_WORLD, &status);
	}

	MPI_Finalize();

	return EXIT_SUCCESS;
}

/* vim: set shiftwidth=8 tabstop=8 noexpandtab : */
