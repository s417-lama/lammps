/* ----------------------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#include "verlet.h"
#include <cstring>
#include "neighbor.h"
#include "domain.h"
#include "comm.h"
#include "atom.h"
#include "atom_vec.h"
#include "force.h"
#include "pair.h"
#include "bond.h"
#include "angle.h"
#include "dihedral.h"
#include "improper.h"
#include "kspace.h"
#include "output.h"
#include "update.h"
#include "modify.h"
#include "timer.h"
#include "error.h"

#include "threads.h"

#ifndef ENABLE_ANALYSIS
# define ENABLE_ANALYSIS 1
#endif

#ifndef ANALYSIS_ASYNC
# define ANALYSIS_ASYNC 1
#endif

using namespace LAMMPS_NS;

/* ---------------------------------------------------------------------- */

Verlet::Verlet(LAMMPS *lmp, int narg, char **arg) :
  Integrate(lmp, narg, arg) {}

/* ----------------------------------------------------------------------
   initialization before run
------------------------------------------------------------------------- */

void Verlet::init()
{
  Integrate::init();

  // warn if no fixes

  if (modify->nfix == 0 && comm->me == 0)
    error->warning(FLERR,"No fixes defined, atoms won't move");

  // virial_style:
  // 1 if computed explicitly by pair->compute via sum over pair interactions
  // 2 if computed implicitly by pair->virial_fdotr_compute via sum over ghosts

  if (force->newton_pair) virial_style = 2;
  else virial_style = 1;

  // setup lists of computes for global and per-atom PE and pressure

  ev_setup();

  // detect if fix omp is present for clearing force arrays

  int ifix = modify->find_fix("package_omp");
  if (ifix >= 0) external_force_clear = 1;

  // set flags for arrays to clear in force_clear()

  torqueflag = extraflag = 0;
  if (atom->torque_flag) torqueflag = 1;
  if (atom->avec->forceclearflag) extraflag = 1;

  // orthogonal vs triclinic simulation box

  triclinic = domain->triclinic;
}

/* ----------------------------------------------------------------------
   setup before run
------------------------------------------------------------------------- */

void Verlet::setup(int flag)
{
  if (comm->me == 0 && screen) {
    fprintf(screen,"Setting up Verlet run ...\n");
    if (flag) {
      fprintf(screen,"  Unit style    : %s\n",update->unit_style);
      fprintf(screen,"  Current step  : " BIGINT_FORMAT "\n",update->ntimestep);
      fprintf(screen,"  Time step     : %g\n",update->dt);
      timer->print_timeout(screen);
    }
  }

  if (lmp->kokkos)
    error->all(FLERR,"KOKKOS package requires run_style verlet/kk");

  update->setupflag = 1;

  // setup domain, communication and neighboring
  // acquire ghosts
  // build neighbor lists

  atom->setup();
  modify->setup_pre_exchange();
  if (triclinic) domain->x2lamda(atom->nlocal);
  domain->pbc();
  domain->reset_box();
  comm->setup();
  if (neighbor->style) neighbor->setup_bins();
  comm->exchange();
  if (atom->sortfreq > 0) atom->sort();
  comm->borders();
  if (triclinic) domain->lamda2x(atom->nlocal+atom->nghost);
  domain->image_check();
  domain->box_too_small_check();
  modify->setup_pre_neighbor();
  neighbor->build(1);
  modify->setup_post_neighbor();
  neighbor->ncalls = 0;

  // compute all forces

  force->setup();
  ev_set(update->ntimestep);
  force_clear();
  modify->setup_pre_force(vflag);

  if (pair_compute_flag) force->pair->compute(eflag,vflag);
  else if (force->pair) force->pair->compute_dummy(eflag,vflag);

  if (atom->molecular) {
    if (force->bond) force->bond->compute(eflag,vflag);
    if (force->angle) force->angle->compute(eflag,vflag);
    if (force->dihedral) force->dihedral->compute(eflag,vflag);
    if (force->improper) force->improper->compute(eflag,vflag);
  }

  if (force->kspace) {
    force->kspace->setup();
    if (kspace_compute_flag) force->kspace->compute(eflag,vflag);
    else force->kspace->compute_dummy(eflag,vflag);
  }

  modify->setup_pre_reverse(eflag,vflag);
  if (force->newton) comm->reverse_comm();

  modify->setup(vflag);
  output->setup(flag);
  update->setupflag = 0;
}

