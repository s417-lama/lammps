/* -*- c++ -*- ----------------------------------------------------------
   LAMMPS - Large-scale Atomic/Molecular Massively Parallel Simulator
   http://lammps.sandia.gov, Sandia National Laboratories
   Steve Plimpton, sjplimp@sandia.gov

   Copyright (2003) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level LAMMPS directory.
------------------------------------------------------------------------- */

#ifdef INTEGRATE_CLASS

IntegrateStyle(verlet/kk,VerletKokkos)
IntegrateStyle(verlet/kk/device,VerletKokkos)
IntegrateStyle(verlet/kk/host,VerletKokkos)

#else

#ifndef LMP_VERLET_KOKKOS_H
#define LMP_VERLET_KOKKOS_H

#include "verlet.h"
#include "kokkos_type.h"
#include "atom_kokkos.h"
#include "neigh_list_kokkos.h"

namespace LAMMPS_NS {

class VerletKokkos : public Verlet {
 public:
  VerletKokkos(class LAMMPS *, int, char **);
  ~VerletKokkos() {}
  void setup(int);
  void setup_minimal(int);
  void run(int);

  KOKKOS_INLINE_FUNCTION
  void operator() (const int& i) const {
    f(i,0) += f_merge_copy(i,0);
    f(i,1) += f_merge_copy(i,1);
    f(i,2) += f_merge_copy(i,2);
  }


 protected:
  DAT::t_f_array f_merge_copy,f;

  void force_clear();

  template<class DeviceType>
  void analysis(NeighListKokkos<DeviceType>* list)
  {
    const double BOND_R1 = 0.95;
    const double BOND_R2 = 1.3;

    typename ArrayTypes<DeviceType>::t_x_array x = atomKK->k_x.view<DeviceType>();
    int nlocal = atomKK->nlocal;

    long bond_count = 0;

    for (int i = 0; i < nlocal; i++) {
      const AtomNeighborsConst neighbors_i = list->get_neighbors_const(i);
      const int jnum = list->d_numneigh[i];
      for (int jj = 0; jj < jnum; jj++) {
        int j = neighbors_i(jj);
        X_FLOAT dx = x(i,0) - x(j,0);
        X_FLOAT dy = x(i,1) - x(j,1);
        X_FLOAT dz = x(i,2) - x(j,2);
        X_FLOAT dist = sqrt(dx*dx + dy*dy + dz*dz);
        if ((dist >= BOND_R1) && (dist <= BOND_R2)) {
          bond_count++;
        }
      }
    }

    printf("bond: %ld\n", bond_count);
  }
};

}

#endif
#endif

/* ERROR/WARNING messages:

*/
