#define _GNU_SOURCE

#include <string.h>
#include <strings.h>
#include <stdio.h>

#ifndef __LAYOUT_H
#define __LAYOUT_H

typedef enum layout_policy
{
    LAYOUT_POLICY_NONE = 0,
    LAYOUT_POLICY_RR,
    LAYOUT_POLICY_SPREAD,
} layout_policy_t;

typedef enum rmaps_explicit_mode
{
    RMAPS_EXPLICIT_MODE_NONE = 0,
    RMAPS_EXPLICIT_MODE_MANUAL,
    RMAPS_EXPLICIT_MODE_AUTO,
} rmaps_explicit_mode_t;

typedef struct rmaps_explicit_layout {
    rmaps_explicit_mode_t mode;

    /* Specific to the auto mode */
    int scope;
    layout_policy_t policy;
    int n_per_scope;
    int n_pes;

    /* Specific to the manual mode */
    char *target;
    char *places;
    uint64_t n_places;
    uint8_t *locations;
} rmaps_explicit_layout_t;

#define MAX_LOCATIONS   (1024)
static inline int
_parse_places (char *places, uint64_t *num, uint8_t **locations)
{
    char *s;
    char *token;
    uint64_t idx = 0;
    uint64_t cnt = 0;
    const char delimiters[] = ",";
    uint8_t *_l;
    int target_tid = 0;

    if (locations == NULL || *locations == NULL)
    {
        uint8_t *locs = malloc (MAX_LOCATIONS * sizeof (uint8_t));
        if (locs == NULL)
            return LAYOUT_ERR_OUT_OF_RESOURCE;

        *locations = locs;
    }
    _l = *locations;

    /*
     * The places string looks like X:-:-:X, where "X" is a place where
     * thread X should be placed and "-" a place that should be left alone
     */

    s = strdupa (places);
    token = strtok (s, delimiters);

    do
    {
        if (strcmp (token, "-") != 0)
        {
            fprintf (stderr, "[%s:%s:%d] Assigning OMP thread # %d to place # %d\n",
                     __FILE__, __func__, __LINE__, target_tid, idx);
            _l[target_tid] = idx;
            cnt++;
            target_tid ++;
        }
        idx++;
        token = strtok (NULL, delimiters);
    } while (token != NULL);

    *num = cnt;
    fprintf (stderr, "[%s:%s:%d] Done\n", __FILE__, __func__, __LINE__);

    return LAYOUT_SUCCESS;
}

 /*
  *
  */
static inline int
_parse_omp_layout_desc (char *layout_desc, int parent_mpi_rank, rmaps_explicit_layout_t *layout)
{
    int rc;
    char *s;
    char *token;
    const char delimiters[] = "[,]";
    const char block_delimiters[] = "[]";

    s = strdupa (layout_desc);
    token = strtok (s, delimiters);

    while (token != NULL)
    {
        if ((strcasecmp (token, "OpenMP") == 0) ||
            (strcasecmp (token, "openmp") == 0)   )
        {
            int _rank;

            fprintf (stderr, "[%s:%s:%d] Found OpenMP block\n",
                     __FILE__, __func__, __LINE__);

            token = strtok (NULL, delimiters);
            _rank = atoi (token);

            fprintf (stderr, "[%s:%s:%d] Block for MPI rank: %d\n",
                     __FILE__, __func__, __LINE__, _rank);

            if (_rank == parent_mpi_rank)
            {
                token = strtok (NULL, delimiters);
                (*layout).target = strdup (token);

                token = strtok (NULL, block_delimiters);
                (*layout).places = strdup (token);

                fprintf (stderr, "[%s:%s:%d] Layout places: %s\n",
                         __FILE__, __func__, __LINE__, (*layout).places);

                (*layout).locations = NULL; /* We need to make sure the ptr is
                                             * set to NULL to have the expected
                                             * behavior */

                rc = _parse_places (token,
                                    &((*layout).n_places),
                                    &((*layout).locations));
                if (rc != LAYOUT_SUCCESS)
                    return LAYOUT_ERROR;
            }
        }
        token = strtok (NULL, delimiters);
    }

    return LAYOUT_SUCCESS;
}

#endif /* __LAYOUT_H */