/* ----------------------------------------------------------------------
   setup without output
   flag = 0 = just force calculation
   flag = 1 = reneighbor and force calculation
------------------------------------------------------------------------- */

void Verlet::setup_minimal(int flag)
{
  update->setupflag = 1;

  // setup domain, communication and neighboring
  // acquire ghosts
  // build neighbor lists

  if (flag) {
    modify->setup_pre_exchange();
    if (triclinic) domain->x2lamda(atom->nlocal);
    domain->pbc();
    domain->reset_box();
    comm->setup();
    if (neighbor->style) neighbor->setup_bins();
    comm->exchange();
    comm->borders();
    if (triclinic) domain->lamda2x(atom->nlocal+atom->nghost);
    domain->image_check();
    domain->box_too_small_check();
    modify->setup_pre_neighbor();
    neighbor->build(1);
    modify->setup_post_neighbor();
    neighbor->ncalls = 0;
  }

  // compute all forces

  ev_set(update->ntimestep);
  force_clear();
  modify->setup_pre_force(vflag);

  if (pair_compute_flag) force->pair->compute(eflag,vflag);
  else if (force->pair) force->pair->compute_dummy(eflag,vflag);

  if (atom->molecular) {
    if (force->bond) force->bond->compute(eflag,vflag);
    if (force->angle) force->angle->compute(eflag,vflag);
    if (force->dihedral) force->dihedral->compute(eflag,vflag);
    if (force->improper) force->improper->compute(eflag,vflag);
  }

  if (force->kspace) {
    force->kspace->setup();
    if (kspace_compute_flag) force->kspace->compute(eflag,vflag);
    else force->kspace->compute_dummy(eflag,vflag);
  }

  modify->setup_pre_reverse(eflag,vflag);
  if (force->newton) comm->reverse_comm();

  modify->setup(vflag);
  update->setupflag = 0;
}

/* ----------------------------------------------------------------------
   run for N steps
------------------------------------------------------------------------- */

