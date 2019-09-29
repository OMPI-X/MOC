/*
 * Copyright(c) 2017-2018   UT-Battelle, LLC
 *                          All rights reserved.
 */

#define _GNU_SOURCE

#include <pmix.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "moc.h"

uint32_t        node_local_procs    = 0;
pmix_proc_t     myproc;

moc_status_t
MOC_Init (MPI_Comm comm)
{
    int             i, rc;
    char            *s_procNames        = NULL;
    char            *r_procNames        = NULL;
//    uint32_t        node_local_procs    = 0;
    pmix_pdata_t    *lookup_pdata;
    int             dummy               = 0;
    int             lookup_range        = PMIX_RANGE_GLOBAL;
    pmix_value_t    *val;
    pmix_proc_t     proc;
    int             numCPU;
    pmix_value_t    value;
    pmix_value_t    *_val_local_peers;
    rmaps_explicit_layout_t layout;
    int _local_rank_id = -1;

    PMIx_Init (&myproc, NULL, 0);

    /* First we try to see if the info is already available */
    PMIX_PDATA_CREATE (lookup_pdata, 1);
    (void)strncpy(lookup_pdata[0].key, PMIX_LOCAL_SIZE, PMIX_MAX_KEYLEN);
    //(void)strncpy(lookup_pdata[0].key, PMIX_HWLOC_SHMEM_ADDR, PMIX_MAX_KEYLEN);

    rc = PMIx_Lookup (lookup_pdata, 1, NULL, 0);
    if (rc != PMIX_SUCCESS)
        fprintf (stderr, "[%s:%s:%d:%d] PMIx_Lookup returned %s\n", __FILE__, __func__, __LINE__, getpid(), PMIx_Error_string(rc));
    else
        fprintf (stderr, "[%s:%s:%d] PMIx_Lookup succeeded\n", __FILE__, __func__, __LINE__);
    //rc = PMIX_ERROR;
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

        PMIX_VAL_SET (&value, uint32_t, node_local_procs);
        rc = PMIx_Put (PMIX_GLOBAL, PMIX_LOCAL_SIZE, &value);
        if (rc != PMIX_SUCCESS)
        {
            fprintf (stderr, "[%s:%s:%d] ERROR: PMIx_Put() failed\n", __FILE__, __func__, __LINE__);
        }

        PMIx_Commit ();
        PMIx_Fence (&proc, 1, NULL, 0);
        PMIX_PDATA_FREE (lookup_pdata, 1);

        MPI_Comm_free (&shmem_comm);
    }

    rc = PMIx_Get (&proc, PMIX_LOCAL_PEERS, NULL, 0, &_val_local_peers);
    if (rc != PMIX_SUCCESS)
    {
        fprintf (stderr, "[%s:%s:%d:%d] ERROR: PMIx_Get() failed (%d)\n", __FILE__, __func__, __LINE__, getpid(), rc);
    }
    else
    {
        if (_val_local_peers->type != PMIX_STRING)
        {
            fprintf (stderr, "[%s:%s:%d] ERROR: Incorrect type\n", __FILE__, __func__, __LINE__);
        }
        else
        {
            char *_list_ranks;
            char *_tok;
            int _my_rank;
            int _n = 0;

            MPI_Comm_rank (MPI_COMM_WORLD, &_my_rank);

            _list_ranks = strdup (_val_local_peers->data.string);
            fprintf (stderr, "[%s:%s:%d] YOUPI - local peers: %s\n", __FILE__, __func__, __LINE__, _list_ranks);
            _tok = strtok (_list_ranks, ",");
            while (_tok != NULL)
            {
                if (atoi (_tok) == _my_rank)
                {
                    _local_rank_id = _n;
                    fprintf (stderr, "[%s:%s:%d] YOUPI - on this node, i am rank # %d\n", __FILE__, __func__, __LINE__, _local_rank_id);
                }
                _n ++;
                _tok = strtok (NULL, ",");
            }
        }
    }

    char *layout_env = getenv ("OMPI_MCA_rmaps_explicit_layout");
    if (layout_env != NULL && _local_rank_id >= 0)
    {
        char *places = NULL;
        fprintf (stderr, "%d: YOUPI!! I have a layout! %s\n", getpid(), layout_env);

        rc = _parse_omp_layout_desc (layout_env, _local_rank_id, &layout);

        if (rc == LAYOUT_ERR_NOT_FOUND)
        {
            fprintf (stderr, "[%s:%s:%d] WARN: No OpenMP layout provided\n",
                     __FILE__, __func__, __LINE__);
            /* No OMP layout info to process, so just return */
            return MOC_SUCCESS;
        }
        else if (rc == LAYOUT_ERROR)
        {
            fprintf (stderr, "[%s:%s:%d] ERROR: Failed to parse OpenMP layout\n",
                     __FILE__, __func__, __LINE__);
            /* Early exit */
            return MOC_ERROR;
        }
        else if (NULL == layout.target)
        {
            fprintf (stderr, "[%s:%s:%d] ERROR: Failed to get target from layout\n",
                     __FILE__, __func__, __LINE__);
            /* Early exit */
            return MOC_ERROR;
        }
        else
        {
            if (strcmp (layout.target, "Core") == 0)
            {
                for (i = 0; i < layout.n_places; i++)
                {
                    if (places == NULL)
                    {
                        asprintf (&places, "{%d},", layout.locations[i]);
                    }
                    else
                    {
                        char *new_place = NULL;
                        asprintf (&new_place, "{%d},", layout.locations[i]);
                        strcat (places, new_place);
                    }
                }
            }
            if ((strcmp (layout.target, "PU") == 0)||
                (strcmp (layout.target, "HT") == 0) )
            {
                char *ht_per_core_env = getenv ("MOC_HT_PER_CORE");
                if (ht_per_core_env == NULL)
                {
                    fprintf (stderr, "WARN: Missing 'MOC_HT_PER_CORE' for PU layout\n");
                }
                else
                {
                    int ht_per_core = atoi (ht_per_core_env);
                    if (layout.locations[0] % ht_per_core != 0)
                    {
                        fprintf (stderr, "ERROR: threads are not places of HT boundaries\n");
                    }
                    else
                    {
                        int first_ht, last_ht;
                        int core_num;
                        for (int j = 0; j < layout.n_places; j++)
                        {
                            int k;
                            char *new_places = NULL;
                            asprintf (&new_places, "{%d,", layout.locations[j]);
                            first_ht = last_ht = layout.locations[j];
                            for (k = 1; k < ht_per_core; k++)
                            {
                                char *detail = NULL;
                                if (layout.locations[j+k] != last_ht + 1)
                                {
                                    fprintf (stderr, "ERROR: threads are not places of HT boundaries!!\n");
                                    exit (1);
                                }
                                if (k != ht_per_core - 1)
                                {
                                    asprintf (&detail, "%d,", layout.locations[j+k]);
                                }
                                else
                                {
                                    asprintf (&detail, "%d", layout.locations[j+k]);
                                }
                                strcat (new_places, detail);
                                last_ht = layout.locations[j+k];
                            }

                            j = j + ht_per_core;
                            if (j < layout.n_places)
                            {
                                strcat (new_places, "},");
                            }
                            else
                            {
                                strcat (new_places, "}");
                            }

                            core_num = first_ht / ht_per_core;
                            fprintf (stderr, "HT %d - %d (core # %d) are used\n", first_ht, last_ht, core_num);
                            if (places == NULL)
                            {
                                places = new_places;
                            }
                            else
                            {
                                strcat (places, new_places);
                            }
    #if 0
                            if (places == NULL)
                            {
                                places = strdup ("{");
                                for (i = 0; i < layout.n_places; i++)
                                    for (i = 0; i < ht_per_core; i++)
                                    {
                                        char *details = NULL;
                                        asprintf (&details, "%d,", layout.locations[i]);
                                        strcat (&places, details);
                                    }
                                    strcat (&places, "},");
                            }
                            else
                            {
                                char *new_places = NULL;
                                new_places = strdup ("{");
                                for (i = 0; i < ht_per_core; i++)

                            }
    #endif
                        } /* for */
                    } /* else */
                } /* ht_per_core_env */
            } /* strcmp PU */

            if (NULL == places)
            {
                fprintf (stderr, "[%s:%s:%d] ERROR: OMP places info is NULL\n",
                         __FILE__, __func__, __LINE__);
                return MOC_ERROR;
            }
            else
            {

                fprintf (stderr, "OMP Places: %s\n", places);
                setenv ("OMP_PLACES", places, 1);
                fprintf (stderr, "setenv OMP_PLACES=%s\n", places);
            }

        } /* else */

    } /* layout_env */

    numCPU = sysconf (_SC_NPROCESSORS_ONLN);
    fprintf (stderr, "[%s:%s:%d] Num CPU: %d\n", __FILE__, __func__, __LINE__, numCPU);

#if 0
    {
        pmix_info_t *pubinfo;

        PMIX_INFO_CREATE (pubinfo, 1);
        (void)strncpy(pubinfo[0].key, "moc.ncpus", PMIX_MAX_KEYLEN);
        pubinfo[0].value.type = PMIX_UINT8;
        pubinfo[0].value.data.uint8 = (uint8_t)numCPU;
        PMIx_Publish (pubinfo, 1);

        PMIx_Commit ();
        PMIx_Fence (&proc, 1, NULL, 0);
    }
#endif

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
