#define _GNU_SOURCE

#include <pmix.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sched.h>
#include <errno.h>
#include "moc.h"

uint32_t        node_local_procs    = 0;
pmix_proc_t     myproc;

moc_status_t
MOC_Init (MPI_Comm comm)
{
    int             i, rc;
    char            *s_procNames        = NULL;
    char            *r_procNames        = NULL;
    pmix_pdata_t    *lookup_pdata;
    int             dummy               = 0;
    int             lookup_range        = PMIX_RANGE_GLOBAL;
    pmix_value_t    *val;
    pmix_proc_t     proc;
    int             numCPU;
    pmix_value_t    value;
    cpu_set_t       mask;
    size_t          n_cores             = 0;

    /* We get the number of local cores to make basic checks regarding the # of MPI ranks that
       are running locally (e.g., we do not support oversubscribing) */
    /* Without HWLOC, OpenMP relies on sched_getaffinity to set affinities */
    rc = sched_getaffinity (0, sizeof(mask), &mask);
    if (rc == -1)
    {
        fprintf (stderr, "ERROR: syscall() failed (%s)\n", strerror (errno));
        return EXIT_FAILURE;
    }
    for (i = 0; i < CPU_SETSIZE; ++i)
    {
        if (CPU_ISSET (i, &mask))
        {
            n_cores++;
        }
    }
    printf ("Number of cores: %d\n", (int)n_cores);

    PMIx_Init (&myproc, NULL, 0);

    /* First we try to see if the info is already available */
    PMIX_PDATA_CREATE (lookup_pdata, 1);
    (void)strncpy(lookup_pdata[0].key, PMIX_LOCAL_SIZE, PMIX_MAX_KEYLEN);

    rc = PMIx_Lookup (lookup_pdata, 1, NULL, 0);
    rc = PMIX_ERROR;
    if (rc != PMIX_SUCCESS)
    {
        MPI_Comm        shmem_comm;
        int             nprocs;

        /* if not we set it */

        /* Rely on MPI3 features to figure out the number of local ranks: split the communicator
           to get ranks that can set a shared memory between them. Not perfect but does the job
           for now. */
        MPI_Comm_split_type (MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0, MPI_INFO_NULL, &shmem_comm);
        MPI_Comm_size (shmem_comm, &nprocs);

        PMIX_PROC_CONSTRUCT(&proc);
        (void)strncpy(proc.nspace, myproc.nspace, PMIX_MAX_NSLEN);
        proc.rank = PMIX_RANK_WILDCARD;

        PMIx_Fence (&proc, 1, NULL, 0);
    
        node_local_procs = (uint32_t)nprocs;
        if (node_local_procs > n_cores)
            fprintf (stderr, "WARN: the number of ranks running on the node is greater than the number of cores. This will lead to problems when coordinating MPI and OpenMP");

        PMIX_VAL_SET (&value, uint32_t, node_local_procs);
        rc = PMIx_Put (PMIX_GLOBAL, PMIX_LOCAL_SIZE, &value);
        if (rc != PMIX_SUCCESS)
        {
            fprintf (stderr, "[%s:%s:%d] ERROR: PMIx_Put() failed\n", __FILE__, __func__, __LINE__);
        }

        PMIx_Commit ();
        PMIx_Fence (&proc, 1, NULL, 0);

        MPI_Comm_free (&shmem_comm);
    }

    PMIX_PDATA_FREE (lookup_pdata, 1);

    numCPU = sysconf (_SC_NPROCESSORS_ONLN);
    fprintf (stderr, "[%s:%s:%d] numCPUS: %d\n", __FILE__, __func__, __LINE__, numCPU);
    {
        pmix_info_t *pubinfo;

        PMIX_INFO_CREATE (pubinfo, 1);
        (void)strncpy(pubinfo[0].key, "moc.ncpus", PMIX_MAX_KEYLEN);
        pubinfo[0].value.type = PMIX_UINT8;
        pubinfo[0].value.data.uint8 = (uint8_t)numCPU;
        PMIx_Publish (pubinfo, 1);
    }

    PMIx_Commit ();
    PMIx_Fence (&proc, 1, NULL, 0);

    return MOC_SUCCESS;
}

moc_status_t
MOC_Policy_set (pmix_info_t *policy, size_t npolicy)
{
    pmix_proc_t     proc;

    PMIX_PROC_CONSTRUCT(&proc);
    (void)strncpy(proc.nspace, myproc.nspace, PMIX_MAX_NSLEN);
    proc.rank = PMIX_RANK_WILDCARD;

    PMIx_Publish (policy, npolicy);
    PMIx_Commit ();
    PMIx_Fence (&proc, 1, NULL, 0);

    return MOC_SUCCESS;
}

moc_status_t
MOC_Fini (void)
{
    PMIx_Finalize (NULL, 0);

    return MOC_SUCCESS;
}