void Verlet::run(int n)
{
  bigint ntimestep;
  int nflag,sortflag;

  int n_post_integrate = modify->n_post_integrate;
  int n_pre_exchange = modify->n_pre_exchange;
  int n_pre_neighbor = modify->n_pre_neighbor;
  int n_post_neighbor = modify->n_post_neighbor;
  int n_pre_force = modify->n_pre_force;
  int n_pre_reverse = modify->n_pre_reverse;
  int n_post_force = modify->n_post_force;
  int n_end_of_step = modify->n_end_of_step;

  if (atom->sortfreq > 0) sortflag = 1;
  else sortflag = 0;

  for (int i = 0; i < n; i++) {
    if (timer->check_timeout(i)) {
      update->nsteps = i;
      break;
    }

    ntimestep = ++update->ntimestep;
    ev_set(ntimestep);

    // initial time integration

    timer->stamp();
    modify->initial_integrate(vflag);
    if (n_post_integrate) modify->post_integrate();
    timer->stamp(Timer::MODIFY);

    // regular communication vs neighbor list rebuild

    nflag = neighbor->decide();

    if (nflag == 0) {
      timer->stamp();
      comm->forward_comm();
      timer->stamp(Timer::COMM);
    } else {
      analysis_wait();

      if (n_pre_exchange) {
        timer->stamp();
        modify->pre_exchange();
        timer->stamp(Timer::MODIFY);
      }
      if (triclinic) domain->x2lamda(atom->nlocal);
      domain->pbc();
      if (domain->box_change) {
        domain->reset_box();
        comm->setup();
        if (neighbor->style) neighbor->setup_bins();
      }
      timer->stamp();
      comm->exchange();
      if (sortflag && ntimestep >= atom->nextsort) atom->sort();
      comm->borders();
      if (triclinic) domain->lamda2x(atom->nlocal+atom->nghost);
      timer->stamp(Timer::COMM);
      if (n_pre_neighbor) {
        modify->pre_neighbor();
        timer->stamp(Timer::MODIFY);
      }
      neighbor->build(1);
      timer->stamp(Timer::NEIGH);
      if (n_post_neighbor) {
        modify->post_neighbor();
        timer->stamp(Timer::MODIFY);
      }
    }

    analysis_wait();
#if ENABLE_ANALYSIS
    analysis(force->pair->list);
#endif
#if !ANALYSIS_ASYNC
    analysis_wait();
#endif

    // force computations
    // important for pair to come before bonded contributions
    // since some bonded potentials tally pairwise energy/virial
    // and Pair:ev_tally() needs to be called before any tallying

    force_clear();

    timer->stamp();

    if (n_pre_force) {
      modify->pre_force(vflag);
      timer->stamp(Timer::MODIFY);
    }

    if (pair_compute_flag) {
      force->pair->compute(eflag,vflag);
      timer->stamp(Timer::PAIR);
    }

    if (atom->molecular) {
      if (force->bond) force->bond->compute(eflag,vflag);
      if (force->angle) force->angle->compute(eflag,vflag);
      if (force->dihedral) force->dihedral->compute(eflag,vflag);
      if (force->improper) force->improper->compute(eflag,vflag);
      timer->stamp(Timer::BOND);
    }

    if (kspace_compute_flag) {
      force->kspace->compute(eflag,vflag);
      timer->stamp(Timer::KSPACE);
    }

    if (n_pre_reverse) {
      modify->pre_reverse(eflag,vflag);
      timer->stamp(Timer::MODIFY);
    }

    // reverse communication of forces

    if (force->newton) {
      comm->reverse_comm();
      timer->stamp(Timer::COMM);
    }

    // force modifications, final time integration, diagnostics

    if (n_post_force) modify->post_force(vflag);
    modify->final_integrate();
    if (n_end_of_step) modify->end_of_step();
    timer->stamp(Timer::MODIFY);

    // all output

    if (ntimestep == output->next) {
      timer->stamp();
      output->write(ntimestep);
      timer->stamp(Timer::OUTPUT);
    }
  }

  analysis_wait();
}

/* ---------------------------------------------------------------------- */

void Verlet::cleanup()
{
  modify->post_run();
  domain->box_too_small_check();
  update->update_time();
}

/* ----------------------------------------------------------------------
   clear force on own & ghost atoms
   clear other arrays as needed
------------------------------------------------------------------------- */

void Verlet::force_clear()
{
  size_t nbytes;

  if (external_force_clear) return;

  // clear force on all particles
  // if either newton flag is set, also include ghosts
  // when using threads always clear all forces.

  int nlocal = atom->nlocal;

  if (neighbor->includegroup == 0) {
    nbytes = sizeof(double) * nlocal;
    if (force->newton) nbytes += sizeof(double) * atom->nghost;

    if (nbytes) {
      memset(&atom->f[0][0],0,3*nbytes);
      if (torqueflag) memset(&atom->torque[0][0],0,3*nbytes);
      if (extraflag) atom->avec->force_clear(0,nbytes);
    }

  // neighbor includegroup flag is set
  // clear force only on initial nfirst particles
  // if either newton flag is set, also include ghosts

  } else {
    nbytes = sizeof(double) * atom->nfirst;

    if (nbytes) {
      memset(&atom->f[0][0],0,3*nbytes);
      if (torqueflag) memset(&atom->torque[0][0],0,3*nbytes);
      if (extraflag) atom->avec->force_clear(0,nbytes);
    }

    if (force->newton) {
      nbytes = sizeof(double) * atom->nghost;

      if (nbytes) {
        memset(&atom->f[nlocal][0],0,3*nbytes);
        if (torqueflag) memset(&atom->torque[nlocal][0],0,3*nbytes);
        if (extraflag) atom->avec->force_clear(nlocal,nbytes);
      }
    }
  }
}

/* ----------------------------------------------------------------------
   Analysis task
------------------------------------------------------------------------- */

static inline int _isBonded(double dist, double currR1, double currR2)
{
  if ((dist >= currR1) && (dist <= currR2))
    return 1;

  return 0;
}

typedef struct { long val; } __attribute((aligned(64))) counter_t;
typedef struct { double x,y,z; } dbl3_t;

static constexpr int nthreads = 28;

static dbl3_t * _noalias x_ = NULL;
static ABT_thread threads[nthreads];
static task* ts[nthreads];
static counter_t alignas(64) bond_counts[nthreads];
static int started = 0;

void Verlet::analysis(NeighList *list)
{
  dbl3_t * _noalias const x = (dbl3_t *) atom->x[0];
  const int * _noalias const ilist = list->ilist;
  const int * _noalias const numneigh = list->numneigh;
  const int * const * const firstneigh = list->firstneigh;
  const long nlocal = atom->nlocal;
  const double DEFAULT_R1 = 0.95;
  const double DEFAULT_R2 = 1.3;
  int ret;

  if (x_ == NULL) {
    x_ = (dbl3_t *)malloc(sizeof(dbl3_t) * nlocal);
  } else {
    x_ = (dbl3_t *)realloc(x_, sizeof(dbl3_t) * nlocal);
  }

#if defined (_OPENMP)
  int i;
#pragma omp parallel for default(none) schedule(static) shared(x_, x)
  for (i = 0; i < nlocal; i++)
#else
  parallel_for("copy", 0, nlocal, [&](int i) {
#endif
    x_[i].x = x[i].x;
    x_[i].y = x[i].y;
    x_[i].z = x[i].z;
#if !defined (_OPENMP)
  });
#endif

  int nb = (nlocal + nthreads - 1) / nthreads;

  for (int tid = 0; tid < nthreads; tid++) {
    auto thread_fn = [=]() {
      int ifrom = tid * nb;
      int ito = ((tid + 1) * nb > nlocal) ? nlocal : (tid + 1) * nb;

      bond_counts[tid].val = 0;
      /* for (int a = 0; a < 10; a++) */
      for (int ii = ifrom; ii < ito; ii++) {
        const int i = ilist[ii];
        const int * _noalias const jlist = firstneigh[i];
        const int jnum = numneigh[i];
        for (int jj = 0; jj < jnum; jj++) {
          int j = jlist[jj];
          j &= NEIGHMASK;
          if (j < nlocal) {
            double dx = x_[i].x - x_[j].x;
            double dy = x_[i].y - x_[j].y;
            double dz = x_[i].z - x_[j].z;

            double dist = sqrt(dx*dx + dy*dy + dz*dz);

            if (_isBonded(dist, DEFAULT_R1, DEFAULT_R2)) {
              bond_counts[tid].val++;
            }
          }
        }
      }
    };

    ts[tid] = new callable_task<decltype(thread_fn)>(thread_fn);
    ABT_thread_attr attr;
    ABT_thread_attr_create(&attr);
    ABT_thread_attr_set_preemption_type(attr, ABT_PREEMPTION_YIELD);
    ret = ABT_thread_create(g_analysis_pool,
                            invoke,
                            ts[tid],
                            attr,
                            &threads[tid]);
    HANDLE_ERROR(ret, "ABT_thread_create");
  }

  started = 1;
}

void Verlet::analysis_wait()
{
  int ret;
  if (started) {
    long bond_count = 0;
    for (int tid = 0; tid < nthreads; tid++) {
      ret = ABT_thread_free(&threads[tid]);
      HANDLE_ERROR(ret, "ABT_thread_free");
      delete ts[tid];
      bond_count += bond_counts[tid].val;
    }
    printf("bond: %ld\n", bond_count);

    started = 0;
  }
}
